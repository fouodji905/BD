#ifndef SECURITE_H
#define SECURITE_H

/* Roles systeme (module 3 du document). */
typedef enum {
    ROLE_PROPRIETAIRE = 1,
    ROLE_GERANT = 2,
    ROLE_CAISSIER = 3,
    ROLE_OPERATEUR = 4
} Role;

#define NB_ROLES 4

/* Permissions par module et par action. */
typedef enum {
    P_VENTE = 1 << 0,         /* creer des commandes, encaisser */
    P_CAISSE = 1 << 1,        /* ouvrir / fermer la caisse, depenses */
    P_ANNULER = 1 << 2,       /* annuler une commande, remise a credit */
    P_REMISE_LIBRE = 1 << 3,  /* remise au-dela du plafond caissier */
    P_CLIENTS = 1 << 4,       /* fiches clients */
    P_CATALOGUE = 1 << 5,     /* modifier services et prix */
    P_TRAITEMENT = 1 << 6,    /* changer le statut des commandes */
    P_RAPPORTS = 1 << 7,      /* tableaux de bord, exports */
    P_UTILISATEURS = 1 << 8,  /* comptes et journaux */
    P_PARAMETRES = 1 << 9     /* parametres de l'entreprise */
} Permission;

typedef struct {
    int id;
    char identifiant[32];
    char nom[64];
    Role role;
} Utilisateur;

/* L'utilisateur connecte. */
extern Utilisateur session;

const char *role_nom(Role r);
int role_a(Role r, Permission p);

/* Renvoie 1 si l'utilisateur connecte a la permission. */
int autorise(Permission p);

/* Comme autorise(), mais affiche un message en cas de refus. */
int exiger(Permission p);

void generer_sel(char sel[33]);
void hacher_mdp(const char *mdp, const char *sel, char hash[65]);

/* Verifie la politique de mot de passe (affiche la raison du refus). */
int mdp_acceptable(const char *mdp);

/* Demande et enregistre un nouveau mot de passe. */
int changer_mot_de_passe(int utilisateur_id, int exiger_ancien);

/* Ecran de connexion. Renvoie 1 si connecte, 0 pour quitter. */
int connexion(void);

/* Ajoute une ligne au journal des actions (audit). */
void journaliser(const char *format, ...);

#endif
