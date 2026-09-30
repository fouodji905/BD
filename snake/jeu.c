#include "jeu.h"
#include "graphics.h"

/* Lit les fleches du clavier et change la direction du serpent.
   (get_arrow : y positif = fleche haut, y negatif = fleche bas) */
void jeu_lire_clavier(Serpent *s)
{
    POINT fleche = get_arrow();

    if (fleche.x < 0)
        serpent_changer_direction(s, GAUCHE);
    else if (fleche.x > 0)
        serpent_changer_direction(s, DROITE);
    else if (fleche.y > 0)
        serpent_changer_direction(s, HAUT);
    else if (fleche.y < 0)
        serpent_changer_direction(s, BAS);
}

/* Joue un tour : fait avancer le serpent.
   Renvoie 1 si la partie continue, 0 si elle est terminee. */
int jeu_tour(Serpent *s)
{
    return serpent_avancer(s);
}
