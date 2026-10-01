#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base.h"
#include "caisse.h"
#include "catalogue.h"
#include "clients.h"
#include "commandes.h"
#include "console.h"
#include "notifications.h"
#include "parametres.h"
#include "securite.h"

#define MAX_LIGNES 50
#define MAX_PAIEMENTS 8
#define MAX_LISTE 40
#define LARGEUR_RECU 42

typedef struct {
    int service_id;
    char libelle[80];
    int quantite;
    Montant prix_unitaire;
    char options[160];
    Montant prix_options;   /* par piece */
    int duree;
    char couleur[40];
    char marque[40];
    char defauts[120];
    char instructions[120];
} Ligne;

typedef struct {
    Ligne lignes[MAX_LIGNES];
    int nb;
    int express;
} Panier;

typedef struct {
    int mode;
    Montant montant;
} Paiement;

typedef struct {
    Montant sous_total;
    Montant majoration;
    Montant remise;
    Montant total;
} Totaux;

static const char *NOMS_STATUTS[NB_STATUTS] = {
    "Reçu", "Tri", "Lavage", "Séchage", "Repassage", "Contrôle", "Prêt",
    "Récupéré"
};

/* Prototypes des fonctions internes au module. */
static Montant arrondir(Montant m);
static Montant montant_ligne(const Ligne *l);
static void calculer(const Panier *p, int remise_pct, Totaux *t);
static int ajouter_article(Panier *p);
static void lire_options(Ligne *l);
static void retirer_article(Panier *p);
static void afficher_panier(const Panier *p);
static int saisir_paiements(Montant du, Paiement pay[], int max);
static int enregistrer_paiements(int commande_id, int session_id,
                                 const Paiement pay[], int n);
static void generer_numero(char *dest, size_t taille);
static int enregistrer_commande(int client_id, const Panier *p,
                                int remise_pct, const Totaux *t,
                                const char *notes, int session_id,
                                const Paiement pay[], int nb_pay);
static Montant reste_a_payer(int id);
static void ligne_recu(FILE *f, const char *gauche, const char *droite);
static void centre_recu(FILE *f, const char *texte);
static void ecrire_recu(FILE *f, int id);
static void imprimer_recu(int id);
static void afficher_commande(int id);
static void encaisser(int id);
static void retrait(int id);
static void annuler(int id);
static void rechercher(void);
static int lister(const char *fin_sql, int param, const char *texte,
                  int ids[], int max);
static void choisir_dans_liste(const char *titre_liste, const char *fin_sql,
                               const char *texte);

const char *statut_nom(int statut)
{
    if (statut < 0 || statut >= NB_STATUTS)
        return "?";
    return NOMS_STATUTS[statut];
}

/* En FCFA (0 decimale), on arrondit a l'unite. */
static Montant arrondir(Montant m)
{
    if (config.decimales == 0)
        return (m + 50) / 100 * 100;
    return m;
}

static Montant montant_ligne(const Ligne *l)
{
    return l->quantite * (l->prix_unitaire + l->prix_options);
}

static void calculer(const Panier *p, int remise_pct, Totaux *t)
{
    int i;

    t->sous_total = 0;
    for (i = 0; i < p->nb; i++)
        t->sous_total += montant_ligne(&p->lignes[i]);
    t->majoration = p->express
                  ? arrondir(t->sous_total * config.express_pct / 100) : 0;
    t->remise = arrondir((t->sous_total + t->majoration) * remise_pct / 100);
    t->total = t->sous_total + t->majoration - t->remise;
}

/* ---------- Panier ---------- */

static void lire_options(Ligne *l)
{
    char saisie[120], *jeton;
    sqlite3_stmt *st;

    l->options[0] = '\0';
    l->prix_options = 0;
    if (req_entier("SELECT COUNT(*) FROM options_service WHERE actif = 1",
                   NULL) == 0)
        return;
    printf("Options disponibles :\n");
    catalogue_afficher_options(1);
    lire_ligne("N° des options, séparés par des virgules (vide = aucune) : ",
               saisie, sizeof saisie);
    for (jeton = strtok(saisie, ", ;"); jeton != NULL;
         jeton = strtok(NULL, ", ;")) {
        st = req_preparer("SELECT nom, prix FROM options_service"
                          " WHERE id = ? AND actif = 1", "i", atoi(jeton));
        if (st == NULL)
            continue;
        if (sqlite3_step(st) == SQLITE_ROW) {
            if (l->options[0] != '\0')
                strncat(l->options, ", ",
                        sizeof l->options - strlen(l->options) - 1);
            strncat(l->options, col_texte(st, 0),
                    sizeof l->options - strlen(l->options) - 1);
            l->prix_options += sqlite3_column_int64(st, 1);
        } else {
            printf("  Option %s ignorée (inconnue).\n", jeton);
        }
        sqlite3_finalize(st);
    }
}

