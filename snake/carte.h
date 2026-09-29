#ifndef CARTE_H
#define CARTE_H

/* Taille de la carte : valeur par defaut et bornes autorisees. */
#define TAILLE_DEFAUT 20
#define TAILLE_MIN 10
#define TAILLE_MAX 60

/* Contenu d'une case de la carte (le serpent n'y figure pas). */
typedef enum { VIDE, MUR, FRUIT } Case;

/* Une position dans la carte : numero de ligne et de colonne. */
typedef struct {
    int ligne;
    int colonne;
} Position;

/* La carte du jeu : un tableau taille x taille alloue dynamiquement. */
typedef struct {
    int taille;
    Case **cases;
} Carte;

/* Seule variable globale du programme (definie dans carte.c). */
extern Carte carte;

/* Alloue une carte vide de la taille demandee. Renvoie 0 si echec. */
int carte_creer(int taille);

/* Libere la memoire de la carte. */
void carte_detruire(void);

/* Renvoie la taille (nombre de lignes = nombre de colonnes). */
int carte_taille(void);

/* Lit / modifie le contenu d'une case. */
Case carte_lire(Position p);
void carte_ecrire(Position p, Case contenu);

/* Place quelques murs sur la carte (sans fermer les bords). */
void carte_placer_murs(void);

#endif
