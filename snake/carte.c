#include <stdlib.h>

#include "carte.h"

/* Prototypes des fonctions internes au module. */
static void placer_segment_horizontal(int ligne, int debut, int longueur);
static void placer_segment_vertical(int colonne, int debut, int longueur);

/* La seule variable globale : le tableau qui code la carte du jeu. */
Carte carte = { 0, NULL };

/* Alloue une carte vide de la taille demandee. Renvoie 0 si echec. */
int carte_creer(int taille)
{
    int i, j;

    carte.cases = malloc(taille * sizeof(Case *));
    if (carte.cases == NULL)
        return 0;
    carte.taille = taille;
    for (i = 0; i < taille; i++) {
        carte.cases[i] = malloc(taille * sizeof(Case));
        if (carte.cases[i] == NULL) {
            carte.taille = i;
            carte_detruire();
            return 0;
        }
        for (j = 0; j < taille; j++)
            carte.cases[i][j] = VIDE;
    }
    return 1;
}

/* Libere la memoire de la carte. */
void carte_detruire(void)
{
    int i;

    for (i = 0; i < carte.taille; i++)
        free(carte.cases[i]);
    free(carte.cases);
    carte.cases = NULL;
    carte.taille = 0;
}

/* Renvoie la taille (nombre de lignes = nombre de colonnes). */
int carte_taille(void)
{
    return carte.taille;
}

/* Renvoie le contenu de la case p. */
Case carte_lire(Position p)
{
    return carte.cases[p.ligne][p.colonne];
}

/* Remplace le contenu de la case p. */
void carte_ecrire(Position p, Case contenu)
{
    carte.cases[p.ligne][p.colonne] = contenu;
}

/* Place quatre murs en "L" pres des coins, en laissant les bords
   ouverts pour que le serpent puisse traverser d'un cote a l'autre. */
void carte_placer_murs(void)
{
    int n = carte.taille;
    int marge = n / 5;
    int longueur = n / 4;

    placer_segment_horizontal(marge, marge, longueur);
    placer_segment_vertical(marge, marge, longueur);
    placer_segment_horizontal(marge, n - marge - longueur, longueur);
    placer_segment_vertical(n - marge - 1, marge, longueur);
    placer_segment_horizontal(n - marge - 1, marge, longueur);
    placer_segment_vertical(marge, n - marge - longueur, longueur);
    placer_segment_horizontal(n - marge - 1, n - marge - longueur, longueur);
    placer_segment_vertical(n - marge - 1, n - marge - longueur, longueur);
}

/* Met des murs sur une ligne, de la colonne debut sur longueur cases. */
static void placer_segment_horizontal(int ligne, int debut, int longueur)
{
    Position p;

    p.ligne = ligne;
    for (p.colonne = debut; p.colonne < debut + longueur; p.colonne++)
        carte_ecrire(p, MUR);
}

/* Met des murs sur une colonne, de la ligne debut sur longueur cases. */
static void placer_segment_vertical(int colonne, int debut, int longueur)
{
    Position p;

    p.colonne = colonne;
    for (p.ligne = debut; p.ligne < debut + longueur; p.ligne++)
        carte_ecrire(p, MUR);
}
