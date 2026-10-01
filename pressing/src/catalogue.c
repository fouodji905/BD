#include <stdio.h>
#include <string.h>

#include "base.h"
#include "catalogue.h"
#include "console.h"
#include "parametres.h"
#include "securite.h"

typedef struct {
    const char *categorie;
    const char *nom;
    int prix;            /* en unites de la devise */
    int duree_heures;
} ServiceExemple;

static const ServiceExemple EXEMPLES[] = {
    { "Hauts", "Chemise", 1000, 48 },
    { "Hauts", "T-shirt / Polo", 700, 48 },
    { "Hauts", "Pull", 1500, 48 },
    { "Hauts", "Veste", 2000, 72 },
    { "Bas", "Pantalon", 1000, 48 },
    { "Bas", "Jean", 1200, 48 },
    { "Bas", "Jupe", 1000, 48 },
    { "Tenues", "Costume 2 pièces", 3500, 72 },
    { "Tenues", "Costume 3 pièces", 4500, 72 },
    { "Tenues", "Robe simple", 2000, 48 },
    { "Tenues", "Robe de soirée / mariage", 7000, 96 },
    { "Tenues", "Boubou / Kaba", 2500, 72 },
    { "Tenues", "Manteau", 3000, 72 },
    { "Linge de maison", "Drap", 1500, 48 },
    { "Linge de maison", "Couverture", 3500, 96 },
    { "Linge de maison", "Rideau", 2500, 72 },
    { "Linge de maison", "Nappe", 1500, 48 },
    { "Divers", "Repassage seul (pièce)", 300, 24 }
};

static const ServiceExemple OPTIONS_EXEMPLES[] = {
    { "", "Détachage", 500, 0 },
    { "", "Amidon", 200, 0 },
    { "", "Sur cintre + housse", 200, 0 },
    { "", "Parfum", 200, 0 }
};

/* Prototypes des fonctions internes au module. */
static int choisir_categorie(void);
static void ajouter_service(void);
static void modifier_service(void);
static void basculer_service(void);
static void ajouter_option(void);
static void modifier_option(void);
static void historique_prix(void);
static void noter_prix(const char *libelle, Montant ancien, Montant nouveau);

void catalogue_exemple(void)
{
    size_t i;

    base_debut();
    for (i = 0; i < sizeof EXEMPLES / sizeof EXEMPLES[0]; i++) {
        req_executer("INSERT OR IGNORE INTO categories (nom) VALUES (?)",
                     "t", EXEMPLES[i].categorie);
        req_executer("INSERT INTO services (categorie_id, nom, prix,"
                     " duree_heures) VALUES ((SELECT id FROM categories"
                     " WHERE nom = ?), ?, ?, ?)",
                     "ttli", EXEMPLES[i].categorie, EXEMPLES[i].nom,
                     (long long)EXEMPLES[i].prix * 100,
                     EXEMPLES[i].duree_heures);
    }
    for (i = 0; i < sizeof OPTIONS_EXEMPLES / sizeof OPTIONS_EXEMPLES[0]; i++)
        req_executer("INSERT INTO options_service (nom, prix) VALUES (?, ?)",
                     "tl", OPTIONS_EXEMPLES[i].nom,
                     (long long)OPTIONS_EXEMPLES[i].prix * 100);
    base_valider();
    printf("  Catalogue d'exemple chargé (%d services, %d options).\n",
           (int)(sizeof EXEMPLES / sizeof EXEMPLES[0]),
           (int)(sizeof OPTIONS_EXEMPLES / sizeof OPTIONS_EXEMPLES[0]));
}

void catalogue_afficher_services(int seulement_actifs)
{
    sqlite3_stmt *st;
    char categorie[64] = "";

    st = req_preparer("SELECT s.id, c.nom, s.nom, s.prix, s.duree_heures,"
                      " s.actif FROM services s"
                      " JOIN categories c ON c.id = s.categorie_id"
                      " WHERE s.actif = 1 OR ? = 0"
                      " ORDER BY c.nom, s.nom", "i", seulement_actifs);
    if (st == NULL)
        return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (strcmp(categorie, col_texte(st, 1)) != 0) {
            copier(categorie, col_texte(st, 1), sizeof categorie);
            printf("\n  [%s]\n", categorie);
        }
        printf("  %4d  ", sqlite3_column_int(st, 0));
        ecrire_col(stdout, col_texte(st, 2), 28, 0);
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 3)), 16, 1);
        printf("  %3dh%s\n", sqlite3_column_int(st, 4),
               sqlite3_column_int(st, 5) ? "" : "  (désactivé)");
    }
    sqlite3_finalize(st);
    if (categorie[0] == '\0')
        printf("  Aucun service. Ajoutez-en dans le menu Catalogue.\n");
}

