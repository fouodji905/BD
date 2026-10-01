#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "base.h"
#include "console.h"
#include "securite.h"
#include "utilisateurs.h"

/* Prototypes des fonctions internes au module. */
static int lire_identifiant(char *dest, size_t taille);
static Role choisir_role(void);
static void lister_utilisateurs(void);
static void creer_utilisateur(void);
static int choisir_utilisateur(int *role);
static int peut_gerer(int id, int role);
static int dernier_proprietaire(int id);
static void modifier_utilisateur(void);
static void basculer_activation(void);
static void reinitialiser_mdp(void);
static void journal_connexions(void);
static void journal_actions(void);

/* Identifiant : lettres, chiffres, point, tiret ; unique. */
static int lire_identifiant(char *dest, size_t taille)
{
    const char *p;
    int valide;

    for (;;) {
        lire_obligatoire("Identifiant de connexion : ", dest, taille);
        valide = 1;
        for (p = dest; *p != '\0'; p++)
            if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-'
                && *p != '_')
                valide = 0;
        if (!valide) {
            printf("  Utilisez seulement lettres, chiffres, . - _\n");
            continue;
        }
        if (req_entier("SELECT COUNT(*) FROM utilisateurs"
                       " WHERE identifiant = ?", "t", dest) > 0) {
            printf("  Cet identifiant existe déjà.\n");
            continue;
        }
        return 1;
    }
}

void creer_proprietaire(void)
{
    char identifiant[32], nom[64], telephone[40];
    int id;

    lire_identifiant(identifiant, sizeof identifiant);
    lire_obligatoire("Nom complet : ", nom, sizeof nom);
    lire_ligne("Téléphone : ", telephone, sizeof telephone);
    req_executer("INSERT INTO utilisateurs"
                 " (identifiant, nom, telephone, role, sel, hash)"
                 " VALUES (?, ?, ?, ?, '', '')",
                 "ttti", identifiant, nom, telephone, ROLE_PROPRIETAIRE);
    id = (int)base_dernier_id();
    while (!changer_mot_de_passe(id, 0))
        ;
}

static Role choisir_role(void)
{
    int r, premier = autorise(P_PARAMETRES) ? 1 : 2;

    printf("Rôles :\n");
    for (r = premier; r <= NB_ROLES; r++)
        printf("  %d. %s\n", r, role_nom((Role)r));
    printf("  (Propriétaire : tout ; Gérant : tout sauf paramètres ;\n"
           "   Caissier : caisse, clients, suivi ; Opérateur : suivi atelier)\n");
    return (Role)lire_entier("Rôle : ", premier, NB_ROLES);
}

static void lister_utilisateurs(void)
{
    sqlite3_stmt *st;

    titre("Utilisateurs");
    st = req_preparer("SELECT id, identifiant, nom, role, actif,"
                      " COALESCE(derniere_connexion, 'jamais'),"
                      " CASE WHEN bloque_jusqua > datetime('now','localtime')"
                      "      THEN 1 ELSE 0 END"
                      " FROM utilisateurs ORDER BY actif DESC, role, nom",
                      NULL);
    if (st == NULL)
        return;
    printf("N°   Identifiant     Nom                   Rôle          État"
           "       Dernière connexion\n");
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *etat = !sqlite3_column_int(st, 4) ? "désactivé"
                         : sqlite3_column_int(st, 6) ? "bloqué" : "actif";

        printf("%-4d ", sqlite3_column_int(st, 0));
        ecrire_col(stdout, col_texte(st, 1), 15, 0);
        printf(" ");
        ecrire_col(stdout, col_texte(st, 2), 21, 0);
        printf(" ");
        ecrire_col(stdout, role_nom((Role)sqlite3_column_int(st, 3)), 13, 0);
        printf(" ");
        ecrire_col(stdout, etat, 10, 0);
        printf(" %s\n", col_texte(st, 5));
    }
    sqlite3_finalize(st);
}

