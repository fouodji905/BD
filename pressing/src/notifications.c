#include <stdio.h>
#include <string.h>
#include <time.h>

#include "base.h"
#include "clients.h"
#include "commandes.h"
#include "console.h"
#include "notifications.h"
#include "parametres.h"
#include "securite.h"

#define MAX_LISTE 50

/* Prototypes des fonctions internes au module. */
static void remplir_modele(const char *modele, int commande_id,
                           char *dest, size_t taille);
static int creer_notification(int commande_id, const char *type,
                              const char *modele);
static void messages_a_envoyer(void);
static void generer_rappels(void);
static void historique(void);
static void exporter(void);

/* Remplace {client} {numero} {reste} {pressing} {telephone}. */
static void remplir_modele(const char *modele, int commande_id,
                           char *dest, size_t taille)
{
    char client[80], numero[24], reste[48], valeur[128];
    const char *p = modele;
    size_t n = 0;

    req_texte(client, sizeof client,
              "SELECT client_nom FROM v_commandes WHERE id = ?", "i",
              commande_id);
    req_texte(numero, sizeof numero,
              "SELECT numero FROM commandes WHERE id = ?", "i", commande_id);
    copier(reste, fmt_montant(req_entier("SELECT total - paye FROM v_commandes"
                                         " WHERE id = ?", "i", commande_id)),
           sizeof reste);
    while (*p != '\0' && n + 1 < taille) {
        const char *fin;

        if (*p == '{' && (fin = strchr(p, '}')) != NULL) {
            size_t lg = (size_t)(fin - p - 1);

            valeur[0] = '\0';
            if (lg == 6 && strncmp(p + 1, "client", lg) == 0)
                copier(valeur, client, sizeof valeur);
            else if (lg == 6 && strncmp(p + 1, "numero", lg) == 0)
                copier(valeur, numero, sizeof valeur);
            else if (lg == 5 && strncmp(p + 1, "reste", lg) == 0)
                copier(valeur, reste, sizeof valeur);
            else if (lg == 8 && strncmp(p + 1, "pressing", lg) == 0)
                copier(valeur, config.nom, sizeof valeur);
            else if (lg == 9 && strncmp(p + 1, "telephone", lg) == 0)
                copier(valeur, config.telephone, sizeof valeur);
            else
                lg = (size_t)-1;
            if (lg != (size_t)-1) {
                size_t v = strlen(valeur);

                if (n + v >= taille)
                    v = taille - n - 1;
                memcpy(dest + n, valeur, v);
                n += v;
                p = fin + 1;
                continue;
            }
        }
        dest[n++] = *p++;
    }
    dest[n] = '\0';
}

static int creer_notification(int commande_id, const char *type,
                              const char *modele)
{
    char message[600], telephone[40];
    int client_id;

    client_id = (int)req_entier("SELECT client_id FROM commandes"
                                " WHERE id = ?", "i", commande_id);
    req_texte(telephone, sizeof telephone,
              "SELECT telephone FROM clients WHERE id = ?", "i", client_id);
    remplir_modele(modele, commande_id, message, sizeof message);
    return req_executer("INSERT INTO notifications (commande_id, client_id,"
                        " telephone, type, message) VALUES (?, ?, ?, ?, ?)",
                        "iittt", commande_id, client_id, telephone, type,
                        message);
}

void notification_commande_prete(int commande_id)
{
    if (creer_notification(commande_id, "PRET", config.modele_pret))
        printf("  Message « linge prêt » préparé pour le client"
               " (menu Notifications).\n");
}

int notifications_en_attente(void)
{
    return (int)req_entier("SELECT COUNT(*) FROM notifications"
                           " WHERE statut = 0", NULL);
}

static void messages_a_envoyer(void)
{
    sqlite3_stmt *st;
    int ids[MAX_LISTE], n, choix;

    for (;;) {
        titre("Messages à envoyer aux clients");
        printf("Envoyez chaque message par SMS ou WhatsApp, puis marquez-le "
               "comme envoyé.\n\n");
        n = 0;
        st = req_preparer("SELECT n.id, n.type, n.telephone, n.message,"
                          " substr(n.cree_le, 1, 16), cl.nom"
                          " FROM notifications n"
                          " LEFT JOIN clients cl ON cl.id = n.client_id"
                          " WHERE n.statut = 0 ORDER BY n.id LIMIT ?",
                          "i", MAX_LISTE);
        while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
            ids[n] = sqlite3_column_int(st, 0);
            printf("%2d. [%s] %s - %s - Tél : %s\n    %s\n\n", n + 1,
                   col_texte(st, 1), col_texte(st, 4), col_texte(st, 5),
                   col_texte(st, 2)[0] ? col_texte(st, 2) : "(aucun)",
                   col_texte(st, 3));
            n++;
        }
        if (st != NULL)
            sqlite3_finalize(st);
        if (n == 0) {
            printf("  Aucun message en attente.\n");
            return;
        }
        printf("1. Marquer un message comme envoyé\n");
        printf("2. Marquer TOUS les messages affichés comme envoyés\n");
        printf("3. Ignorer un message (ne pas l'envoyer)\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 3);
        if (choix == 0)
            return;
        if (choix == 2) {
            int i;

            if (!confirmer("Confirmer ?"))
                continue;
            for (i = 0; i < n; i++)
                req_executer("UPDATE notifications SET statut = 1,"
                             " traite_le = datetime('now','localtime')"
                             " WHERE id = ?", "i", ids[i]);
            printf("  %d message(s) marqué(s) comme envoyé(s).\n", n);
            continue;
        }
        {
            int k = lire_entier("N° du message (0 = annuler) : ", 0, n);

            if (k == 0)
                continue;
            req_executer("UPDATE notifications SET statut = ?,"
                         " traite_le = datetime('now','localtime')"
                         " WHERE id = ?", "ii", choix == 1 ? 1 : 2,
                         ids[k - 1]);
        }
    }
}

