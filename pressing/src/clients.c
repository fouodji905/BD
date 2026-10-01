#include <stdio.h>
#include <string.h>
#include <time.h>

#include "base.h"
#include "clients.h"
#include "commandes.h"
#include "console.h"
#include "parametres.h"
#include "securite.h"

#define MAX_RESULTATS 30

/* Prototypes des fonctions internes au module. */
static int creer_client(const char *telephone_saisi);
static int rechercher(const char *texte, int ids[], int max);
static void fiche_client(int id);
static void modifier_client(int id);
static void ajouter_note(int id);
static void fusionner(int id);
static void lister_clients(void);
static void exporter_clients(void);
static void rechercher_et_ouvrir(void);

static int creer_client(const char *telephone_saisi)
{
    char nom[80], telephone[40], email[80], adresse[120];
    long long existant;
    int id;

    titre("Nouveau client");
    lire_obligatoire("Nom complet : ", nom, sizeof nom);
    if (telephone_saisi != NULL && telephone_saisi[0] != '\0') {
        copier(telephone, telephone_saisi, sizeof telephone);
        printf("Téléphone : %s\n", telephone);
    } else {
        lire_ligne("Téléphone : ", telephone, sizeof telephone);
    }
    if (telephone[0] != '\0') {
        existant = req_entier("SELECT id FROM clients WHERE telephone = ?"
                              " AND actif = 1", "t", telephone);
        if (existant > 0) {
            char autre[80];

            req_texte(autre, sizeof autre,
                      "SELECT nom FROM clients WHERE id = ?", "l", existant);
            printf("  Ce numéro appartient déjà à : %s\n", autre);
            if (confirmer("Utiliser ce client existant ?"))
                return (int)existant;
        }
    }
    lire_ligne("Email (facultatif) : ", email, sizeof email);
    lire_ligne("Adresse / quartier (facultatif) : ", adresse, sizeof adresse);
    if (!req_executer("INSERT INTO clients (nom, telephone, email, adresse)"
                      " VALUES (?, ?, ?, ?)", "tttt",
                      nom, telephone, email, adresse))
        return 0;
    id = (int)base_dernier_id();
    req_executer("UPDATE clients SET code = printf('C%05d', id)"
                 " WHERE id = ?", "i", id);
    journaliser("Client n°%d créé : %s", id, nom);
    printf("  Client créé (code C%05d).\n", id);
    return id;
}

/* Cherche par code, nom ou telephone. Affiche et renvoie les resultats. */
static int rechercher(const char *texte, int ids[], int max)
{
    sqlite3_stmt *st;
    char motif[128];
    int n = 0;

    snprintf(motif, sizeof motif, "%%%s%%", texte);
    st = req_preparer("SELECT c.id, c.code, c.nom, c.telephone,"
                      " (SELECT COUNT(*) FROM commandes m"
                      "  WHERE m.client_id = c.id AND m.annulee = 0)"
                      " FROM clients c WHERE c.actif = 1 AND"
                      " (c.code = upper(?) OR c.nom LIKE ? OR c.telephone LIKE ?)"
                      " ORDER BY c.nom LIMIT ?", "tttl",
                      texte, motif, motif, (long long)max);
    if (st == NULL)
        return 0;
    while (sqlite3_step(st) == SQLITE_ROW && n < max) {
        ids[n] = sqlite3_column_int(st, 0);
        printf("  %2d. ", n + 1);
        ecrire_col(stdout, col_texte(st, 1), 7, 0);
        printf(" ");
        ecrire_col(stdout, col_texte(st, 2), 28, 0);
        printf(" ");
        ecrire_col(stdout, col_texte(st, 3), 16, 0);
        printf(" %d commande(s)\n", sqlite3_column_int(st, 4));
        n++;
    }
    sqlite3_finalize(st);
    return n;
}

