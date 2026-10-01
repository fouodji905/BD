#include <stdio.h>
#include <string.h>

#include "base.h"
#include "caisse.h"
#include "clients.h"
#include "commandes.h"
#include "console.h"
#include "notifications.h"
#include "parametres.h"
#include "securite.h"
#include "tableau_bord.h"

/* Prototypes des fonctions internes au module. */
static void tableau_du_jour(void);
static void chiffre_affaires(void);
static void lire_periode(char debut[11], char fin[11]);
static void classements(void);
static void performance_employes(void);
static void exporter_commandes(void);

static void tableau_du_jour(void)
{
    int s, session_id;
    long long nb;

    titre("Tableau de bord du jour");
    printf("Encaissé aujourd'hui        : %s\n",
           fmt_montant(req_entier("SELECT COALESCE(SUM(montant), 0)"
                                  " FROM paiements WHERE date(date) ="
                                  " date('now','localtime')", NULL)));
    nb = req_entier("SELECT COUNT(*) FROM commandes WHERE annulee = 0"
                    " AND date(date_depot) = date('now','localtime')", NULL);
    printf("Commandes du jour           : %lld (facturé %s)\n", nb,
           fmt_montant(req_entier("SELECT COALESCE(SUM(total), 0)"
                                  " FROM commandes WHERE annulee = 0 AND"
                                  " date(date_depot) ="
                                  " date('now','localtime')", NULL)));
    printf("Articles reçus aujourd'hui  : %lld\n",
           req_entier("SELECT COALESCE(SUM(l.quantite), 0)"
                      " FROM lignes_commande l JOIN commandes c"
                      " ON c.id = l.commande_id WHERE c.annulee = 0 AND"
                      " date(c.date_depot) = date('now','localtime')", NULL));
    printf("Retraits aujourd'hui        : %lld\n",
           req_entier("SELECT COUNT(*) FROM commandes WHERE"
                      " date(date_retrait) = date('now','localtime')", NULL));

    printf("\nAtelier :\n");
    for (s = 0; s < ST_RECUPERE; s++) {
        printf("  ");
        ecrire_col(stdout, statut_nom(s), 10, 0);
        printf(" : %lld\n",
               req_entier("SELECT COUNT(*) FROM commandes WHERE statut = ?"
                          " AND annulee = 0", "i", s));
    }
    printf("  EN RETARD  : %lld\n",
           req_entier("SELECT COUNT(*) FROM commandes WHERE statut < ? AND"
                      " annulee = 0 AND date_prevue <"
                      " datetime('now','localtime')", "i", (int)ST_PRET));

    printf("\nCréances clients (reste dû)  : %s\n",
           fmt_montant(req_entier("SELECT COALESCE(SUM(total - paye), 0)"
                                  " FROM v_commandes WHERE annulee = 0", NULL)));
    printf("Messages clients en attente  : %d\n", notifications_en_attente());
    session_id = caisse_session_ouverte();
    if (session_id != 0)
        printf("Caisse                       : ouverte (session n°%d)\n",
               session_id);
    else
        printf("Caisse                       : fermée\n");
    nb = req_entier("SELECT COUNT(*) FROM commandes WHERE annulee = 0 AND"
                    " date_depot >= date('now','localtime','start of month')",
                    NULL);
    if (nb > 0)
        printf("Panier moyen du mois         : %s (%lld commandes)\n",
               fmt_montant(req_entier("SELECT SUM(total) FROM commandes"
                                      " WHERE annulee = 0 AND date_depot >="
                                      " date('now','localtime',"
                                      "'start of month')", NULL) / nb), nb);
}

