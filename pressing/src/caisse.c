#include <stdio.h>
#include <string.h>

#include "caisse.h"
#include "console.h"
#include "parametres.h"
#include "securite.h"

/* Prototypes des fonctions internes au module. */
static int ouvrir_caisse(void);
static Montant especes_theoriques(int session_id);
static void etat_caisse(FILE *f, int session_id);
static void mouvement(int sens);
static void fermer_caisse(void);
static void historique_sessions(void);
static void afficher_mouvements(int session_id);

const char *mode_nom(int mode)
{
    switch (mode) {
    case MODE_ESPECES:      return "Espèces";
    case MODE_MOBILE_MONEY: return "Mobile Money";
    case MODE_CARTE:        return "Carte bancaire";
    case MODE_VIREMENT:     return "Virement / chèque";
    }
    return "?";
}

int choisir_mode_paiement(void)
{
    int m;

    for (m = 1; m <= NB_MODES; m++)
        printf("  %d. %s\n", m, mode_nom(m));
    return lire_entier_defaut("Mode de paiement [1] : ", 1, NB_MODES, 1);
}

int caisse_session_ouverte(void)
{
    return (int)req_entier("SELECT id FROM sessions_caisse"
                           " WHERE fermeture IS NULL ORDER BY id DESC LIMIT 1",
                           NULL);
}

static int ouvrir_caisse(void)
{
    Montant fond;
    int id;

    if ((id = caisse_session_ouverte()) != 0) {
        printf("  La caisse est déjà ouverte (session n°%d).\n", id);
        return id;
    }
    titre("Ouverture de caisse");
    fond = lire_montant("Fond de caisse (espèces dans le tiroir) : ");
    if (!req_executer("INSERT INTO sessions_caisse (utilisateur_id,"
                      " fond_initial) VALUES (?, ?)", "il", session.id, fond))
        return 0;
    id = (int)base_dernier_id();
    journaliser("Ouverture de caisse n°%d, fond %s", id, fmt_montant(fond));
    printf("  Caisse ouverte (session n°%d).\n", id);
    return id;
}

int caisse_exiger_session(void)
{
    int id = caisse_session_ouverte();

    if (id != 0)
        return id;
    printf("\n  La caisse est fermée.\n");
    if (!autorise(P_CAISSE)) {
        printf("  Demandez à un responsable d'ouvrir la caisse.\n");
        return 0;
    }
    if (!confirmer("Ouvrir la caisse maintenant ?"))
        return 0;
    return ouvrir_caisse();
}

static Montant especes_theoriques(int session_id)
{
    return req_entier("SELECT fond_initial FROM sessions_caisse WHERE id = ?",
                      "i", session_id)
         + req_entier("SELECT COALESCE(SUM(montant), 0) FROM paiements"
                      " WHERE session_id = ? AND mode = ?", "ii",
                      session_id, MODE_ESPECES)
         + req_entier("SELECT COALESCE(SUM(sens * montant), 0)"
                      " FROM mouvements_caisse WHERE session_id = ?",
                      "i", session_id);
}

