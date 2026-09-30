#include "affichage.h"
#include "graphics.h"

/* Prototypes des fonctions internes au module. */
static void dessiner_case(Position p, int taille_case, COULEUR couleur);
static void dessiner_fruit(Position p, int taille_case);
static void dessiner_contenu(Position p, int taille_case);

/* Ouvre la fenetre graphique adaptee a la taille de la carte.
   Renvoie la taille d'une case en pixels. */
int affichage_ouvrir(int taille_carte)
{
    int taille_case = TAILLE_FENETRE / taille_carte;

    init_graphics(taille_case * taille_carte, taille_case * taille_carte);
    affiche_auto_off();
    return taille_case;
}

/* Dessine toute la carte (fond, murs, fruits). */
void affichage_carte(int taille_case)
{
    Position p;
    int n = carte_taille();

    fill_screen(noir);
    for (p.ligne = 0; p.ligne < n; p.ligne++)
        for (p.colonne = 0; p.colonne < n; p.colonne++)
            dessiner_contenu(p, taille_case);
}

/* Dessine le serpent : tete en jaune, corps en vert. */
void affichage_serpent(const Serpent *s, int taille_case)
{
    Maillon *m = s->tete;

    while (m != NULL) {
        if (m == s->tete)
            dessiner_case(m->position, taille_case, jaune);
        else
            dessiner_case(m->position, taille_case, vert);
        m = m->suivant;
    }
}

/* Dessine le contenu d'une case de la carte. */
static void dessiner_contenu(Position p, int taille_case)
{
    Case contenu = carte_lire(p);

    if (contenu == MUR)
        dessiner_case(p, taille_case, gris);
    else if (contenu == FRUIT)
        dessiner_fruit(p, taille_case);
}

/* Remplit la case p d'une couleur. La ligne 0 est en haut de l'ecran
   (l'axe y de graphics est oriente vers le haut). */
static void dessiner_case(Position p, int taille_case, COULEUR couleur)
{
    POINT bas_gauche, haut_droit;

    bas_gauche.x = p.colonne * taille_case;
    bas_gauche.y = HEIGHT - (p.ligne + 1) * taille_case;
    haut_droit.x = bas_gauche.x + taille_case - 2;
    haut_droit.y = bas_gauche.y + taille_case - 2;
    draw_fill_rectangle(bas_gauche, haut_droit, couleur);
}

/* Dessine un fruit : un disque rouge au centre de la case. */
static void dessiner_fruit(Position p, int taille_case)
{
    POINT centre;

    centre.x = p.colonne * taille_case + taille_case / 2;
    centre.y = HEIGHT - p.ligne * taille_case - taille_case / 2;
    draw_fill_circle(centre, taille_case / 2 - 1, rouge);
}