int client_choisir(void)
{
    char texte[80];
    int ids[MAX_RESULTATS], n, choix;

    for (;;) {
        printf("\nClient : tapez un nom, un téléphone ou un code client.\n");
        printf("(« + » = nouveau client, vide = annuler)\n");
        if (lire_ligne("Recherche : ", texte, sizeof texte) == 0)
            return 0;
        if (strcmp(texte, "+") == 0)
            return creer_client(NULL);
        n = rechercher(texte, ids, MAX_RESULTATS);
        if (n == 0) {
            printf("  Aucun client trouvé.\n");
            if (confirmer("Créer un nouveau client ?")) {
                /* Si on a tape un numero, on le reutilise. */
                if (strspn(texte, "+0123456789 ") == strlen(texte))
                    return creer_client(texte);
                return creer_client(NULL);
            }
            continue;
        }
        choix = lire_entier("N° dans la liste (0 = nouvelle recherche) : ",
                            0, n);
        if (choix > 0)
            return ids[choix - 1];
    }
}

const char *client_segment(int client_id)
{
    long long nb, total, inactif;

    nb = req_entier("SELECT COUNT(*) FROM commandes"
                    " WHERE client_id = ? AND annulee = 0", "i", client_id);
    if (nb == 0)
        return "Nouveau";
    total = req_entier("SELECT COALESCE(SUM(total), 0) FROM commandes"
                       " WHERE client_id = ? AND annulee = 0", "i", client_id);
    if (total >= config.seuil_vip)
        return "VIP";
    inactif = req_entier("SELECT MAX(date_depot) <"
                         " datetime('now','localtime','-90 days')"
                         " FROM commandes WHERE client_id = ? AND annulee = 0",
                         "i", client_id);
    if (inactif)
        return "Inactif (+90 j)";
    return nb == 1 ? "Nouveau" : "Régulier";
}