static void chiffre_affaires(void)
{
    sqlite3_stmt *st;
    int i;

    titre("Chiffre d'affaires - 7 derniers jours");
    printf("Jour          Commandes   Facturé           Encaissé\n");
    for (i = 6; i >= 0; i--) {
        char decalage[24], jour[16];

        snprintf(decalage, sizeof decalage, "-%d days", i);
        req_texte(jour, sizeof jour, "SELECT date('now','localtime', ?)",
                  "t", decalage);
        printf("%-12s  %9lld   ", jour,
               req_entier("SELECT COUNT(*) FROM commandes WHERE annulee = 0"
                          " AND date(date_depot) = ?", "t", jour));
        ecrire_col(stdout, fmt_montant(req_entier(
            "SELECT COALESCE(SUM(total), 0) FROM commandes WHERE annulee = 0"
            " AND date(date_depot) = ?", "t", jour)), 16, 1);
        printf("  ");
        ecrire_col(stdout, fmt_montant(req_entier(
            "SELECT COALESCE(SUM(montant), 0) FROM paiements"
            " WHERE date(date) = ?", "t", jour)), 16, 1);
        printf("\n");
    }

    titre("Chiffre d'affaires - 12 derniers mois");
    printf("Mois       Commandes   Facturé           Encaissé\n");
    st = req_preparer(
        "WITH RECURSIVE mois(m, n) AS ("
        "  SELECT strftime('%Y-%m', 'now', 'localtime'), 0"
        "  UNION ALL SELECT strftime('%Y-%m', date('now','localtime',"
        "    'start of month', '-' || (n + 1) || ' months')), n + 1"
        "  FROM mois WHERE n < 11)"
        " SELECT m,"
        "  (SELECT COUNT(*) FROM commandes WHERE annulee = 0"
        "   AND strftime('%Y-%m', date_depot) = m),"
        "  (SELECT COALESCE(SUM(total), 0) FROM commandes WHERE annulee = 0"
        "   AND strftime('%Y-%m', date_depot) = m),"
        "  (SELECT COALESCE(SUM(montant), 0) FROM paiements"
        "   WHERE strftime('%Y-%m', date) = m)"
        " FROM mois ORDER BY m", NULL);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        printf("%-9s  %9d   ", col_texte(st, 0), sqlite3_column_int(st, 1));
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 16, 1);
        printf("  ");
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 3)), 16, 1);
        printf("\n");
    }
    if (st != NULL)
        sqlite3_finalize(st);
}

/* Periode AAAA-MM-JJ ; par defaut le mois en cours. */
static void lire_periode(char debut[11], char fin[11])
{
    char saisie[32], defaut[11];

    req_texte(defaut, 11, "SELECT date('now','localtime','start of month')",
              NULL);
    for (;;) {
        printf("Du (AAAA-MM-JJ) [%s] : ", defaut);
        if (lire_ligne("", saisie, sizeof saisie) == 0)
            copier(saisie, defaut, sizeof saisie);
        if (req_texte(debut, 11, "SELECT date(?)", "t", saisie)
            && debut[0] != '\0')
            break;
        printf("  Date invalide.\n");
    }
    req_texte(defaut, 11, "SELECT date('now','localtime')", NULL);
    for (;;) {
        printf("Au (AAAA-MM-JJ) [%s] : ", defaut);
        if (lire_ligne("", saisie, sizeof saisie) == 0)
            copier(saisie, defaut, sizeof saisie);
        if (req_texte(fin, 11, "SELECT date(?)", "t", saisie)
            && fin[0] != '\0')
            break;
        printf("  Date invalide.\n");
    }
}

static void classements(void)
{
    sqlite3_stmt *st;
    char debut[11], fin[11];
    int rang = 0;

    titre("Top services et top clients");
    lire_periode(debut, fin);

    printf("\nTop 10 des services (du %s au %s)\n", debut, fin);
    printf("     Service                         Pièces   Chiffre d'affaires\n");
    st = req_preparer("SELECT l.libelle, SUM(l.quantite), SUM(l.montant)"
                      " FROM lignes_commande l JOIN commandes c"
                      " ON c.id = l.commande_id WHERE c.annulee = 0"
                      " AND date(c.date_depot) BETWEEN ? AND ?"
                      " GROUP BY l.libelle ORDER BY 3 DESC LIMIT 10",
                      "tt", debut, fin);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        printf("  %2d. ", ++rang);
        ecrire_col(stdout, col_texte(st, 0), 30, 0);
        printf(" %7d   ", sqlite3_column_int(st, 1));
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 18, 1);
        printf("\n");
    }
    if (st != NULL)
        sqlite3_finalize(st);

    rang = 0;
    printf("\nTop 10 des clients (du %s au %s)\n", debut, fin);
    printf("     Client                          Commandes   Total\n");
    st = req_preparer("SELECT client_nom, COUNT(*), SUM(total)"
                      " FROM v_commandes WHERE annulee = 0"
                      " AND date(date_depot) BETWEEN ? AND ?"
                      " GROUP BY client_id ORDER BY 3 DESC LIMIT 10",
                      "tt", debut, fin);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        printf("  %2d. ", ++rang);
        ecrire_col(stdout, col_texte(st, 0), 30, 0);
        printf(" %9d   ", sqlite3_column_int(st, 1));
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 18, 1);
        printf("\n");
    }
    if (st != NULL)
        sqlite3_finalize(st);
    printf("\nTaux de retard sur la période : ");
    {
        long long total = req_entier(
            "SELECT COUNT(*) FROM commandes WHERE annulee = 0"
            " AND date(date_depot) BETWEEN ? AND ?", "tt", debut, fin);
        long long retards = req_entier(
            "SELECT COUNT(*) FROM commandes c WHERE c.annulee = 0"
            " AND date(c.date_depot) BETWEEN ? AND ?"
            " AND COALESCE((SELECT MIN(h.date) FROM historique_statuts h"
            "   WHERE h.commande_id = c.id AND h.nouveau >= ?),"
            "   datetime('now','localtime')) > c.date_prevue",
            "tti", debut, fin, (int)ST_PRET);

        if (total == 0)
            printf("aucune commande\n");
        else
            printf("%lld %% (%lld sur %lld commandes prêtes après la date "
                   "prévue)\n", retards * 100 / total, retards, total);
    }
}

