#include <stdio.h>

#include "base.h"
#include "commandes.h"
#include "console.h"
#include "traitement.h"

#define MAX_LISTE 60

/* Prototypes des fonctions internes au module. */
static void vue_par_statut(void);
static void avancer_une(void);
static void avancer_en_lot(void);
static void en_retard(void);

static void vue_par_statut(void)
{
    char sql[128];
    int s, ids[MAX_LISTE], n, choix;

    titre("Atelier : commandes par statut");
    for (s = 0; s < ST_RECUPERE; s++) {
        printf("  %d. ", s);
        ecrire_col(stdout, statut_nom(s), 10, 0);
        printf(" : %lld\n",
               req_entier("SELECT COUNT(*) FROM commandes WHERE statut = ?"
                          " AND annulee = 0", "i", s));
    }
    s = lire_entier_defaut("Voir quel statut ? (Entrée = retour) : ",
                           0, ST_PRET, -1);
    if (s < 0)
        return;
    snprintf(sql, sizeof sql, "WHERE statut = %d AND annulee = 0"
             " ORDER BY date_prevue", s);
    titre(statut_nom(s));
    n = commandes_lister(sql, 0, ids, MAX_LISTE);
    if (n == 0) {
        pause_console();
        return;
    }
    choix = lire_entier("N° dans la liste (0 = retour) : ", 0, n);
    if (choix > 0)
        commande_detail(ids[choix - 1]);
}

/* Saisie ou scan du ticket, puis passage au statut suivant. */
static void avancer_une(void)
{
    char numero[32];
    int id;

    titre("Changer le statut d'une commande");
    for (;;) {
        if (lire_ligne("N° de ticket (scan ou saisie, vide = retour) : ",
                       numero, sizeof numero) == 0)
            return;
        id = commande_par_numero(numero);
        if (id == 0) {
            printf("  Ticket introuvable.\n");
            continue;
        }
        printf("Client : ");
        {
            char nom[80];

            req_texte(nom, sizeof nom,
                      "SELECT client_nom FROM v_commandes WHERE id = ?",
                      "i", id);
            printf("%s\n", nom);
        }
        commande_statut_interactif(id);
    }
}

/* Mode lot : on choisit l'etape puis on scanne les tickets a la suite. */
static void avancer_en_lot(void)
{
    char numero[32];
    int cible, id, n = 0, s, actuel;

    titre("Changement de statut en lot");
    for (s = 0; s <= ST_PRET; s++)
        printf("  %d. %s\n", s, statut_nom(s));
    cible = lire_entier("Mettre les tickets au statut : ", 0, ST_PRET);
    printf("Scannez ou tapez les tickets un par un (vide = terminer).\n");
    for (;;) {
        if (lire_ligne("Ticket : ", numero, sizeof numero) == 0)
            break;
        id = commande_par_numero(numero);
        if (id == 0) {
            printf("  Ticket introuvable.\n");
            continue;
        }
        if (req_entier("SELECT annulee FROM commandes WHERE id = ?", "i", id)) {
            printf("  Commande annulée : ignorée.\n");
            continue;
        }
        actuel = (int)req_entier("SELECT statut FROM commandes WHERE id = ?",
                                 "i", id);
        if (actuel == ST_RECUPERE) {
            printf("  Déjà remise au client : ignorée.\n");
            continue;
        }
        if (commande_changer_statut(id, cible)) {
            printf("  OK : %s -> %s\n", statut_nom(actuel), statut_nom(cible));
            n++;
        }
    }
    printf("  %d commande(s) mise(s) à jour.\n", n);
    pause_console();
}

static void en_retard(void)
{
    char sql[160];
    int ids[MAX_LISTE], n, choix;

    titre("Commandes en retard");
    snprintf(sql, sizeof sql, "WHERE statut < %d AND annulee = 0 AND"
             " date_prevue < datetime('now','localtime') ORDER BY date_prevue",
             (int)ST_PRET);
    n = commandes_lister(sql, 0, ids, MAX_LISTE);
    if (n == 0) {
        pause_console();
        return;
    }
    choix = lire_entier("N° dans la liste (0 = retour) : ", 0, n);
    if (choix > 0)
        commande_detail(ids[choix - 1]);
}

void menu_traitement(void)
{
    int choix;

    for (;;) {
        titre("Suivi du traitement (atelier)");
        printf("Étapes : Reçu > Tri > Lavage > Séchage > Repassage > "
               "Contrôle > Prêt > Récupéré\n\n");
        printf("1. Vue par statut\n");
        printf("2. Changer le statut d'un ticket (scan / saisie)\n");
        printf("3. Changement en lot (plusieurs tickets d'un coup)\n");
        printf("4. Commandes en retard\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 4);
        switch (choix) {
        case 0: return;
        case 1: vue_par_statut(); break;
        case 2: avancer_une(); break;
        case 3: avancer_en_lot(); break;
        case 4: en_retard(); break;
        }
    }
}