/* Ecrit l'etat de la caisse dans f (ecran ou fichier). */
static void etat_caisse(FILE *f, int session_id)
{
    char ouverture[32], caissier[64];
    int m;

    req_texte(ouverture, sizeof ouverture,
              "SELECT ouverture FROM sessions_caisse WHERE id = ?",
              "i", session_id);
    req_texte(caissier, sizeof caissier,
              "SELECT u.nom FROM sessions_caisse s JOIN utilisateurs u"
              " ON u.id = s.utilisateur_id WHERE s.id = ?", "i", session_id);
    fprintf(f, "Session n°%d ouverte le %s par %s\n", session_id, ouverture,
            caissier);
    fprintf(f, "Fond initial              : %s\n",
            fmt_montant(req_entier("SELECT fond_initial FROM sessions_caisse"
                                   " WHERE id = ?", "i", session_id)));
    fprintf(f, "Encaissements par mode :\n");
    for (m = 1; m <= NB_MODES; m++) {
        Montant total = req_entier("SELECT COALESCE(SUM(montant), 0)"
                                   " FROM paiements WHERE session_id = ?"
                                   " AND mode = ?", "ii", session_id, m);
        fprintf(f, "  ");
        ecrire_col(f, mode_nom(m), 24, 0);
        fprintf(f, ": %s\n", fmt_montant(total));
    }
    fprintf(f, "Total encaissé            : %s\n",
            fmt_montant(req_entier("SELECT COALESCE(SUM(montant), 0)"
                                   " FROM paiements WHERE session_id = ?",
                                   "i", session_id)));
    fprintf(f, "Entrées diverses          : %s\n",
            fmt_montant(req_entier("SELECT COALESCE(SUM(montant), 0)"
                                   " FROM mouvements_caisse WHERE"
                                   " session_id = ? AND sens = 1",
                                   "i", session_id)));
    fprintf(f, "Dépenses / sorties        : %s\n",
            fmt_montant(req_entier("SELECT COALESCE(SUM(montant), 0)"
                                   " FROM mouvements_caisse WHERE"
                                   " session_id = ? AND sens = -1",
                                   "i", session_id)));
    fprintf(f, "Commandes enregistrées    : %lld\n",
            req_entier("SELECT COUNT(DISTINCT commande_id) FROM paiements"
                       " WHERE session_id = ?", "i", session_id));
    fprintf(f, "ESPÈCES ATTENDUES EN CAISSE : %s\n",
            fmt_montant(especes_theoriques(session_id)));
}

static void afficher_mouvements(int session_id)
{
    sqlite3_stmt *st;

    st = req_preparer("SELECT date, sens, montant, motif FROM mouvements_caisse"
                      " WHERE session_id = ? ORDER BY id", "i", session_id);
    if (st == NULL)
        return;
    printf("\nMouvements de la session :\n");
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("  %s  %s ", col_texte(st, 0),
               sqlite3_column_int(st, 1) > 0 ? "Entrée " : "Sortie ");
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 16, 1);
        printf("  %s\n", col_texte(st, 3));
    }
    sqlite3_finalize(st);
}

static void mouvement(int sens)
{
    char motif[120];
    Montant montant;
    int session_id = caisse_session_ouverte();

    if (session_id == 0) {
        printf("  La caisse est fermée.\n");
        return;
    }
    titre(sens > 0 ? "Entrée d'argent" : "Dépense / sortie d'argent");
    montant = lire_montant("Montant : ");
    if (montant <= 0) {
        printf("  Montant nul : rien n'est enregistré.\n");
        return;
    }
    if (sens < 0 && montant > especes_theoriques(session_id)) {
        printf("  Attention : la caisse ne contient que %s en espèces.\n",
               fmt_montant(especes_theoriques(session_id)));
        if (!confirmer("Enregistrer quand même ?"))
            return;
    }
    lire_obligatoire(sens > 0 ? "Motif (ex. apport monnaie) : "
                              : "Motif (ex. achat lessive, transport) : ",
                     motif, sizeof motif);
    req_executer("INSERT INTO mouvements_caisse (session_id, sens, montant,"
                 " motif, utilisateur_id) VALUES (?, ?, ?, ?, ?)",
                 "iilti", session_id, sens, montant, motif, session.id);
    journaliser("%s de caisse %s : %s", sens > 0 ? "Entrée" : "Sortie",
                fmt_montant(montant), motif);
    printf("  Enregistré.\n");
}