void catalogue_afficher_options(int seulement_actifs)
{
    sqlite3_stmt *st;
    int vide = 1;

    st = req_preparer("SELECT id, nom, prix, actif FROM options_service"
                      " WHERE actif = 1 OR ? = 0 ORDER BY nom", "i",
                      seulement_actifs);
    if (st == NULL)
        return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        vide = 0;
        printf("  %4d  ", sqlite3_column_int(st, 0));
        ecrire_col(stdout, col_texte(st, 1), 28, 0);
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 16, 1);
        printf("%s\n", sqlite3_column_int(st, 3) ? "" : "  (désactivée)");
    }
    sqlite3_finalize(st);
    if (vide)
        printf("  Aucune option.\n");
}

static int choisir_categorie(void)
{
    sqlite3_stmt *st;
    char nom[64];
    int id;

    printf("Catégories :\n");
    st = req_preparer("SELECT id, nom FROM categories ORDER BY nom", NULL);
    if (st == NULL)
        return 0;
    while (sqlite3_step(st) == SQLITE_ROW)
        printf("  %d. %s\n", sqlite3_column_int(st, 0), col_texte(st, 1));
    sqlite3_finalize(st);
    for (;;) {
        id = lire_entier("N° de catégorie (0 = nouvelle catégorie) : ",
                         0, 1000000);
        if (id == 0) {
            lire_obligatoire("Nom de la nouvelle catégorie : ", nom,
                             sizeof nom);
            req_executer("INSERT OR IGNORE INTO categories (nom) VALUES (?)",
                         "t", nom);
            return (int)req_entier("SELECT id FROM categories WHERE nom = ?",
                                   "t", nom);
        }
        if (req_entier("SELECT COUNT(*) FROM categories WHERE id = ?",
                       "i", id) > 0)
            return id;
        printf("  Catégorie introuvable.\n");
    }
}

static void noter_prix(const char *libelle, Montant ancien, Montant nouveau)
{
    if (ancien == nouveau)
        return;
    req_executer("INSERT INTO historique_prix (libelle, ancien, nouveau,"
                 " utilisateur_id) VALUES (?, ?, ?, ?)",
                 "tlli", libelle, ancien, nouveau, session.id);
}

static void ajouter_service(void)
{
    char nom[80];
    int categorie, duree;
    Montant prix;

    titre("Nouveau service");
    categorie = choisir_categorie();
    if (categorie == 0)
        return;
    lire_obligatoire("Nom du service (ex. Chemise) : ", nom, sizeof nom);
    prix = lire_montant("Prix unitaire : ");
    duree = lire_entier_defaut("Délai de traitement en heures [48] : ",
                               1, 2000, 48);
    req_executer("INSERT INTO services (categorie_id, nom, prix, duree_heures)"
                 " VALUES (?, ?, ?, ?)", "itli", categorie, nom, prix, duree);
    journaliser("Service ajouté : %s à %s", nom, fmt_montant(prix));
    printf("  Service ajouté.\n");
}

static void modifier_service(void)
{
    sqlite3_stmt *st;
    char nom[80], actuel[80], invite[64];
    int id, duree, categorie;
    Montant prix, ancien_prix;

    catalogue_afficher_services(0);
    id = lire_entier("\nN° du service à modifier (0 pour annuler) : ",
                     0, 1000000);
    if (id == 0)
        return;
    st = req_preparer("SELECT nom, prix, duree_heures, categorie_id"
                      " FROM services WHERE id = ?", "i", id);
    if (st == NULL)
        return;
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        printf("  Service introuvable.\n");
        return;
    }
    copier(actuel, col_texte(st, 0), sizeof actuel);
    ancien_prix = sqlite3_column_int64(st, 1);
    duree = sqlite3_column_int(st, 2);
    categorie = sqlite3_column_int(st, 3);
    sqlite3_finalize(st);

    lire_avec_defaut("Nom", actuel, nom, sizeof nom);
    if (nom[0] == '\0')
        copier(nom, actuel, sizeof nom);
    prix = lire_montant_defaut("Prix unitaire", ancien_prix);
    snprintf(invite, sizeof invite, "Délai en heures [%d] : ", duree);
    duree = lire_entier_defaut(invite, 1, 2000, duree);
    if (confirmer("Changer de catégorie ?"))
        categorie = choisir_categorie();
    req_executer("UPDATE services SET nom = ?, prix = ?, duree_heures = ?,"
                 " categorie_id = ? WHERE id = ?", "tliii",
                 nom, prix, duree, categorie, id);
    noter_prix(nom, ancien_prix, prix);
    journaliser("Service n°%d modifié : %s à %s", id, nom, fmt_montant(prix));
    printf("  Enregistré.\n");
}

