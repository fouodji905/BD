#ifndef SERPENT_H
#define SERPENT_H

#include "carte.h"

/* Les quatre directions possibles du serpent. */
typedef enum { HAUT, BAS, GAUCHE, DROITE } Direction;

/* Un maillon de la liste doublement chainee : une case du serpent. */
typedef struct Maillon {
    Position position;
    struct Maillon *suivant;    /* vers la queue */
    struct Maillon *precedent;  /* vers la tete  */
} Maillon;

/* Le serpent : une file (tete = premier maillon) et une direction. */
typedef struct {
    Maillon *tete;
    Maillon *queue;
    int longueur;
    Direction direction;
} Serpent;

/* Cree un serpent horizontal de la longueur donnee, tete a droite,
   qui part vers la droite. Renvoie 0 si la memoire manque. */
int serpent_creer(Serpent *s, Position tete, int longueur);

/* Libere tous les maillons du serpent. */
void serpent_detruire(Serpent *s);

/* Ajoute une nouvelle tete en position p. Renvoie 0 si echec. */
int serpent_ajouter_tete(Serpent *s, Position p);

/* Retire le dernier maillon (la queue). */
void serpent_retirer_queue(Serpent *s);

/* Renvoie la position de la tete. */
Position serpent_tete(const Serpent *s);

/* Renvoie 1 si le serpent occupe la case p, 0 sinon. */
int serpent_contient(const Serpent *s, Position p);

/* Renvoie la case ou arrivera la tete au prochain pas. Si elle sort
   de la carte, elle reapparait du cote oppose. */
Position serpent_case_suivante(const Serpent *s);

/* Change la direction du serpent, sauf si c'est un demi-tour. */
void serpent_changer_direction(Serpent *s, Direction d);

/* Fait avancer le serpent d'une case. Renvoie 0 si echec memoire. */
int serpent_avancer(Serpent *s);

#endif