static int ajouter_article(Panier *p)
{
    Ligne *l;
    sqlite3_stmt *st;
    char saisie[32];
    int service_id;

    if (p->nb >= MAX_LIGNES) {
        printf("  Nombre maximal d'articles atteint (%d).\n", MAX_LIGNES);
        return 0;
    }
    l = &p->lignes[p->nb];
    memset(l, 0, sizeof *l);
    for (;;) {
        if (lire_ligne("N° du service (? = voir la liste, vide = annuler) : ",
                       saisie, sizeof saisie) == 0)
            return 0;
        if (strcmp(saisie, "?") == 0) {
            catalogue_afficher_services(1);
            continue;
        }
        service_id = atoi(saisie);
        st = req_preparer("SELECT nom, prix, duree_heures FROM services"
                          " WHERE id = ? AND actif = 1", "i", service_id);
        if (st == NULL)
            return 0;
        if (sqlite3_step(st) == SQLITE_ROW) {
            l->service_id = service_id;
            copier(l->libelle, col_texte(st, 0), sizeof l->libelle);
            l->prix_unitaire = sqlite3_column_int64(st, 1);
            l->duree = sqlite3_column_int(st, 2);
            sqlite3_finalize(st);
            break;
        }
        sqlite3_finalize(st);
        printf("  Service introuvable. Tapez ? pour voir la liste.\n");
    }
    printf("  -> %s à %s\n", l->libelle, fmt_montant(l->prix_unitaire));
    l->quantite = lire_entier_defaut("Quantité [1] : ", 1, 999, 1);
    lire_options(l);
    printf("Détails (facultatifs, Entrée pour passer) :\n");
    lire_ligne("  Couleur : ", l->couleur, sizeof l->couleur);
    lire_ligne("  Marque : ", l->marque, sizeof l->marque);
    lire_ligne("  Défauts constatés (taches, trous...) : ", l->defauts,
               sizeof l->defauts);
    lire_ligne("  Instructions du client : ", l->instructions,
               sizeof l->instructions);
    p->nb++;
    printf("  Ajouté : %d x %s = %s\n", l->quantite, l->libelle,
           fmt_montant(montant_ligne(l)));
    return 1;
}

static void retirer_article(Panier *p)
{
    int n;

    if (p->nb == 0)
        return;
    n = lire_entier("N° de l'article à retirer (0 = annuler) : ", 0, p->nb);
    if (n == 0)
        return;
    memmove(&p->lignes[n - 1], &p->lignes[n],
            (size_t)(p->nb - n) * sizeof p->lignes[0]);
    p->nb--;
}

static void afficher_panier(const Panier *p)
{
    Totaux t;
    int i;

    separateur();
    if (p->nb == 0) {
        printf("  Panier vide.\n");
    }
    for (i = 0; i < p->nb; i++) {
        const Ligne *l = &p->lignes[i];
        char texte[160];

        snprintf(texte, sizeof texte, "%d x %s", l->quantite, l->libelle);
        printf("  %2d. ", i + 1);
        ecrire_col(stdout, texte, 34, 0);
        ecrire_col(stdout, fmt_montant(montant_ligne(l)), 16, 1);
        printf("\n");
        if (l->options[0] != '\0')
            printf("      + %s\n", l->options);
    }
    calculer(p, 0, &t);
    printf("  Sous-total : %s", fmt_montant(t.sous_total));
    if (p->express)
        printf("   + express %d %% : %s", config.express_pct,
               fmt_montant(t.majoration));
    printf("\n");
    separateur();
}

/* ---------- Paiements ---------- */

/* Demande un ou plusieurs paiements, sans depasser le montant du. */
static int saisir_paiements(Montant du, Paiement pay[], int max)
{
    Montant reste = du, m;
    int n = 0, mode;

    while (reste > 0 && n < max) {
        printf("Reste à payer : %s\n", fmt_montant(reste));
        m = lire_montant_defaut("Montant reçu (Entrée = tout, 0 = rien "
                                "pour l'instant)", reste);
        if (m == 0)
            break;
        mode = choisir_mode_paiement();
        if (m > reste) {
            if (mode != MODE_ESPECES) {
                printf("  Le montant dépasse le reste à payer.\n");
                continue;
            }
            printf("  >>> MONNAIE À RENDRE : %s <<<\n",
                   fmt_montant(m - reste));
            m = reste;
        }
        pay[n].mode = mode;
        pay[n].montant = m;
        n++;
        reste -= m;
    }
    return n;
}

