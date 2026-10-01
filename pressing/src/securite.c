#define _CRT_RAND_S
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base.h"
#include "console.h"
#include "parametres.h"
#include "securite.h"
#include "sha256.h"

#define ITERATIONS 20000

Utilisateur session;

/* Matrice des permissions par role. */
static const int PERMISSIONS[NB_ROLES + 1] = {
    0,
    /* Proprietaire */ ~0,
    /* Gerant */ ~0 & ~P_PARAMETRES,
    /* Caissier */ P_VENTE | P_CAISSE | P_CLIENTS | P_TRAITEMENT,
    /* Operateur */ P_TRAITEMENT
};

/* Prototypes des fonctions internes au module. */
static void octets_aleatoires(unsigned char *tampon, size_t n);
static void en_hexa(const unsigned char *octets, size_t n, char *dest);
static int verifier_mdp(int utilisateur_id, const char *mdp);
static void noter_connexion(int utilisateur_id, const char *identifiant,
                            int succes, const char *detail);

const char *role_nom(Role r)
{
    switch (r) {
    case ROLE_PROPRIETAIRE: return "Propriétaire";
    case ROLE_GERANT:       return "Gérant";
    case ROLE_CAISSIER:     return "Caissier";
    case ROLE_OPERATEUR:    return "Opérateur";
    }
    return "?";
}

int role_a(Role r, Permission p)
{
    if (r < 1 || r > NB_ROLES)
        return 0;
    return (PERMISSIONS[r] & (int)p) != 0;
}

int autorise(Permission p)
{
    return role_a(session.role, p);
}

int exiger(Permission p)
{
    if (autorise(p))
        return 1;
    printf("\n  Accès refusé : votre rôle (%s) ne permet pas cette action.\n",
           role_nom(session.role));
    return 0;
}

static void octets_aleatoires(unsigned char *tampon, size_t n)
{
    size_t i;
#ifdef _WIN32
    for (i = 0; i < n; i++) {
        unsigned int v;
        rand_s(&v);
        tampon[i] = (unsigned char)v;
    }
#else
    FILE *f = fopen("/dev/urandom", "rb");

    if (f != NULL) {
        size_t lus = fread(tampon, 1, n, f);
        fclose(f);
        if (lus == n)
            return;
    }
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    for (i = 0; i < n; i++)
        tampon[i] = (unsigned char)rand();
#endif
}

static void en_hexa(const unsigned char *octets, size_t n, char *dest)
{
    static const char chiffres[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < n; i++) {
        dest[2 * i] = chiffres[octets[i] >> 4];
        dest[2 * i + 1] = chiffres[octets[i] & 0x0F];
    }
    dest[2 * n] = '\0';
}

void generer_sel(char sel[33])
{
    unsigned char octets[16];

    octets_aleatoires(octets, sizeof octets);
    en_hexa(octets, sizeof octets, sel);
}

/* Empreinte salee et iteree, pour ralentir les attaques par essais. */
void hacher_mdp(const char *mdp, const char *sel, char hash[65])
{
    unsigned char empreinte[32];
    Sha256 c;
    int i;

    sha256_init(&c);
    sha256_ajouter(&c, sel, strlen(sel));
    sha256_ajouter(&c, mdp, strlen(mdp));
    sha256_finir(&c, empreinte);
    for (i = 0; i < ITERATIONS; i++) {
        sha256_init(&c);
        sha256_ajouter(&c, empreinte, sizeof empreinte);
        sha256_ajouter(&c, sel, strlen(sel));
        sha256_ajouter(&c, mdp, strlen(mdp));
        sha256_finir(&c, empreinte);
    }
    en_hexa(empreinte, sizeof empreinte, hash);
}

int mdp_acceptable(const char *mdp)
{
    int lettre = 0, chiffre = 0;
    const char *p;

    if (strlen(mdp) < 8) {
        printf("  Le mot de passe doit contenir au moins 8 caractères.\n");
        return 0;
    }
    for (p = mdp; *p != '\0'; p++) {
        if (isdigit((unsigned char)*p))
            chiffre = 1;
        else if (isalpha((unsigned char)*p))
            lettre = 1;
    }
    if (!lettre || !chiffre) {
        printf("  Le mot de passe doit contenir des lettres et des chiffres.\n");
        return 0;
    }
    return 1;
}

static int verifier_mdp(int utilisateur_id, const char *mdp)
{
    char sel[64], attendu[80], calcule[65];
    int i, difference = 0;

    if (!req_texte(sel, sizeof sel,
                   "SELECT sel FROM utilisateurs WHERE id = ?", "i",
                   utilisateur_id))
        return 0;
    req_texte(attendu, sizeof attendu,
              "SELECT hash FROM utilisateurs WHERE id = ?", "i",
              utilisateur_id);
    hacher_mdp(mdp, sel, calcule);
    if (strlen(attendu) != 64)
        return 0;
    for (i = 0; i < 64; i++)
        difference |= attendu[i] ^ calcule[i];
    return difference == 0;
}

