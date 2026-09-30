#ifndef JEU_H
#define JEU_H

#include "serpent.h"

/* Duree d'un tour de jeu en millisecondes (plus petit = plus rapide). */
#define DELAI_TOUR 150

/* Lit les fleches du clavier et change la direction du serpent. */
void jeu_lire_clavier(Serpent *s);

/* Joue un tour : fait avancer le serpent.
   Renvoie 1 si la partie continue, 0 si elle est terminee. */
int jeu_tour(Serpent *s);

#endif