static void basculer_service(void)
{
    int id, actif;

    catalogue_afficher_services(0);
    id = lire_entier("\nN° du service (0 pour annuler) : ", 0, 1000000);
    if (id == 0)
        return;
    if (req_entier("SELECT COUNT(*) FROM services WHERE id = ?", "i", id)
        == 0) {
        printf("  Service introuvable.\n");
        return;
    }
    actif = (int)req_entier("SELECT actif FROM services WHERE id = ?",
                            "i", id);
    req_executer("UPDATE services SET actif = ? WHERE id = ?", "ii",
                 !actif, id);
    journaliser("Service n°%d %s", id, actif ? "désactivé" : "activé");
    printf("  Service %s.\n", actif ? "désactivé" : "activé");
}

static void ajouter_option(void)
{
    char nom[80];
    Montant prix;

    titre("Nouvelle option");
    lire_obligatoire("Nom de l'option (ex. Détachage) : ", nom, sizeof nom);
    prix = lire_montant("Prix par pièce : ");
    req_executer("INSERT INTO options_service (nom, prix) VALUES (?, ?)",
                 "tl", nom, prix);
    journaliser("Option ajoutée : %s à %s", nom, fmt_montant(prix));
    printf("  Option ajoutée.\n");
}

static void modifier_option(void)
{
    char nom[80], actuel[80];
    int id, actif;
    Montant prix, ancien_prix;

    catalogue_afficher_options(0);
    id = lire_entier("\nN° de l'option (0 pour annuler) : ", 0, 1000000);
    if (id == 0)
        return;
    if (!req_texte(actuel, sizeof actuel,
                   "SELECT nom FROM options_service WHERE id = ?", "i", id)) {
        printf("  Option introuvable.\n");
        return;
    }
    ancien_prix = req_entier("SELECT prix FROM options_service WHERE id = ?",
                             "i", id);
    actif = (int)req_entier("SELECT actif FROM options_service WHERE id = ?",
                            "i", id);
    lire_avec_defaut("Nom", actuel, nom, sizeof nom);
    if (nom[0] == '\0')
        copier(nom, actuel, sizeof nom);
    prix = lire_montant_defaut("Prix par pièce", ancien_prix);
    if (confirmer(actif ? "Désactiver cette option ?"
                        : "Réactiver cette option ?"))
        actif = !actif;
    req_executer("UPDATE options_service SET nom = ?, prix = ?, actif = ?"
                 " WHERE id = ?", "tlii", nom, prix, actif, id);
    noter_prix(nom, ancien_prix, prix);
    journaliser("Option n°%d modifiée : %s à %s", id, nom, fmt_montant(prix));
    printf("  Enregistré.\n");
}

static void historique_prix(void)
{
    sqlite3_stmt *st;

    titre("Historique des changements de prix");
    st = req_preparer("SELECT h.date, h.libelle, h.ancien, h.nouveau,"
                      " COALESCE(u.identifiant, '?') FROM historique_prix h"
                      " LEFT JOIN utilisateurs u ON u.id = h.utilisateur_id"
                      " ORDER BY h.id DESC LIMIT 100", NULL);
    if (st == NULL)
        return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf("%s  ", col_texte(st, 0));
        ecrire_col(stdout, col_texte(st, 1), 26, 0);
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 2)), 14, 1);
        printf(" -> ");
        ecrire_col(stdout, fmt_montant(sqlite3_column_int64(st, 3)), 14, 1);
        printf("  par %s\n", col_texte(st, 4));
    }
    sqlite3_finalize(st);
}

void menu_catalogue(void)
{
    int choix;

    for (;;) {
        titre("Catalogue et tarifs");
        printf("1. Voir les services et prix\n");
        printf("2. Voir les options (détachage, amidon...)\n");
        printf("3. Ajouter un service\n");
        printf("4. Modifier un service (nom, prix, délai)\n");
        printf("5. Activer / désactiver un service\n");
        printf("6. Ajouter une option\n");
        printf("7. Modifier / désactiver une option\n");
        printf("8. Historique des prix\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 8);
        if (choix == 0)
            return;
        if (choix >= 3 && !exiger(P_CATALOGUE)) {
            pause_console();
            continue;
        }
        switch (choix) {
        case 1:
            titre("Services");
            catalogue_afficher_services(0);
            break;
        case 2:
            titre("Options");
            catalogue_afficher_options(0);
            break;
        case 3: ajouter_service(); break;
        case 4: modifier_service(); break;
        case 5: basculer_service(); break;
        case 6: ajouter_option(); break;
        case 7: modifier_option(); break;
        case 8: historique_prix(); break;
        }
        pause_console();
    }
}