static int enregistrer_paiements(int commande_id, int session_id,
                                 const Paiement pay[], int n)
{
    int i;

    for (i = 0; i < n; i++)
        if (!req_executer("INSERT INTO paiements (commande_id, session_id,"
                          " mode, montant, utilisateur_id)"
                          " VALUES (?, ?, ?, ?, ?)", "iiili",
                          commande_id, session_id, pay[i].mode,
                          pay[i].montant, session.id))
            return 0;
    return 1;
}

/* ---------- Creation de commande ---------- */

/* Numero de ticket : P + date + numero du jour, ex. P261001-004. */
static void generer_numero(char *dest, size_t taille)
{
    char date[16], motif[24];
    time_t t = time(NULL);
    long long dernier;

    strftime(date, sizeof date, "%y%m%d", localtime(&t));
    snprintf(motif, sizeof motif, "P%s-%%", date);
    dernier = req_entier("SELECT COALESCE(MAX(CAST(substr(numero, 9)"
                         " AS INTEGER)), 0) FROM commandes"
                         " WHERE numero LIKE ?", "t", motif);
    snprintf(dest, taille, "P%.6s-%03d", date, (int)(dernier + 1) % 100000);
}

static int enregistrer_commande(int client_id, const Panier *p,
                                int remise_pct, const Totaux *t,
                                const char *notes, int session_id,
                                const Paiement pay[], int nb_pay)
{
    char numero[24], delai[24];
    int i, id, duree = 1;

    for (i = 0; i < p->nb; i++)
        if (p->lignes[i].duree > duree)
            duree = p->lignes[i].duree;
    if (p->express)
        duree = (duree + 1) / 2;
    snprintf(delai, sizeof delai, "+%d hours", duree);

    base_debut();
    generer_numero(numero, sizeof numero);
    if (!req_executer("INSERT INTO commandes (numero, client_id,"
                      " utilisateur_id, date_prevue, express, sous_total,"
                      " majoration, remise_pct, remise, total, taux_tva,"
                      " statut, notes) VALUES (?, ?, ?,"
                      " datetime('now','localtime', ?), ?, ?, ?, ?, ?, ?, ?,"
                      " ?, ?)", "tiitillilliit",
                      numero, client_id, session.id, delai, p->express,
                      t->sous_total, t->majoration, remise_pct, t->remise,
                      t->total, config.taux_tva, (int)ST_RECU, notes)) {
        base_annuler();
        return 0;
    }
    id = (int)base_dernier_id();
    for (i = 0; i < p->nb; i++) {
        const Ligne *l = &p->lignes[i];

        if (!req_executer("INSERT INTO lignes_commande (commande_id,"
                          " service_id, libelle, quantite, prix_unitaire,"
                          " options, prix_options, montant, couleur, marque,"
                          " defauts, instructions)"
                          " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                          "iitiltlltttt", id, l->service_id, l->libelle,
                          l->quantite, l->prix_unitaire, l->options,
                          l->prix_options, montant_ligne(l), l->couleur,
                          l->marque, l->defauts, l->instructions)) {
            base_annuler();
            return 0;
        }
    }
    if (!req_executer("INSERT INTO historique_statuts (commande_id, nouveau,"
                      " utilisateur_id) VALUES (?, ?, ?)", "iii",
                      id, (int)ST_RECU, session.id)
        || !enregistrer_paiements(id, session_id, pay, nb_pay)
        || !base_valider()) {
        base_annuler();
        return 0;
    }
    journaliser("Commande %s créée (%s)", numero, fmt_montant(t->total));
    return id;
}