static void creer_utilisateur(void)
{
    char identifiant[32], nom[64], telephone[40], sel[33], hash[65];
    char provisoire[16];
    Role role;

    titre("Nouvel utilisateur");
    lire_identifiant(identifiant, sizeof identifiant);
    lire_obligatoire("Nom complet : ", nom, sizeof nom);
    lire_ligne("Téléphone : ", telephone, sizeof telephone);
    role = choisir_role();

    /* Mot de passe provisoire, a changer a la premiere connexion. */
    generer_sel(sel);
    snprintf(provisoire, sizeof provisoire, "%.8sa7", sel);
    generer_sel(sel);
    hacher_mdp(provisoire, sel, hash);
    if (!req_executer("INSERT INTO utilisateurs (identifiant, nom, telephone,"
                      " role, sel, hash, doit_changer_mdp)"
                      " VALUES (?, ?, ?, ?, ?, ?, 1)",
                      "tttitt", identifiant, nom, telephone, (int)role,
                      sel, hash))
        return;
    journaliser("Utilisateur %s créé (%s)", identifiant, role_nom(role));
    printf("\n  Compte créé.\n");
    printf("  Identifiant          : %s\n", identifiant);
    printf("  Mot de passe provisoire : %s\n", provisoire);
    printf("  Il devra être changé à la première connexion.\n");
}

static int choisir_utilisateur(int *role)
{
    int id = lire_entier("N° de l'utilisateur (0 pour annuler) : ",
                         0, 1000000);

    if (id == 0)
        return 0;
    *role = (int)req_entier("SELECT role FROM utilisateurs WHERE id = ?",
                            "i", id);
    if (*role == 0) {
        printf("  Utilisateur introuvable.\n");
        return 0;
    }
    return id;
}

/* Un gerant ne peut pas modifier un proprietaire. */
static int peut_gerer(int id, int role)
{
    if (id == session.id) {
        printf("  Vous ne pouvez pas modifier votre propre compte ici.\n");
        return 0;
    }
    if (role == ROLE_PROPRIETAIRE && session.role != ROLE_PROPRIETAIRE) {
        printf("  Seul un propriétaire peut modifier un propriétaire.\n");
        return 0;
    }
    return 1;
}

static int dernier_proprietaire(int id)
{
    return req_entier("SELECT COUNT(*) FROM utilisateurs WHERE role = ?"
                      " AND actif = 1 AND id <> ?", "ii",
                      ROLE_PROPRIETAIRE, id) == 0
        && req_entier("SELECT role FROM utilisateurs WHERE id = ?", "i", id)
           == ROLE_PROPRIETAIRE;
}

static void modifier_utilisateur(void)
{
    char nom[64], telephone[40], actuel_nom[64], actuel_tel[40];
    int id, role, nouveau_role;

    lister_utilisateurs();
    if ((id = choisir_utilisateur(&role)) == 0 || !peut_gerer(id, role))
        return;
    req_texte(actuel_nom, sizeof actuel_nom,
              "SELECT nom FROM utilisateurs WHERE id = ?", "i", id);
    req_texte(actuel_tel, sizeof actuel_tel,
              "SELECT telephone FROM utilisateurs WHERE id = ?", "i", id);
    lire_avec_defaut("Nom", actuel_nom, nom, sizeof nom);
    if (nom[0] == '\0')
        copier(nom, actuel_nom, sizeof nom);
    lire_avec_defaut("Téléphone", actuel_tel, telephone, sizeof telephone);
    printf("Rôle actuel : %s\n", role_nom((Role)role));
    nouveau_role = role;
    if (confirmer("Changer le rôle ?")) {
        nouveau_role = (int)choisir_role();
        if (role == ROLE_PROPRIETAIRE && nouveau_role != role
            && dernier_proprietaire(id)) {
            printf("  Impossible : c'est le dernier propriétaire actif.\n");
            nouveau_role = role;
        }
    }
    req_executer("UPDATE utilisateurs SET nom = ?, telephone = ?, role = ?"
                 " WHERE id = ?", "ttii", nom, telephone, nouveau_role, id);
    journaliser("Utilisateur n°%d modifié (rôle %s)", id,
                role_nom((Role)nouveau_role));
    printf("  Enregistré.\n");
}

