#ifndef AFFICHAGE_H
#define AFFICHAGE_H

#include "carte.h"

/* Taille approximative (en pixels) de la zone de jeu a l'ecran. */
#define TAILLE_FENETRE 600

/* Ouvre la fenetre graphique adaptee a la taille de la carte.
   Renvoie la taille d'une case en pixels. */
int affichage_ouvrir(int taille_carte);

/* Dessine toute la carte (fond, murs, fruits). */
void affichage_carte(int taille_case);

#endif