static void fermer_caisse(void)
{
    char commentaire[200] = "", chemin[64];
    int session_id = caisse_session_ouverte();
    Montant theorique, compte, ecart;
    FILE *f;

    if (session_id == 0) {
        printf("  La caisse est déjà fermée.\n");
        return;
    }
    titre("Fermeture de caisse");
    etat_caisse(stdout, session_id);
    theorique = especes_theoriques(session_id);
    printf("\n");
    compte = lire_montant("Espèces réellement comptées dans le tiroir : ");
    ecart = compte - theorique;
    if (ecart != 0) {
        printf("  ÉCART : %s (%s)\n", fmt_montant(ecart),
               ecart > 0 ? "excédent" : "manquant");
        lire_ligne("Explication de l'écart : ", commentaire,
                   sizeof commentaire);
    } else {
        printf("  Caisse juste, aucun écart.\n");
    }
    if (!confirmer("Confirmer la fermeture ?"))
        return;
    req_executer("UPDATE sessions_caisse SET fermeture ="
                 " datetime('now','localtime'), ferme_par = ?,"
                 " montant_theorique = ?, montant_compte = ?, ecart = ?,"
                 " commentaire = ? WHERE id = ?", "illlti",
                 session.id, theorique, compte, ecart, commentaire,
                 session_id);
    journaliser("Fermeture de caisse n°%d, écart %s", session_id,
                fmt_montant(ecart));

    /* Rapport de cloture (ticket Z) dans un fichier. */
    creer_dossier("recus");
    snprintf(chemin, sizeof chemin, "recus/cloture_caisse_%d.txt",
             session_id);
    f = fopen(chemin, "w");
    if (f != NULL) {
        fprintf(f, "%s\nCLÔTURE DE CAISSE\n", config.nom);
        etat_caisse(f, session_id);
        fprintf(f, "Espèces comptées           : %s\n", fmt_montant(compte));
        fprintf(f, "Écart                      : %s\n", fmt_montant(ecart));
        if (commentaire[0] != '\0')
            fprintf(f, "Commentaire : %s\n", commentaire);
        fprintf(f, "Fermée par %s\n", session.nom);
        fclose(f);
        printf("  Rapport de clôture enregistré : %s\n", chemin);
    }
    printf("  Caisse fermée.\n");
}

static void historique_sessions(void)
{
    sqlite3_stmt *st;

    titre("Historique des sessions de caisse (20 dernières)");
    st = req_preparer("SELECT s.id, s.ouverture, COALESCE(s.fermeture, '"
                      "en cours'), u.identifiant, s.montant_theorique,"
                      " s.montant_compte, s.ecart, s.fermeture IS NULL"
                      " FROM sessions_caisse s JOIN utilisateurs u"
                      " ON u.id = s.utilisateur_id ORDER BY s.id DESC LIMIT 20",
                      NULL);
    if (st == NULL)
        return;
    printf("N°   Ouverture            Fermeture            Par         "
           "      Attendu          Compté           Écart\n");
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%-4d %-20s %-20s ", sqlite3_column_int(st, 0),
               col_texte(st, 1), col_texte(st, 2));
        ecrire_col(stdout, col_texte(st, 3), 10, 0);
        if (sqlite3_column_int(st, 7)) {
            printf("\n");
            continue;
        }
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 4)), 16, 1);
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 5)), 16, 1);
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 6)), 16, 1);
        printf("\n");
    }
    sqlite3_finalize(st);
}

void menu_caisse(void)
{
    int choix, session_id;

    for (;;) {
        session_id = caisse_session_ouverte();
        titre("Caisse et trésorerie");
        if (session_id != 0)
            printf("Caisse OUVERTE (session n°%d) - espèces attendues : %s\n\n",
                   session_id, fmt_montant(especes_theoriques(session_id)));
        else
            printf("Caisse FERMÉE\n\n");
        printf("1. Ouvrir la caisse\n");
        printf("2. État de la caisse\n");
        printf("3. Enregistrer une dépense / sortie d'argent\n");
        printf("4. Enregistrer une entrée d'argent\n");
        printf("5. Fermer la caisse (comptage et écart)\n");
        printf("6. Historique des sessions\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 6);
        switch (choix) {
        case 0:
            return;
        case 1:
            ouvrir_caisse();
            break;
        case 2:
            if (session_id == 0) {
                printf("  La caisse est fermée.\n");
                break;
            }
            titre("État de la caisse");
            etat_caisse(stdout, session_id);
            afficher_mouvements(session_id);
            break;
        case 3:
            mouvement(-1);
            break;
        case 4:
            mouvement(1);
            break;
        case 5:
            fermer_caisse();
            break;
        case 6:
            historique_sessions();
            break;
        }
        pause_console();
    }
}