void commande_nouvelle(void)
{
    Panier *panier;
    Paiement pay[MAX_PAIEMENTS];
    Totaux t;
    char nom_client[80], notes[200];
    int session_id, client_id, choix, remise_pct, nb_pay, id;

    session_id = caisse_exiger_session();
    if (session_id == 0)
        return;
    titre("Nouvelle commande");
    client_id = client_choisir();
    if (client_id == 0)
        return;
    req_texte(nom_client, sizeof nom_client,
              "SELECT nom FROM clients WHERE id = ?", "i", client_id);
    printf("\nClient : %s (%s)\n", nom_client, client_segment(client_id));

    panier = calloc(1, sizeof *panier);
    if (panier == NULL)
        return;
    {
        char question[96];

        snprintf(question, sizeof question,
                 "Service EXPRESS (+%d %%, délai divisé par 2) ?",
                 config.express_pct);
        panier->express = confirmer(question);
    }
    printf("\nServices disponibles :");
    catalogue_afficher_services(1);
    printf("\n");
    ajouter_article(panier);
    for (;;) {
        afficher_panier(panier);
        printf("1. Ajouter un article   2. Retirer un article   "
               "3. Valider   0. Abandonner\n");
        choix = lire_entier("Votre choix : ", 0, 3);
        if (choix == 0) {
            if (confirmer("Abandonner cette commande ?")) {
                free(panier);
                return;
            }
        } else if (choix == 1) {
            ajouter_article(panier);
        } else if (choix == 2) {
            retirer_article(panier);
        } else if (panier->nb == 0) {
            printf("  Ajoutez au moins un article.\n");
        } else {
            break;
        }
    }

    for (;;) {
        remise_pct = lire_entier_defaut("Remise en % [0] : ", 0, 100, 0);
        if (remise_pct <= config.remise_max || autorise(P_REMISE_LIBRE))
            break;
        printf("  Remise maximale autorisée pour votre rôle : %d %%.\n",
               config.remise_max);
    }
    lire_ligne("Remarque sur la commande (facultatif) : ", notes,
               sizeof notes);
    calculer(panier, remise_pct, &t);

    titre("Récapitulatif");
    afficher_panier(panier);
    if (t.remise > 0)
        printf("  Remise %d %% : -%s\n", remise_pct, fmt_montant(t.remise));
    printf("  TOTAL À PAYER : %s\n\n", fmt_montant(t.total));

    nb_pay = saisir_paiements(t.total, pay, MAX_PAIEMENTS);
    id = enregistrer_commande(client_id, panier, remise_pct, &t, notes,
                              session_id, pay, nb_pay);
    free(panier);
    if (id == 0) {
        printf("  ERREUR : la commande n'a pas pu être enregistrée.\n");
        pause_console();
        return;
    }
    printf("\n  Commande enregistrée.\n");
    imprimer_recu(id);
    pause_console();
}

/* ---------- Recu ---------- */

static void ligne_recu(FILE *f, const char *gauche, const char *droite)
{
    int place = LARGEUR_RECU - utf8_longueur(droite) - 1;

    ecrire_col(f, gauche, place, 0);
    fprintf(f, " %s\n", droite);
}

static void centre_recu(FILE *f, const char *texte)
{
    int marge = (LARGEUR_RECU - utf8_longueur(texte)) / 2;

    if (texte[0] == '\0')
        return;
    if (marge > 0)
        fprintf(f, "%*s", marge, "");
    fprintf(f, "%s\n", texte);
}

