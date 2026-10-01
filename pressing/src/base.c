#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "base.h"

/* La connexion a la base de donnees, partagee par tous les modules. */
sqlite3 *db = NULL;

#define MAINTENANT "(datetime('now','localtime'))"

static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS parametres ("
    "  cle TEXT PRIMARY KEY,"
    "  valeur TEXT NOT NULL);"

    "CREATE TABLE IF NOT EXISTS utilisateurs ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  identifiant TEXT NOT NULL UNIQUE COLLATE NOCASE,"
    "  nom TEXT NOT NULL,"
    "  telephone TEXT NOT NULL DEFAULT '',"
    "  role INTEGER NOT NULL,"
    "  sel TEXT NOT NULL,"
    "  hash TEXT NOT NULL,"
    "  actif INTEGER NOT NULL DEFAULT 1,"
    "  echecs INTEGER NOT NULL DEFAULT 0,"
    "  bloque_jusqua TEXT,"
    "  doit_changer_mdp INTEGER NOT NULL DEFAULT 0,"
    "  derniere_connexion TEXT,"
    "  cree_le TEXT NOT NULL DEFAULT " MAINTENANT ");"

    "CREATE TABLE IF NOT EXISTS journal_connexions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  utilisateur_id INTEGER,"
    "  identifiant TEXT NOT NULL,"
    "  date TEXT NOT NULL DEFAULT " MAINTENANT ","
    "  succes INTEGER NOT NULL,"
    "  detail TEXT NOT NULL DEFAULT '');"

    "CREATE TABLE IF NOT EXISTS journal_actions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  utilisateur_id INTEGER,"
    "  date TEXT NOT NULL DEFAULT " MAINTENANT ","
    "  action TEXT NOT NULL);"

    "CREATE TABLE IF NOT EXISTS clients ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  code TEXT UNIQUE,"
    "  nom TEXT NOT NULL,"
    "  telephone TEXT NOT NULL DEFAULT '',"
    "  email TEXT NOT NULL DEFAULT '',"
    "  adresse TEXT NOT NULL DEFAULT '',"
    "  notes TEXT NOT NULL DEFAULT '',"
    "  actif INTEGER NOT NULL DEFAULT 1,"
    "  cree_le TEXT NOT NULL DEFAULT " MAINTENANT ");"

    "CREATE TABLE IF NOT EXISTS categories ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  nom TEXT NOT NULL UNIQUE COLLATE NOCASE);"

    "CREATE TABLE IF NOT EXISTS services ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  categorie_id INTEGER NOT NULL REFERENCES categories(id),"
    "  nom TEXT NOT NULL,"
    "  prix INTEGER NOT NULL,"
    "  duree_heures INTEGER NOT NULL DEFAULT 48,"
    "  actif INTEGER NOT NULL DEFAULT 1);"

    "CREATE TABLE IF NOT EXISTS options_service ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  nom TEXT NOT NULL,"
    "  prix INTEGER NOT NULL,"
    "  actif INTEGER NOT NULL DEFAULT 1);"

    "CREATE TABLE IF NOT EXISTS historique_prix ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  libelle TEXT NOT NULL,"
    "  ancien INTEGER NOT NULL,"
    "  nouveau INTEGER NOT NULL,"
    "  utilisateur_id INTEGER,"
    "  date TEXT NOT NULL DEFAULT " MAINTENANT ");"

    "CREATE TABLE IF NOT EXISTS sessions_caisse ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  utilisateur_id INTEGER NOT NULL,"
    "  ouverture TEXT NOT NULL DEFAULT " MAINTENANT ","
    "  fond_initial INTEGER NOT NULL,"
    "  fermeture TEXT,"
    "  ferme_par INTEGER,"
    "  montant_theorique INTEGER,"
    "  montant_compte INTEGER,"
    "  ecart INTEGER,"
    "  commentaire TEXT NOT NULL DEFAULT '');"

    "CREATE TABLE IF NOT EXISTS mouvements_caisse ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  session_id INTEGER NOT NULL REFERENCES sessions_caisse(id),"
    "  sens INTEGER NOT NULL,"
    "  montant INTEGER NOT NULL,"
    "  motif TEXT NOT NULL,"
    "  utilisateur_id INTEGER,"
    "  date TEXT NOT NULL DEFAULT " MAINTENANT ");"

    "CREATE TABLE IF NOT EXISTS commandes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  numero TEXT NOT NULL UNIQUE,"
    "  client_id INTEGER NOT NULL REFERENCES clients(id),"
    "  utilisateur_id INTEGER NOT NULL,"
    "  date_depot TEXT NOT NULL DEFAULT " MAINTENANT ","
    "  date_prevue TEXT NOT NULL,"
    "  express INTEGER NOT NULL DEFAULT 0,"
    "  sous_total INTEGER NOT NULL,"
    "  majoration INTEGER NOT NULL DEFAULT 0,"
    "  remise_pct INTEGER NOT NULL DEFAULT 0,"
    "  remise INTEGER NOT NULL DEFAULT 0,"
    "  total INTEGER NOT NULL,"
    "  taux_tva INTEGER NOT NULL DEFAULT 0,"
    "  statut INTEGER NOT NULL DEFAULT 0,"
    "  annulee INTEGER NOT NULL DEFAULT 0,"
    "  motif_annulation TEXT NOT NULL DEFAULT '',"
    "  date_retrait TEXT,"
    "  notes TEXT NOT NULL DEFAULT '');"

    "CREATE TABLE IF NOT EXISTS lignes_commande ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  commande_id INTEGER NOT NULL REFERENCES commandes(id),"
    "  service_id INTEGER REFERENCES services(id),"
    "  libelle TEXT NOT NULL,"
    "  quantite INTEGER NOT NULL,"
    "  prix_unitaire INTEGER NOT NULL,"
    "  options TEXT NOT NULL DEFAULT '',"
    "  prix_options INTEGER NOT NULL DEFAULT 0,"
    "  montant INTEGER NOT NULL,"
    "  couleur TEXT NOT NULL DEFAULT '',"
    "  marque TEXT NOT NULL DEFAULT '',"
    "  defauts TEXT NOT NULL DEFAULT '',"
    "  instructions TEXT NOT NULL DEFAULT '');"

    "CREATE TABLE IF NOT EXISTS paiements ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  commande_id INTEGER NOT NULL REFERENCES commandes(id),"
    "  session_id INTEGER REFERENCES sessions_caisse(id),"
    "  mode INTEGER NOT NULL,"
    "  montant INTEGER NOT NULL,"
    "  utilisateur_id INTEGER,"
    "  date TEXT NOT NULL DEFAULT " MAINTENANT ");"

    "CREATE TABLE IF NOT EXISTS historique_statuts ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  commande_id INTEGER NOT NULL REFERENCES commandes(id),"
    "  ancien INTEGER,"
    "  nouveau INTEGER NOT NULL,"
    "  utilisateur_id INTEGER,"
    "  date TEXT NOT NULL DEFAULT " MAINTENANT ");"

    "CREATE TABLE IF NOT EXISTS notifications ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  commande_id INTEGER,"
    "  client_id INTEGER,"
    "  telephone TEXT NOT NULL DEFAULT '',"
    "  type TEXT NOT NULL,"
    "  message TEXT NOT NULL,"
    "  statut INTEGER NOT NULL DEFAULT 0,"
    "  cree_le TEXT NOT NULL DEFAULT " MAINTENANT ","
    "  traite_le TEXT);"

    "CREATE INDEX IF NOT EXISTS idx_commandes_client ON commandes(client_id);"
    "CREATE INDEX IF NOT EXISTS idx_commandes_statut ON commandes(statut);"
    "CREATE INDEX IF NOT EXISTS idx_lignes_commande ON lignes_commande(commande_id);"
    "CREATE INDEX IF NOT EXISTS idx_paiements_commande ON paiements(commande_id);"
    "CREATE INDEX IF NOT EXISTS idx_paiements_session ON paiements(session_id);"
    "CREATE INDEX IF NOT EXISTS idx_clients_telephone ON clients(telephone);"

    /* Vue pratique : commande + client + montant deja paye. */
    "CREATE VIEW IF NOT EXISTS v_commandes AS"
    "  SELECT c.*, cl.nom AS client_nom, cl.telephone AS client_tel,"
    "         cl.code AS client_code,"
    "         COALESCE((SELECT SUM(p.montant) FROM paiements p"
    "                   WHERE p.commande_id = c.id), 0) AS paye"
    "  FROM commandes c JOIN clients cl ON cl.id = c.client_id;";

/* Prototypes des fonctions internes au module. */
static int lier(sqlite3_stmt *st, const char *types, va_list ap);
static sqlite3_stmt *preparer_va(const char *sql, const char *types,
                                 va_list ap);

int base_ouvrir(const char *chemin)
{
    if (sqlite3_open(chemin, &db) != SQLITE_OK) {
        fprintf(stderr, "Impossible d'ouvrir la base %s : %s\n",
                chemin, sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_busy_timeout(db, 5000);
    return base_exec("PRAGMA foreign_keys = ON;") && base_exec(SCHEMA);
}

void base_fermer(void)
{
    if (db != NULL) {
        sqlite3_close(db);
        db = NULL;
    }
}

int base_exec(const char *sql)
{
    char *erreur = NULL;

    if (sqlite3_exec(db, sql, NULL, NULL, &erreur) != SQLITE_OK) {
        fprintf(stderr, "Erreur base de donnees : %s\n", erreur);
        sqlite3_free(erreur);
        return 0;
    }
    return 1;
}

static int lier(sqlite3_stmt *st, const char *types, va_list ap)
{
    int i, rc;
    const char *texte;

    for (i = 0; types != NULL && types[i] != '\0'; i++) {
        switch (types[i]) {
        case 'i':
            rc = sqlite3_bind_int(st, i + 1, va_arg(ap, int));
            break;
        case 'l':
            rc = sqlite3_bind_int64(st, i + 1, va_arg(ap, long long));
            break;
        case 't':
            texte = va_arg(ap, const char *);
            if (texte == NULL)
                rc = sqlite3_bind_null(st, i + 1);
            else
                rc = sqlite3_bind_text(st, i + 1, texte, -1,
                                       SQLITE_TRANSIENT);
            break;
        default:
            rc = SQLITE_MISUSE;
        }
        if (rc != SQLITE_OK)
            return 0;
    }
    return 1;
}

static sqlite3_stmt *preparer_va(const char *sql, const char *types,
                                 va_list ap)
{
    sqlite3_stmt *st = NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL : %s\n", sqlite3_errmsg(db));
        return NULL;
    }
    if (!lier(st, types, ap)) {
        fprintf(stderr, "Erreur de parametres SQL : %s\n",
                sqlite3_errmsg(db));
        sqlite3_finalize(st);
        return NULL;
    }
    return st;
}

sqlite3_stmt *req_preparer(const char *sql, const char *types, ...)
{
    sqlite3_stmt *st;
    va_list ap;

    va_start(ap, types);
    st = preparer_va(sql, types, ap);
    va_end(ap);
    return st;
}

int req_executer(const char *sql, const char *types, ...)
{
    sqlite3_stmt *st;
    va_list ap;
    int rc;

    va_start(ap, types);
    st = preparer_va(sql, types, ap);
    va_end(ap);
    if (st == NULL)
        return 0;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW)
        ;
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Erreur base de donnees : %s\n", sqlite3_errmsg(db));
        return 0;
    }
    return 1;
}