int changer_mot_de_passe(int utilisateur_id, int exiger_ancien)
{
    char ancien[128], nouveau[128], confirmation[128];
    char sel[33], hash[65];

    if (exiger_ancien) {
        lire_mot_de_passe("Mot de passe actuel : ", ancien, sizeof ancien);
        if (!verifier_mdp(utilisateur_id, ancien)) {
            printf("  Mot de passe actuel incorrect.\n");
            return 0;
        }
    }
    for (;;) {
        lire_mot_de_passe("Nouveau mot de passe (8 caractères min., "
                          "lettres et chiffres) : ", nouveau, sizeof nouveau);
        if (!mdp_acceptable(nouveau))
            continue;
        lire_mot_de_passe("Confirmez le mot de passe : ",
                          confirmation, sizeof confirmation);
        if (strcmp(nouveau, confirmation) != 0) {
            printf("  Les deux saisies sont différentes.\n");
            continue;
        }
        break;
    }
    generer_sel(sel);
    hacher_mdp(nouveau, sel, hash);
    if (!req_executer("UPDATE utilisateurs SET sel = ?, hash = ?,"
                      " doit_changer_mdp = 0 WHERE id = ?",
                      "tti", sel, hash, utilisateur_id))
        return 0;
    printf("  Mot de passe enregistré.\n");
    return 1;
}

static void noter_connexion(int utilisateur_id, const char *identifiant,
                            int succes, const char *detail)
{
    if (utilisateur_id > 0)
        req_executer("INSERT INTO journal_connexions"
                     " (utilisateur_id, identifiant, succes, detail)"
                     " VALUES (?, ?, ?, ?)",
                     "itit", utilisateur_id, identifiant, succes, detail);
    else
        req_executer("INSERT INTO journal_connexions"
                     " (identifiant, succes, detail) VALUES (?, ?, ?)",
                     "tit", identifiant, succes, detail);
}

int connexion(void)
{
    char identifiant[32], mdp[128], bloque[32];
    sqlite3_stmt *st;
    int id, actif, echecs, doit_changer;

    titre("SMARTPRESS - Connexion");
    printf("  %s\n", config.nom);
    for (;;) {
        printf("\n");
        if (lire_ligne("Identifiant (vide pour quitter) : ",
                       identifiant, sizeof identifiant) == 0)
            return 0;
        lire_mot_de_passe("Mot de passe : ", mdp, sizeof mdp);

        st = req_preparer(
            "SELECT id, nom, role, actif, echecs, doit_changer_mdp,"
            " CASE WHEN bloque_jusqua > datetime('now','localtime')"
            "      THEN bloque_jusqua END, identifiant"
            " FROM utilisateurs WHERE identifiant = ?", "t", identifiant);
        if (st == NULL)
            return 0;
        if (sqlite3_step(st) != SQLITE_ROW) {
            sqlite3_finalize(st);
            noter_connexion(0, identifiant, 0, "identifiant inconnu");
            printf("  Identifiant ou mot de passe incorrect.\n");
            continue;
        }
        id = sqlite3_column_int(st, 0);
        copier(session.nom, col_texte(st, 1), sizeof session.nom);
        session.role = (Role)sqlite3_column_int(st, 2);
        actif = sqlite3_column_int(st, 3);
        echecs = sqlite3_column_int(st, 4);
        doit_changer = sqlite3_column_int(st, 5);
        copier(bloque, col_texte(st, 6), sizeof bloque);
        copier(session.identifiant, col_texte(st, 7),
               sizeof session.identifiant);
        sqlite3_finalize(st);

        if (!actif) {
            noter_connexion(id, identifiant, 0, "compte désactivé");
            printf("  Ce compte est désactivé. Contactez le responsable.\n");
            continue;
        }
        if (bloque[0] != '\0') {
            noter_connexion(id, identifiant, 0, "compte bloqué");
            printf("  Compte bloqué jusqu'à %s (trop de tentatives).\n",
                   bloque);
            continue;
        }
        if (!verifier_mdp(id, mdp)) {
            echecs++;
            if (echecs >= config.tentatives_max) {
                char duree[32];

                snprintf(duree, sizeof duree, "+%d minutes",
                         config.blocage_minutes);
                req_executer("UPDATE utilisateurs SET echecs = 0,"
                             " bloque_jusqua = datetime('now','localtime',?)"
                             " WHERE id = ?", "ti", duree, id);
                noter_connexion(id, identifiant, 0,
                                "mot de passe incorrect, compte bloqué");
                printf("  Trop de tentatives : compte bloqué %d minutes.\n",
                       config.blocage_minutes);
            } else {
                req_executer("UPDATE utilisateurs SET echecs = ? WHERE id = ?",
                             "ii", echecs, id);
                noter_connexion(id, identifiant, 0, "mot de passe incorrect");
                printf("  Identifiant ou mot de passe incorrect.\n");
            }
            continue;
        }

        session.id = id;
        req_executer("UPDATE utilisateurs SET echecs = 0, bloque_jusqua = NULL,"
                     " derniere_connexion = datetime('now','localtime')"
                     " WHERE id = ?", "i", id);
        noter_connexion(id, identifiant, 1, "connexion réussie");
        printf("\n  Bienvenue %s (%s).\n", session.nom, role_nom(session.role));
        if (doit_changer) {
            printf("\n  Vous devez choisir un nouveau mot de passe.\n");
            while (!changer_mot_de_passe(id, 0))
                ;
        }
        return 1;
    }
}

void journaliser(const char *format, ...)
{
    char texte[512];
    va_list ap;

    va_start(ap, format);
    vsnprintf(texte, sizeof texte, format, ap);
    va_end(ap);
    req_executer("INSERT INTO journal_actions (utilisateur_id, action)"
                 " VALUES (?, ?)", "it", session.id, texte);
}