static void ecrire_recu(FILE *f, int id)
{
    sqlite3_stmt *st;
    char texte[160], numero[24];
    int express, remise_pct, taux, annulee, statut;
    Montant total, paye;
    int i;

    st = req_preparer("SELECT numero, substr(date_depot, 1, 16),"
                      " substr(date_prevue, 1, 16), express, client_nom,"
                      " client_code, client_tel, sous_total, majoration,"
                      " remise_pct, remise, total, taux_tva, paye, annulee,"
                      " statut, notes, motif_annulation,"
                      " (SELECT nom FROM utilisateurs u"
                      "  WHERE u.id = v_commandes.utilisateur_id)"
                      " FROM v_commandes WHERE id = ?", "i", id);
    if (st == NULL || sqlite3_step(st) != SQLITE_ROW) {
        if (st != NULL)
            sqlite3_finalize(st);
        return;
    }
    copier(numero, col_texte(st, 0), sizeof numero);
    express = sqlite3_column_int(st, 3);
    remise_pct = sqlite3_column_int(st, 9);
    total = sqlite3_column_int64(st, 11);
    taux = sqlite3_column_int(st, 12);
    paye = sqlite3_column_int64(st, 13);
    annulee = sqlite3_column_int(st, 14);
    statut = sqlite3_column_int(st, 15);

    centre_recu(f, config.nom);
    centre_recu(f, config.adresse);
    if (config.telephone[0] != '\0') {
        snprintf(texte, sizeof texte, "Tél : %s", config.telephone);
        centre_recu(f, texte);
    }
    centre_recu(f, config.identifiant_fiscal);
    for (i = 0; i < LARGEUR_RECU; i++)
        fputc('=', f);
    fputc('\n', f);
    if (annulee)
        fprintf(f, "*** COMMANDE ANNULÉE ***\nMotif : %s\n",
                col_texte(st, 17));
    fprintf(f, "TICKET    : %s\n", numero);
    fprintf(f, "Déposé le : %s\n", col_texte(st, 1));
    fprintf(f, "Retrait   : %s%s\n", col_texte(st, 2),
            express ? "  (EXPRESS)" : "");
    fprintf(f, "Client    : %s (%s)\n", col_texte(st, 4), col_texte(st, 5));
    if (col_texte(st, 6)[0] != '\0')
        fprintf(f, "Tél       : %s\n", col_texte(st, 6));
    fprintf(f, "Servi par : %s\n", col_texte(st, 18));
    for (i = 0; i < LARGEUR_RECU; i++)
        fputc('-', f);
    fputc('\n', f);

    {
        sqlite3_stmt *lg = req_preparer(
            "SELECT quantite, libelle, montant, options, couleur, marque,"
            " defauts, instructions FROM lignes_commande"
            " WHERE commande_id = ? ORDER BY id", "i", id);

        while (lg != NULL && sqlite3_step(lg) == SQLITE_ROW) {
            char details[200] = "";
            int c;

            snprintf(texte, sizeof texte, "%d x %s",
                     sqlite3_column_int(lg, 0), col_texte(lg, 1));
            ligne_recu(f, texte, fmt_montant(sqlite3_column_int64(lg, 2)));
            if (col_texte(lg, 3)[0] != '\0')
                fprintf(f, "   + %s\n", col_texte(lg, 3));
            for (c = 4; c <= 5; c++) {
                if (col_texte(lg, c)[0] == '\0')
                    continue;
                if (details[0] != '\0')
                    strncat(details, ", ",
                            sizeof details - strlen(details) - 1);
                strncat(details, col_texte(lg, c),
                        sizeof details - strlen(details) - 1);
            }
            if (details[0] != '\0')
                fprintf(f, "   %s\n", details);
            if (col_texte(lg, 6)[0] != '\0')
                fprintf(f, "   Défauts : %s\n", col_texte(lg, 6));
            if (col_texte(lg, 7)[0] != '\0')
                fprintf(f, "   Consigne : %s\n", col_texte(lg, 7));
        }
        if (lg != NULL)
            sqlite3_finalize(lg);
    }
    for (i = 0; i < LARGEUR_RECU; i++)
        fputc('-', f);
    fputc('\n', f);
    ligne_recu(f, "Sous-total", fmt_montant(sqlite3_column_int64(st, 7)));
    if (express) {
        snprintf(texte, sizeof texte, "Majoration express");
        ligne_recu(f, texte, fmt_montant(sqlite3_column_int64(st, 8)));
    }
    if (remise_pct > 0) {
        snprintf(texte, sizeof texte, "Remise (%d %%)", remise_pct);
        ligne_recu(f, texte, fmt_montant(-sqlite3_column_int64(st, 10)));
    }
    ligne_recu(f, "TOTAL", fmt_montant(total));
    if (taux > 0) {
        snprintf(texte, sizeof texte, "  dont TVA %s", fmt_taux(taux));
        ligne_recu(f, texte,
                   fmt_montant(arrondir((total * taux + (10000 + taux) / 2)
                                        / (10000 + taux))));
    }
    ligne_recu(f, "Payé", fmt_montant(paye));
    if (!annulee)
        ligne_recu(f, "RESTE À PAYER", fmt_montant(total - paye));
    if (col_texte(st, 16)[0] != '\0')
        fprintf(f, "Remarque : %s\n", col_texte(st, 16));
    sqlite3_finalize(st);

    st = req_preparer("SELECT substr(date, 1, 16), mode, montant"
                      " FROM paiements WHERE commande_id = ? ORDER BY id",
                      "i", id);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        snprintf(texte, sizeof texte, "%s %s", col_texte(st, 0),
                 mode_nom(sqlite3_column_int(st, 1)));
        ligne_recu(f, texte, fmt_montant(sqlite3_column_int64(st, 2)));
    }
    if (st != NULL)
        sqlite3_finalize(st);
    for (i = 0; i < LARGEUR_RECU; i++)
        fputc('-', f);
    fputc('\n', f);
    fprintf(f, "Statut : %s\n", statut_nom(statut));
    centre_recu(f, config.pied_recu);
    centre_recu(f, "Présentez ce ticket au retrait.");
    centre_recu(f, numero);
}

/* Affiche le recu et l'enregistre dans recus/<numero>.txt. */
static void imprimer_recu(int id)
{
    char numero[24], chemin[64];
    FILE *f;

    req_texte(numero, sizeof numero,
              "SELECT numero FROM commandes WHERE id = ?", "i", id);
    printf("\n");
    ecrire_recu(stdout, id);
    creer_dossier("recus");
    snprintf(chemin, sizeof chemin, "recus/%s.txt", numero);
    f = fopen(chemin, "w");
    if (f != NULL) {
        ecrire_recu(f, id);
        fclose(f);
        printf("\n(Reçu enregistré dans %s - à imprimer)\n", chemin);
    }
}

/* ---------- Consultation et actions ---------- */

int commande_par_numero(const char *numero)
{
    return (int)req_entier("SELECT id FROM commandes WHERE numero = upper(?)",
                           "t", numero);
}