static void fiche_client(int id)
{
    sqlite3_stmt *st;
    int choix;

    for (;;) {
        st = req_preparer("SELECT code, nom, telephone, email, adresse, notes,"
                          " cree_le FROM clients WHERE id = ?", "i", id);
        if (st == NULL)
            return;
        if (sqlite3_step(st) != SQLITE_ROW) {
            sqlite3_finalize(st);
            printf("  Client introuvable.\n");
            return;
        }
        titre("Fiche client");
        printf("Code       : %s\n", col_texte(st, 0));
        printf("Nom        : %s\n", col_texte(st, 1));
        printf("Téléphone  : %s\n", col_texte(st, 2));
        printf("Email      : %s\n", col_texte(st, 3));
        printf("Adresse    : %s\n", col_texte(st, 4));
        printf("Client depuis : %s\n", col_texte(st, 6));
        printf("Segment    : %s\n", client_segment(id));
        if (col_texte(st, 5)[0] != '\0')
            printf("Notes :\n%s\n", col_texte(st, 5));
        sqlite3_finalize(st);

        printf("Commandes  : %lld  |  Total dépensé : %s  |  Reste dû : %s\n",
               req_entier("SELECT COUNT(*) FROM commandes WHERE client_id = ?"
                          " AND annulee = 0", "i", id),
               fmt_montant(req_entier("SELECT COALESCE(SUM(total), 0)"
                                      " FROM commandes WHERE client_id = ?"
                                      " AND annulee = 0", "i", id)),
               fmt_montant(req_entier("SELECT COALESCE(SUM(total - paye), 0)"
                                      " FROM v_commandes WHERE client_id = ?"
                                      " AND annulee = 0", "i", id)));
        printf("\nDernières commandes :\n");
        commandes_lister("WHERE client_id = ? ORDER BY id DESC LIMIT 10", id,
                         NULL, 0);

        printf("\n1. Modifier   2. Ajouter une note   3. Fusionner un doublon"
               "   4. Désactiver   0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 4);
        switch (choix) {
        case 0:
            return;
        case 1:
            modifier_client(id);
            break;
        case 2:
            ajouter_note(id);
            break;
        case 3:
            if (exiger(P_ANNULER))
                fusionner(id);
            break;
        case 4:
            if (exiger(P_ANNULER) && confirmer("Désactiver ce client ?")) {
                req_executer("UPDATE clients SET actif = 0 WHERE id = ?",
                             "i", id);
                journaliser("Client n°%d désactivé", id);
                return;
            }
            break;
        }
    }
}

static void modifier_client(int id)
{
    char actuel[4][120], nouveau[4][120];
    static const char *champs[4] = { "nom", "telephone", "email", "adresse" };
    static const char *libelles[4] = { "Nom", "Téléphone", "Email", "Adresse" };
    char sql[96];
    int i;

    printf("(Entrée = garder, « - » = vider)\n");
    for (i = 0; i < 4; i++) {
        snprintf(sql, sizeof sql, "SELECT %s FROM clients WHERE id = ?",
                 champs[i]);
        req_texte(actuel[i], sizeof actuel[i], sql, "i", id);
        lire_avec_defaut(libelles[i], actuel[i], nouveau[i],
                         sizeof nouveau[i]);
    }
    if (nouveau[0][0] == '\0')
        copier(nouveau[0], actuel[0], sizeof nouveau[0]);
    req_executer("UPDATE clients SET nom = ?, telephone = ?, email = ?,"
                 " adresse = ? WHERE id = ?", "tttti",
                 nouveau[0], nouveau[1], nouveau[2], nouveau[3], id);
    journaliser("Client n°%d modifié", id);
    printf("  Enregistré.\n");
}

static void ajouter_note(int id)
{
    char note[200];

    if (lire_ligne("Note : ", note, sizeof note) == 0)
        return;
    req_executer("UPDATE clients SET notes = notes ||"
                 " CASE WHEN notes = '' THEN '' ELSE char(10) END ||"
                 " '- ' || date('now','localtime') || ' (' || ? || ') : ' || ?"
                 " WHERE id = ?", "tti", session.identifiant, note, id);
    printf("  Note ajoutée.\n");
}

/* Rattache les commandes d'un doublon a ce client, puis desactive le doublon. */
static void fusionner(int id)
{
    char texte[80];
    int ids[MAX_RESULTATS], n, choix, doublon;

    printf("Recherchez le doublon à fusionner dans ce client.\n");
    if (lire_ligne("Recherche : ", texte, sizeof texte) == 0)
        return;
    n = rechercher(texte, ids, MAX_RESULTATS);
    if (n == 0) {
        printf("  Aucun client trouvé.\n");
        return;
    }
    choix = lire_entier("N° du doublon (0 = annuler) : ", 0, n);
    if (choix == 0)
        return;
    doublon = ids[choix - 1];
    if (doublon == id) {
        printf("  C'est le même client.\n");
        return;
    }
    if (!confirmer("Ses commandes seront rattachées à ce client et le doublon"
                   " sera désactivé. Confirmer ?"))
        return;
    base_debut();
    req_executer("UPDATE commandes SET client_id = ? WHERE client_id = ?",
                 "ii", id, doublon);
    req_executer("UPDATE notifications SET client_id = ? WHERE client_id = ?",
                 "ii", id, doublon);
    req_executer("UPDATE clients SET actif = 0, notes = notes ||"
                 " ' [fusionné dans le client n°' || ? || ']' WHERE id = ?",
                 "ii", id, doublon);
    base_valider();
    journaliser("Client n°%d fusionné dans le client n°%d", doublon, id);
    printf("  Fusion effectuée.\n");
}

static void lister_clients(void)
{
    sqlite3_stmt *st;

    titre("Clients (200 plus récents)");
    st = req_preparer("SELECT code, nom, telephone, cree_le FROM clients"
                      " WHERE actif = 1 ORDER BY id DESC LIMIT 200", NULL);
    if (st == NULL)
        return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("  ");
        ecrire_col(stdout, col_texte(st, 0), 7, 0);
        printf(" ");
        ecrire_col(stdout, col_texte(st, 1), 30, 0);
        printf(" ");
        ecrire_col(stdout, col_texte(st, 2), 16, 0);
        printf(" %s\n", col_texte(st, 3));
    }
    sqlite3_finalize(st);
    printf("\nTotal : %lld client(s) actif(s).\n",
           req_entier("SELECT COUNT(*) FROM clients WHERE actif = 1", NULL));
}