static void performance_employes(void)
{
    sqlite3_stmt *st;
    char debut[11], fin[11];

    titre("Performance par employé");
    lire_periode(debut, fin);
    printf("\nEmployé              Commandes   Facturé           "
           "Encaissé          Étapes atelier\n");
    st = req_preparer(
        "SELECT u.nom,"
        " (SELECT COUNT(*) FROM commandes c WHERE c.utilisateur_id = u.id"
        "  AND c.annulee = 0 AND date(c.date_depot) BETWEEN ?1 AND ?2),"
        " (SELECT COALESCE(SUM(total), 0) FROM commandes c"
        "  WHERE c.utilisateur_id = u.id AND c.annulee = 0"
        "  AND date(c.date_depot) BETWEEN ?1 AND ?2),"
        " (SELECT COALESCE(SUM(montant), 0) FROM paiements p"
        "  WHERE p.utilisateur_id = u.id AND date(p.date) BETWEEN ?1 AND ?2),"
        " (SELECT COUNT(*) FROM historique_statuts h"
        "  WHERE h.utilisateur_id = u.id AND h.ancien IS NOT NULL"
        "  AND date(h.date) BETWEEN ?1 AND ?2)"
        " FROM utilisateurs u ORDER BY 3 DESC", "tt", debut, fin);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        ecrire_col(stdout, col_texte(st, 0), 20, 0);
        printf(" %9d   ", sqlite3_column_int(st, 1));
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 16, 1);
        printf("  ");
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 3)), 16, 1);
        printf("  %9d\n", sqlite3_column_int(st, 4));
    }
    if (st != NULL)
        sqlite3_finalize(st);
}

static void exporter_commandes(void)
{
    sqlite3_stmt *st;
    char debut[11], fin[11], chemin[96];
    FILE *f;
    int n = 0, i;

    titre("Export des commandes (CSV / Excel)");
    lire_periode(debut, fin);
    creer_dossier("exports");
    snprintf(chemin, sizeof chemin, "exports/commandes_%s_%s.csv", debut, fin);
    f = fopen(chemin, "wb");
    if (f == NULL) {
        printf("  Impossible de créer %s\n", chemin);
        return;
    }
    fputs("\xEF\xBB\xBFTicket;Date dépôt;Date prévue;Date retrait;Client;"
          "Téléphone;Statut;Annulée;Express;Sous-total;Majoration;Remise;"
          "Total;Payé;Reste\r\n", f);
    st = req_preparer("SELECT numero, date_depot, date_prevue,"
                      " COALESCE(date_retrait, ''), client_nom, client_tel,"
                      " statut, annulee, express, sous_total, majoration,"
                      " remise, total, paye FROM v_commandes"
                      " WHERE date(date_depot) BETWEEN ? AND ? ORDER BY id",
                      "tt", debut, fin);
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
        for (i = 0; i < 6; i++)
            csv_champ(f, col_texte(st, i), 0);
        csv_champ(f, statut_nom(sqlite3_column_int(st, 6)), 0);
        csv_champ(f, sqlite3_column_int(st, 7) ? "oui" : "non", 0);
        csv_champ(f, sqlite3_column_int(st, 8) ? "oui" : "non", 0);
        for (i = 9; i <= 13; i++)
            csv_montant(f, sqlite3_column_int64(st, i), 0);
        csv_montant(f, sqlite3_column_int(st, 7) ? 0
                    : sqlite3_column_int64(st, 12)
                      - sqlite3_column_int64(st, 13), 1);
        n++;
    }
    if (st != NULL)
        sqlite3_finalize(st);
    fclose(f);
    journaliser("Export de %d commandes (%s au %s)", n, debut, fin);
    printf("  %d commande(s) exportée(s) dans %s\n", n, chemin);
}

void menu_tableau_bord(void)
{
    int choix;

    for (;;) {
        titre("Tableaux de bord et rapports");
        printf("1. Tableau de bord du jour\n");
        printf("2. Chiffre d'affaires (jours et mois)\n");
        printf("3. Top services, top clients, taux de retard\n");
        printf("4. Performance par employé\n");
        printf("5. Exporter les commandes (CSV / Excel)\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 5);
        switch (choix) {
        case 0: return;
        case 1: tableau_du_jour(); break;
        case 2: chiffre_affaires(); break;
        case 3: classements(); break;
        case 4: performance_employes(); break;
        case 5: exporter_commandes(); break;
        }
        pause_console();
    }
}