static Montant reste_a_payer(int id)
{
    return req_entier("SELECT total - paye FROM v_commandes WHERE id = ?",
                      "i", id);
}

int commandes_lister(const char *fin_sql, int param, int ids[], int max)
{
    return lister(fin_sql, param, NULL, ids, max);
}

/* Les ? de fin_sql recoivent texte s'il est fourni, sinon param. */
static int lister(const char *fin_sql, int param, const char *texte,
                  int ids[], int max)
{
    char sql[1024];
    sqlite3_stmt *st;
    int n = 0, i;

    snprintf(sql, sizeof sql,
             "SELECT id, numero, substr(date_depot, 1, 16), client_nom,"
             " statut, annulee, total, total - paye,"
             " (date_prevue < datetime('now','localtime') AND statut < %d"
             "  AND annulee = 0), substr(date_prevue, 1, 16)"
             " FROM v_commandes %s", (int)ST_PRET, fin_sql);
    st = req_preparer(sql, NULL);
    if (st == NULL)
        return 0;
    for (i = 1; i <= sqlite3_bind_parameter_count(st); i++) {
        if (texte != NULL)
            sqlite3_bind_text(st, i, texte, -1, SQLITE_TRANSIENT);
        else
            sqlite3_bind_int(st, i, param);
    }
    printf("%s   Ticket       Déposé le          Client                "
           "Statut      Total           Reste\n", ids != NULL ? "    " : "");
    while (sqlite3_step(st) == SQLITE_ROW && (ids == NULL || n < max)) {
        const char *etat = sqlite3_column_int(st, 5) ? "ANNULÉE"
                         : statut_nom(sqlite3_column_int(st, 4));

        if (ids != NULL) {
            ids[n] = sqlite3_column_int(st, 0);
            printf("%3d.", n + 1);
        }
        printf("%s ", sqlite3_column_int(st, 8) ? " !" : "  ");
        ecrire_col(stdout, col_texte(st, 1), 12, 0);
        printf(" %-16s   ", col_texte(st, 2));
        ecrire_col(stdout, col_texte(st, 3), 20, 0);
        printf("  ");
        ecrire_col(stdout, etat, 10, 0);
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 6)), 14, 1);
        ecrire_col(stdout, sqlite3_column_int(st, 5) ? "-"
                   : fmt_montant(sqlite3_column_int64(st, 7)), 16, 1);
        printf("\n");
        n++;
    }
    sqlite3_finalize(st);
    if (n == 0)
        printf("  (aucune commande)\n");
    return n;
}

static void afficher_commande(int id)
{
    sqlite3_stmt *st;

    titre("Commande");
    ecrire_recu(stdout, id);
    printf("\nHistorique du traitement :\n");
    st = req_preparer("SELECT substr(h.date, 1, 16), h.nouveau,"
                      " COALESCE(u.identifiant, '?') FROM historique_statuts h"
                      " LEFT JOIN utilisateurs u ON u.id = h.utilisateur_id"
                      " WHERE h.commande_id = ? ORDER BY h.id", "i", id);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        printf("  %s  ", col_texte(st, 0));
        ecrire_col(stdout, statut_nom(sqlite3_column_int(st, 1)), 10, 0);
        printf(" par %s\n", col_texte(st, 2));
    }
    if (st != NULL)
        sqlite3_finalize(st);
    if (req_entier("SELECT date_prevue < datetime('now','localtime')"
                   " AND statut < ? AND annulee = 0 FROM commandes"
                   " WHERE id = ?", "ii", (int)ST_PRET, id))
        printf("  !!! EN RETARD : la date de retrait prévue est dépassée.\n");
}

int commande_changer_statut(int id, int nouveau)
{
    int ancien = (int)req_entier("SELECT statut FROM commandes WHERE id = ?",
                                 "i", id);
    char numero[24];

    if (ancien == nouveau)
        return 1;
    base_debut();
    if (!req_executer("UPDATE commandes SET statut = ?,"
                      " date_retrait = CASE WHEN ? = ?"
                      "   THEN datetime('now','localtime') ELSE NULL END"
                      " WHERE id = ?", "iiii", nouveau, nouveau,
                      (int)ST_RECUPERE, id)
        || !req_executer("INSERT INTO historique_statuts (commande_id,"
                         " ancien, nouveau, utilisateur_id)"
                         " VALUES (?, ?, ?, ?)", "iiii",
                         id, ancien, nouveau, session.id)) {
        base_annuler();
        return 0;
    }
    base_valider();
    req_texte(numero, sizeof numero,
              "SELECT numero FROM commandes WHERE id = ?", "i", id);
    journaliser("Commande %s : %s -> %s", numero, statut_nom(ancien),
                statut_nom(nouveau));
    if (nouveau == ST_PRET)
        notification_commande_prete(id);
    return 1;
}

