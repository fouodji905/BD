#ifndef BASE_H
#define BASE_H

#include <stddef.h>

#include "sqlite3.h"

/* Les montants sont stockes en centimes (1 500 FCFA = 150000). */
typedef long long Montant;

extern sqlite3 *db;

int base_ouvrir(const char *chemin);
void base_fermer(void);
int base_exec(const char *sql);

/* Requetes avec parametres. La chaine types decrit les arguments :
   'i' = int, 'l' = long long (Montant), 't' = texte (const char *). */
sqlite3_stmt *req_preparer(const char *sql, const char *types, ...);
int req_executer(const char *sql, const char *types, ...);
long long req_entier(const char *sql, const char *types, ...);
int req_texte(char *dest, size_t taille, const char *sql,
              const char *types, ...);

long long base_dernier_id(void);
void base_debut(void);
int base_valider(void);
void base_annuler(void);

/* Texte d'une colonne ("" si NULL). */
const char *col_texte(sqlite3_stmt *st, int colonne);

/* Copie de chaine sans debordement. */
void copier(char *dest, const char *src, size_t taille);

#endif
