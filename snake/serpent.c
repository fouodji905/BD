#include <stdlib.h>

#include "serpent.h"

/* Prototypes des fonctions internes au module. */
static int sont_opposees(Direction a, Direction b);

/* Cree un serpent horizontal de la longueur donnee, tete a droite,
   qui part vers la droite. Renvoie 0 si la memoire manque. */
int serpent_creer(Serpent *s, Position tete, int longueur)
{
    Position p;
    int ok = 1;

    s->tete = NULL;
    s->queue = NULL;
    s->longueur = 0;
    s->direction = DROITE;
    p.ligne = tete.ligne;
    p.colonne = tete.colonne - longueur + 1;
    while (ok && p.colonne <= tete.colonne) {
        ok = serpent_ajouter_tete(s, p);
        p.colonne++;
    }
    if (!ok)
        serpent_detruire(s);
    return ok;
}

/* Libere tous les maillons du serpent. */
void serpent_detruire(Serpent *s)
{
    while (s->tete != NULL)
        serpent_retirer_queue(s);
}

/* Ajoute une nouvelle tete en position p. Renvoie 0 si echec. */
int serpent_ajouter_tete(Serpent *s, Position p)
{
    Maillon *nouveau = malloc(sizeof(Maillon));

    if (nouveau == NULL)
        return 0;
    nouveau->position = p;
    nouveau->precedent = NULL;
    nouveau->suivant = s->tete;
    if (s->tete != NULL)
        s->tete->precedent = nouveau;
    else
        s->queue = nouveau;
    s->tete = nouveau;
    s->longueur++;
    return 1;
}

/* Retire le dernier maillon (la queue). */
void serpent_retirer_queue(Serpent *s)
{
    Maillon *ancienne = s->queue;

    if (ancienne != NULL) {
        s->queue = ancienne->precedent;
        if (s->queue != NULL)
            s->queue->suivant = NULL;
        else
            s->tete = NULL;
        free(ancienne);
        s->longueur--;
    }
}

/* Renvoie la position de la tete. */
Position serpent_tete(const Serpent *s)
{
    return s->tete->position;
}

/* Renvoie 1 si le serpent occupe la case p, 0 sinon. */
int serpent_contient(const Serpent *s, Position p)
{
    Maillon *m = s->tete;
    int trouve = 0;

    while (m != NULL && !trouve) {
        trouve = (m->position.ligne == p.ligne
                  && m->position.colonne == p.colonne);
        m = m->suivant;
    }
    return trouve;
}

/* Renvoie la case ou arrivera la tete au prochain pas. Si elle sort
   de la carte, elle reapparait du cote oppose. */
Position serpent_case_suivante(const Serpent *s)
{
    Position p = serpent_tete(s);
    int n = carte_taille();

    if (s->direction == HAUT)
        p.ligne = (p.ligne - 1 + n) % n;
    else if (s->direction == BAS)
        p.ligne = (p.ligne + 1) % n;
    else if (s->direction == GAUCHE)
        p.colonne = (p.colonne - 1 + n) % n;
    else
        p.colonne = (p.colonne + 1) % n;
    return p;
}

/* Change la direction du serpent, sauf si c'est un demi-tour. */
void serpent_changer_direction(Serpent *s, Direction d)
{
    if (!sont_opposees(s->direction, d))
        s->direction = d;
}

/* Fait avancer le serpent d'une case. Renvoie 0 si echec memoire. */
int serpent_avancer(Serpent *s)
{
    int ok = serpent_ajouter_tete(s, serpent_case_suivante(s));

    if (ok)
        serpent_retirer_queue(s);
    return ok;
}

/* Renvoie 1 si les deux directions sont opposees (demi-tour). */
static int sont_opposees(Direction a, Direction b)
{
    return (a == HAUT && b == BAS) || (a == BAS && b == HAUT)
        || (a == GAUCHE && b == DROITE) || (a == DROITE && b == GAUCHE);
}
