#include <stdlib.h>

#include "serpent.h"

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