void commande_statut_interactif(int id)
{
    int actuel, annulee, s, suivant, nouveau;

    actuel = (int)req_entier("SELECT statut FROM commandes WHERE id = ?",
                             "i", id);
    annulee = (int)req_entier("SELECT annulee FROM commandes WHERE id = ?",
                              "i", id);
    if (annulee) {
        printf("  Cette commande est annulée.\n");
        return;
    }
    if (actuel == ST_RECUPERE) {
        printf("  Cette commande a déjà été remise au client.\n");
        return;
    }
    printf("Statut actuel : %s\n", statut_nom(actuel));
    for (s = 0; s < ST_RECUPERE; s++)
        printf("  %d. %s%s\n", s, statut_nom(s), s == actuel ? "  <-" : "");
    suivant = actuel < ST_PRET ? actuel + 1 : actuel;
    printf("(La remise au client se fait par l'action « Retrait ».)\n");
    {
        char invite[64];

        snprintf(invite, sizeof invite, "Nouveau statut [%d = %s] : ",
                 suivant, statut_nom(suivant));
        nouveau = lire_entier_defaut(invite, 0, ST_PRET, suivant);
    }
    if (nouveau == actuel) {
        printf("  Statut inchangé.\n");
        return;
    }
    if (commande_changer_statut(id, nouveau))
        printf("  Statut : %s -> %s\n", statut_nom(actuel),
               statut_nom(nouveau));
}

static void encaisser(int id)
{
    Paiement pay[MAX_PAIEMENTS];
    int session_id, n;
    Montant reste = reste_a_payer(id);

    if (req_entier("SELECT annulee FROM commandes WHERE id = ?", "i", id)) {
        printf("  Cette commande est annulée.\n");
        return;
    }
    if (reste <= 0) {
        printf("  Cette commande est déjà entièrement payée.\n");
        return;
    }
    session_id = caisse_exiger_session();
    if (session_id == 0)
        return;
    n = saisir_paiements(reste, pay, MAX_PAIEMENTS);
    if (n == 0)
        return;
    if (enregistrer_paiements(id, session_id, pay, n)) {
        journaliser("Paiement sur la commande n°%d", id);
        printf("  Paiement enregistré. Reste à payer : %s\n",
               fmt_montant(reste_a_payer(id)));
    }
}

static void retrait(int id)
{
    int statut = (int)req_entier("SELECT statut FROM commandes WHERE id = ?",
                                 "i", id);

    if (req_entier("SELECT annulee FROM commandes WHERE id = ?", "i", id)) {
        printf("  Cette commande est annulée.\n");
        return;
    }
    if (statut == ST_RECUPERE) {
        printf("  Déjà remise au client.\n");
        return;
    }
    if (statut != ST_PRET) {
        printf("  La commande n'est pas prête (statut : %s).\n",
               statut_nom(statut));
        if (!autorise(P_ANNULER)
            || !confirmer("Remettre quand même au client ?"))
            return;
    }
    if (reste_a_payer(id) > 0) {
        printf("  Le client doit encore payer %s.\n",
               fmt_montant(reste_a_payer(id)));
        encaisser(id);
        if (reste_a_payer(id) > 0) {
            if (!autorise(P_ANNULER)) {
                printf("  Remise impossible tant que la commande n'est pas "
                       "payée.\n  (Un gérant peut autoriser un crédit.)\n");
                return;
            }
            if (!confirmer("Remettre à crédit (le client paiera plus tard) ?"))
                return;
            journaliser("Commande n°%d remise à crédit (%s dû)", id,
                        fmt_montant(reste_a_payer(id)));
        }
    }
    if (!confirmer("Confirmer la remise du linge au client ?"))
        return;
    if (commande_changer_statut(id, ST_RECUPERE)) {
        req_executer("UPDATE notifications SET statut = 2,"
                     " traite_le = datetime('now','localtime')"
                     " WHERE commande_id = ? AND statut = 0", "i", id);
        printf("  Linge remis au client. Merci !\n");
    }
}

