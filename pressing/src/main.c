/* SmartPress - gestion de pressing en C (version console + SQLite).
   Modules du MVP : authentification, roles, utilisateurs, parametres,
   clients, catalogue, point de vente, cycle de traitement, caisse,
   notifications de base et tableau de bord. */
#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "caisse.h"
#include "catalogue.h"
#include "clients.h"
#include "commandes.h"
#include "console.h"
#include "notifications.h"
#include "parametres.h"
#include "securite.h"
#include "tableau_bord.h"
#include "traitement.h"
#include "utilisateurs.h"

#define FICHIER_BASE "smartpress.db"

typedef struct {
    const char *libelle;
    int permission;          /* 0 = accessible a tous */
    void (*action)(void);
} EntreeMenu;

static const EntreeMenu MENU[] = {
    { "Nouvelle commande (caisse)", P_VENTE, commande_nouvelle },
    { "Commandes : recherche, paiement, retrait", P_VENTE | P_TRAITEMENT,
      menu_commandes },
    { "Suivi du traitement (atelier)", P_TRAITEMENT, menu_traitement },
    { "Clients", P_CLIENTS, menu_clients },
    { "Caisse et trésorerie", P_CAISSE, menu_caisse },
    { "Catalogue et tarifs", P_VENTE | P_CATALOGUE, menu_catalogue },
    { "Notifications clients", P_VENTE, menu_notifications },
    { "Tableaux de bord et rapports", P_RAPPORTS, menu_tableau_bord },
    { "Utilisateurs et sécurité", P_UTILISATEURS, menu_utilisateurs },
    { "Paramètres", P_PARAMETRES, menu_parametres },
    { "Changer mon mot de passe", 0, changer_mon_mot_de_passe }
};

#define NB_ENTREES ((int)(sizeof MENU / sizeof MENU[0]))

/* Prototypes des fonctions internes au module. */
static int accessible(const EntreeMenu *e);
static void menu_principal(void);

static int accessible(const EntreeMenu *e)
{
    return e->permission == 0 || autorise((Permission)e->permission);
}

static void menu_principal(void)
{
    int i, choix, session_caisse, attente;
    long long retards;

    for (;;) {
        titre(config.nom);
        session_caisse = caisse_session_ouverte();
        printf("Connecté : %s (%s)   |   Caisse : %s\n", session.nom,
               role_nom(session.role),
               session_caisse ? "ouverte" : "fermée");
        retards = req_entier("SELECT COUNT(*) FROM commandes WHERE statut < ?"
                             " AND annulee = 0 AND date_prevue <"
                             " datetime('now','localtime')", "i",
                             (int)ST_PRET);
        attente = notifications_en_attente();
        if (retards > 0)
            printf("  ! %lld commande(s) en retard\n", retards);
        if (attente > 0 && autorise(P_VENTE))
            printf("  ! %d message(s) client à envoyer\n", attente);
        printf("\n");
        for (i = 0; i < NB_ENTREES; i++)
            if (accessible(&MENU[i]))
                printf("%2d. %s\n", i + 1, MENU[i].libelle);
        printf(" 0. Se déconnecter\n");
        choix = lire_entier("Votre choix : ", 0, NB_ENTREES);
        if (choix == 0)
            return;
        if (!accessible(&MENU[choix - 1])) {
            exiger((Permission)MENU[choix - 1].permission);
            pause_console();
            continue;
        }
        MENU[choix - 1].action();
    }
}

int main(void)
{
    console_init();
    if (!base_ouvrir(FICHIER_BASE))
        return EXIT_FAILURE;
    atexit(base_fermer);
    config_charger();

    if (premier_lancement())
        assistant_installation();
    config_charger();

    while (connexion()) {
        menu_principal();
        journaliser("Déconnexion");
        printf("\n  Au revoir %s.\n", session.nom);
    }
    printf("Fermeture de SmartPress.\n");
    return EXIT_SUCCESS;
}