/* Rappel pour le linge pret depuis plus de N jours et non retire. */
static void generer_rappels(void)
{
    sqlite3_stmt *st;
    char delai[24];
    int ids[500], n = 0, i;

    snprintf(delai, sizeof delai, "-%d days", config.rappel_jours);
    st = req_preparer(
        "SELECT c.id FROM commandes c WHERE c.statut = ? AND c.annulee = 0"
        " AND (SELECT MAX(h.date) FROM historique_statuts h"
        "      WHERE h.commande_id = c.id AND h.nouveau = ?)"
        "     < datetime('now','localtime', ?)"
        " AND NOT EXISTS (SELECT 1 FROM notifications n"
        "      WHERE n.commande_id = c.id AND n.type = 'RAPPEL'"
        "      AND n.cree_le > datetime('now','localtime', ?))",
        "iitt", (int)ST_PRET, (int)ST_PRET, delai, delai);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW && n < 500)
        ids[n++] = sqlite3_column_int(st, 0);
    if (st != NULL)
        sqlite3_finalize(st);
    for (i = 0; i < n; i++)
        creer_notification(ids[i], "RAPPEL", config.modele_rappel);
    printf("  %d rappel(s) préparé(s) (linge prêt depuis plus de %d jour(s)).\n",
           n, config.rappel_jours);
}

static void historique(void)
{
    sqlite3_stmt *st;
    static const char *etats[] = { "à envoyer", "envoyé", "ignoré" };

    titre("Historique des messages (50 derniers)");
    st = req_preparer("SELECT substr(cree_le, 1, 16), type, telephone,"
                      " statut, message FROM notifications"
                      " ORDER BY id DESC LIMIT 50", NULL);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        int s = sqlite3_column_int(st, 3);

        printf("%s [%s] %s (%s)\n    %s\n", col_texte(st, 0),
               col_texte(st, 1), col_texte(st, 2),
               s >= 0 && s <= 2 ? etats[s] : "?", col_texte(st, 4));
    }
    if (st != NULL)
        sqlite3_finalize(st);
}

/* Fichier CSV telephone;message pour un outil d'envoi en masse. */
static void exporter(void)
{
    sqlite3_stmt *st;
    char chemin[96], date[24];
    time_t t = time(NULL);
    FILE *f;
    int n = 0;

    creer_dossier("exports");
    strftime(date, sizeof date, "%Y%m%d_%H%M", localtime(&t));
    snprintf(chemin, sizeof chemin, "exports/messages_%s.csv", date);
    f = fopen(chemin, "wb");
    if (f == NULL) {
        printf("  Impossible de créer %s\n", chemin);
        return;
    }
    fputs("\xEF\xBB\xBFTéléphone;Message\r\n", f);
    st = req_preparer("SELECT telephone, message FROM notifications"
                      " WHERE statut = 0 ORDER BY id", NULL);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        csv_champ(f, col_texte(st, 0), 0);
        csv_champ(f, col_texte(st, 1), 1);
        n++;
    }
    if (st != NULL)
        sqlite3_finalize(st);
    fclose(f);
    printf("  %d message(s) exporté(s) dans %s\n", n, chemin);
}

void menu_notifications(void)
{
    int choix;

    for (;;) {
        titre("Notifications clients");
        printf("Messages en attente : %d\n\n", notifications_en_attente());
        printf("1. Messages à envoyer\n");
        printf("2. Préparer les rappels (linge prêt non retiré)\n");
        printf("3. Historique des messages\n");
        printf("4. Exporter les messages en attente (CSV)\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 4);
        switch (choix) {
        case 0: return;
        case 1: messages_a_envoyer(); break;
        case 2: generer_rappels(); break;
        case 3: historique(); break;
        case 4: exporter(); break;
        }
        pause_console();
    }
}