void csv_champ(FILE *f, const char *texte, int dernier)
{
    fputc('"', f);
    for (; *texte != '\0'; texte++) {
        if (*texte == '"')
            fputc('"', f);
        fputc(*texte, f);
    }
    fputc('"', f);
    fputs(dernier ? "\r\n" : ";", f);
}

void csv_montant(FILE *f, long long centimes, int dernier)
{
    char nombre[32];
    long long absolu = centimes < 0 ? -centimes : centimes;

    snprintf(nombre, sizeof nombre, "%s%lld,%02lld", centimes < 0 ? "-" : "",
             absolu / 100, absolu % 100);
    csv_champ(f, nombre, dernier);
}

static void exporter_clients(void)
{
    sqlite3_stmt *st;
    char chemin[96], date[16];
    time_t t = time(NULL);
    FILE *f;
    int n = 0;

    creer_dossier("exports");
    strftime(date, sizeof date, "%Y%m%d", localtime(&t));
    snprintf(chemin, sizeof chemin, "exports/clients_%s.csv", date);
    f = fopen(chemin, "wb");
    if (f == NULL) {
        printf("  Impossible de créer %s\n", chemin);
        return;
    }
    fputs("\xEF\xBB\xBF", f); /* BOM : Excel lit correctement les accents */
    fputs("Code;Nom;Téléphone;Email;Adresse;Client depuis;Commandes;"
          "Total dépensé\r\n", f);
    st = req_preparer("SELECT c.code, c.nom, c.telephone, c.email, c.adresse,"
                      " c.cree_le,"
                      " (SELECT COUNT(*) FROM commandes m WHERE"
                      "  m.client_id = c.id AND m.annulee = 0),"
                      " (SELECT COALESCE(SUM(total), 0) FROM commandes m"
                      "  WHERE m.client_id = c.id AND m.annulee = 0)"
                      " FROM clients c WHERE c.actif = 1 ORDER BY c.nom", NULL);
    if (st != NULL) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            char nombre[32];
            int i;

            for (i = 0; i < 6; i++)
                csv_champ(f, col_texte(st, i), 0);
            snprintf(nombre, sizeof nombre, "%d", sqlite3_column_int(st, 6));
            csv_champ(f, nombre, 0);
            csv_montant(f, sqlite3_column_int64(st, 7), 1);
            n++;
        }
        sqlite3_finalize(st);
    }
    fclose(f);
    journaliser("Export de %d clients", n);
    printf("  %d client(s) exporté(s) dans %s\n", n, chemin);
}

static void rechercher_et_ouvrir(void)
{
    char texte[80];
    int ids[MAX_RESULTATS], n, choix;

    if (lire_ligne("Nom, téléphone ou code : ", texte, sizeof texte) == 0)
        return;
    n = rechercher(texte, ids, MAX_RESULTATS);
    if (n == 0) {
        printf("  Aucun client trouvé.\n");
        pause_console();
        return;
    }
    choix = lire_entier("N° dans la liste (0 = retour) : ", 0, n);
    if (choix > 0)
        fiche_client(ids[choix - 1]);
}

void menu_clients(void)
{
    int choix, id;

    for (;;) {
        titre("Clients");
        printf("1. Rechercher un client (fiche, historique)\n");
        printf("2. Nouveau client\n");
        printf("3. Liste des clients\n");
        printf("4. Exporter les clients (CSV / Excel)\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 4);
        switch (choix) {
        case 0:
            return;
        case 1:
            rechercher_et_ouvrir();
            break;
        case 2:
            id = creer_client(NULL);
            if (id > 0)
                fiche_client(id);
            break;
        case 3:
            lister_clients();
            pause_console();
            break;
        case 4:
            if (exiger(P_RAPPORTS))
                exporter_clients();
            pause_console();
            break;
        }
    }
}