static void annuler(int id)
{
    char motif[160];
    Montant paye;
    int session_id = 0, mode = MODE_ESPECES;

    if (req_entier("SELECT annulee FROM commandes WHERE id = ?", "i", id)) {
        printf("  Déjà annulée.\n");
        return;
    }
    if (req_entier("SELECT statut FROM commandes WHERE id = ?", "i", id)
        == ST_RECUPERE) {
        printf("  Impossible : le linge a déjà été remis au client.\n");
        return;
    }
    paye = req_entier("SELECT paye FROM v_commandes WHERE id = ?", "i", id);
    if (paye > 0) {
        printf("  Le client a déjà payé %s : ce montant sera remboursé.\n",
               fmt_montant(paye));
        session_id = caisse_exiger_session();
        if (session_id == 0)
            return;
        printf("Mode de remboursement :\n");
        mode = choisir_mode_paiement();
    }
    lire_obligatoire("Motif de l'annulation : ", motif, sizeof motif);
    if (!confirmer("Confirmer l'annulation ?"))
        return;
    base_debut();
    if ((paye > 0
         && !req_executer("INSERT INTO paiements (commande_id, session_id,"
                          " mode, montant, utilisateur_id)"
                          " VALUES (?, ?, ?, ?, ?)", "iiili", id, session_id,
                          mode, -paye, session.id))
        || !req_executer("UPDATE commandes SET annulee = 1,"
                         " motif_annulation = ? WHERE id = ?", "ti",
                         motif, id)
        || !req_executer("UPDATE notifications SET statut = 2"
                         " WHERE commande_id = ? AND statut = 0", "i", id)) {
        base_annuler();
        printf("  Erreur : annulation impossible.\n");
        return;
    }
    base_valider();
    journaliser("Commande n°%d annulée (%s), remboursé %s", id, motif,
                fmt_montant(paye));
    printf("  Commande annulée.%s\n",
           paye > 0 ? " Remboursement enregistré en caisse." : "");
}

void commande_detail(int id)
{
    int choix;

    for (;;) {
        afficher_commande(id);
        printf("\n1. Encaisser un paiement   2. Changer le statut   "
               "3. Retrait (remise au client)\n4. Réimprimer le reçu      "
               "5. Annuler la commande    0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 5);
        switch (choix) {
        case 0:
            return;
        case 1:
            if (exiger(P_VENTE))
                encaisser(id);
            break;
        case 2:
            if (exiger(P_TRAITEMENT))
                commande_statut_interactif(id);
            break;
        case 3:
            if (exiger(P_VENTE))
                retrait(id);
            break;
        case 4:
            imprimer_recu(id);
            break;
        case 5:
            if (exiger(P_ANNULER))
                annuler(id);
            break;
        }
        pause_console();
    }
}

static void choisir_dans_liste(const char *titre_liste, const char *fin_sql,
                               const char *texte)
{
    int ids[MAX_LISTE], n, choix;

    titre(titre_liste);
    n = lister(fin_sql, 0, texte, ids, MAX_LISTE);
    if (n == 0) {
        pause_console();
        return;
    }
    printf("(« ! » = en retard)\n");
    choix = lire_entier("N° dans la liste (0 = retour) : ", 0, n);
    if (choix > 0)
        commande_detail(ids[choix - 1]);
}

static void rechercher(void)
{
    char texte[80], motif[90];
    int id;

    if (lire_ligne("N° de ticket, nom ou téléphone du client : ", texte,
                   sizeof texte) == 0)
        return;
    id = commande_par_numero(texte);
    if (id != 0) {
        commande_detail(id);
        return;
    }
    snprintf(motif, sizeof motif, "%%%s%%", texte);
    choisir_dans_liste("Résultats de la recherche",
                       "WHERE numero LIKE ?1 OR client_nom LIKE ?1"
                       " OR client_tel LIKE ?1 ORDER BY id DESC LIMIT 40",
                       motif);
}

void menu_commandes(void)
{
    char sql[160];
    int choix;

    for (;;) {
        titre("Commandes (tickets)");
        printf("1. Rechercher (ticket, nom, téléphone)\n");
        printf("2. Commandes du jour\n");
        printf("3. Prêtes, à remettre au client\n");
        printf("4. Commandes en cours (atelier)\n");
        printf("5. Commandes avec un reste à payer\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 5);
        switch (choix) {
        case 0:
            return;
        case 1:
            rechercher();
            break;
        case 2:
            choisir_dans_liste("Commandes du jour",
                               "WHERE date(date_depot) ="
                               " date('now','localtime') ORDER BY id DESC",
                               NULL);
            break;
        case 3:
            snprintf(sql, sizeof sql, "WHERE statut = %d AND annulee = 0"
                     " ORDER BY date_prevue", (int)ST_PRET);
            choisir_dans_liste("Prêtes à retirer", sql, NULL);
            break;
        case 4:
            snprintf(sql, sizeof sql, "WHERE statut < %d AND annulee = 0"
                     " ORDER BY date_prevue", (int)ST_PRET);
            choisir_dans_liste("Commandes en cours", sql, NULL);
            break;
        case 5:
            choisir_dans_liste("Reste à payer",
                               "WHERE annulee = 0 AND total - paye > 0"
                               " ORDER BY date_depot", NULL);
            break;
        }
    }
}