long long req_entier(const char *sql, const char *types, ...)
{
    sqlite3_stmt *st;
    va_list ap;
    long long valeur = 0;

    va_start(ap, types);
    st = preparer_va(sql, types, ap);
    va_end(ap);
    if (st == NULL)
        return 0;
    if (sqlite3_step(st) == SQLITE_ROW)
        valeur = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return valeur;
}

int req_texte(char *dest, size_t taille, const char *sql,
              const char *types, ...)
{
    sqlite3_stmt *st;
    va_list ap;
    int trouve = 0;

    dest[0] = '\0';
    va_start(ap, types);
    st = preparer_va(sql, types, ap);
    va_end(ap);
    if (st == NULL)
        return 0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        copier(dest, col_texte(st, 0), taille);
        trouve = 1;
    }
    sqlite3_finalize(st);
    return trouve;
}

long long base_dernier_id(void)
{
    return sqlite3_last_insert_rowid(db);
}

void base_debut(void)
{
    base_exec("BEGIN;");
}

int base_valider(void)
{
    return base_exec("COMMIT;");
}

void base_annuler(void)
{
    base_exec("ROLLBACK;");
}

const char *col_texte(sqlite3_stmt *st, int colonne)
{
    const unsigned char *t = sqlite3_column_text(st, colonne);

    return t == NULL ? "" : (const char *)t;
}

void copier(char *dest, const char *src, size_t taille)
{
    if (taille == 0)
        return;
    strncpy(dest, src, taille - 1);
    dest[taille - 1] = '\0';
}