static void basculer_activation(void)
{
    int id, role, actif;

    lister_utilisateurs();
    if ((id = choisir_utilisateur(&role)) == 0 || !peut_gerer(id, role))
        return;
    actif = (int)req_entier("SELECT actif FROM utilisateurs WHERE id = ?",
                            "i", id);
    if (actif && dernier_proprietaire(id)) {
        printf("  Impossible : c'est le dernier propriétaire actif.\n");
        return;
    }
    if (!confirmer(actif ? "Désactiver ce compte ?" : "Réactiver ce compte ?"))
        return;
    req_executer("UPDATE utilisateurs SET actif = ? WHERE id = ?", "ii",
                 !actif, id);
    journaliser("Utilisateur n°%d %s", id, actif ? "désactivé" : "réactivé");
    printf("  Compte %s.\n", actif ? "désactivé" : "réactivé");
}

static void reinitialiser_mdp(void)
{
    char sel[33], hash[65], provisoire[16];
    int id, role;

    lister_utilisateurs();
    if ((id = choisir_utilisateur(&role)) == 0 || !peut_gerer(id, role))
        return;
    generer_sel(sel);
    snprintf(provisoire, sizeof provisoire, "%.8sa7", sel);
    generer_sel(sel);
    hacher_mdp(provisoire, sel, hash);
    req_executer("UPDATE utilisateurs SET sel = ?, hash = ?, echecs = 0,"
                 " bloque_jusqua = NULL, doit_changer_mdp = 1 WHERE id = ?",
                 "tti", sel, hash, id);
    journaliser("Mot de passe de l'utilisateur n°%d réinitialisé", id);
    printf("  Compte débloqué. Mot de passe provisoire : %s\n", provisoire);
    printf("  Il devra être changé à la prochaine connexion.\n");
}

static void journal_connexions(void)
{
    sqlite3_stmt *st;

    titre("Journal des connexions (50 dernières)");
    st = req_preparer("SELECT date, identifiant, succes, detail"
                      " FROM journal_connexions ORDER BY id DESC LIMIT 50",
                      NULL);
    if (st == NULL)
        return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s  ", col_texte(st, 0));
        ecrire_col(stdout, col_texte(st, 1), 15, 0);
        printf(" %s  %s\n", sqlite3_column_int(st, 2) ? "OK   " : "ÉCHEC",
               col_texte(st, 3));
    }
    sqlite3_finalize(st);
}

static void journal_actions(void)
{
    sqlite3_stmt *st;

    titre("Journal des actions (100 dernières)");
    st = req_preparer("SELECT j.date, COALESCE(u.identifiant, '?'), j.action"
                      " FROM journal_actions j"
                      " LEFT JOIN utilisateurs u ON u.id = j.utilisateur_id"
                      " ORDER BY j.id DESC LIMIT 100", NULL);
    if (st == NULL)
        return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s  ", col_texte(st, 0));
        ecrire_col(stdout, col_texte(st, 1), 12, 0);
        printf(" %s\n", col_texte(st, 2));
    }
    sqlite3_finalize(st);
}

void changer_mon_mot_de_passe(void)
{
    titre("Changer mon mot de passe");
    if (changer_mot_de_passe(session.id, 1))
        journaliser("Mot de passe changé");
}

void menu_utilisateurs(void)
{
    int choix;

    for (;;) {
        titre("Utilisateurs et sécurité");
        printf("1. Liste des utilisateurs\n");
        printf("2. Créer un utilisateur\n");
        printf("3. Modifier un utilisateur (nom, rôle)\n");
        printf("4. Désactiver / réactiver un compte\n");
        printf("5. Réinitialiser un mot de passe / débloquer\n");
        printf("6. Journal des connexions\n");
        printf("7. Journal des actions (audit)\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 7);
        switch (choix) {
        case 0: return;
        case 1: lister_utilisateurs(); break;
        case 2: creer_utilisateur(); break;
        case 3: modifier_utilisateur(); break;
        case 4: basculer_activation(); break;
        case 5: reinitialiser_mdp(); break;
        case 6: journal_connexions(); break;
        case 7: journal_actions(); break;
        }
        pause_console();
    }
}
