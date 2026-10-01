#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdio.h>
#include <stddef.h>

/* Prepare la console (UTF-8 sous Windows). */
void console_init(void);

/* Lit une ligne (sans espaces au debut et a la fin). Renvoie sa longueur. */
int lire_ligne(const char *invite, char *dest, size_t taille);

/* Lit une ligne non vide. */
void lire_obligatoire(const char *invite, char *dest, size_t taille);

/* Lit une ligne ; si l'utilisateur ne tape rien, garde la valeur actuelle. */
void lire_avec_defaut(const char *invite, const char *actuel,
                      char *dest, size_t taille);

/* Lit un entier entre min et max (redemande tant que c'est invalide). */
int lire_entier(const char *invite, int min, int max);

/* Idem, mais une ligne vide renvoie la valeur par defaut. */
int lire_entier_defaut(const char *invite, int min, int max, int defaut);

/* Pose une question oui/non. Renvoie 1 pour oui. */
int confirmer(const char *question);

/* Lit un mot de passe sans l'afficher. */
void lire_mot_de_passe(const char *invite, char *dest, size_t taille);

void titre(const char *texte);
void separateur(void);
void pause_console(void);

/* Nombre de caracteres affiches d'une chaine UTF-8. */
int utf8_longueur(const char *s);

/* Ecrit un texte dans une colonne de largeur fixe (coupe si trop long). */
void ecrire_col(FILE *f, const char *texte, int largeur, int a_droite);

void majuscules(char *s);

/* Cree un dossier s'il n'existe pas. Renvoie 1 si le dossier existe. */
int creer_dossier(const char *nom);

#endif
