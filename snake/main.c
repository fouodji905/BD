#include <stdio.h>
#include <stdlib.h>

#include "carte.h"
#include "serpent.h"
#include "affichage.h"
#include "jeu.h"
#include "graphics.h"

/* Prototypes des fonctions de ce fichier. */
int lire_taille(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    int taille = lire_taille(argc, argv);
    int taille_case;
    int en_cours = 1;
    Position fruit_test, depart;
    Serpent serpent;

    if (!carte_creer(taille)) {
        fprintf(stderr, "Erreur : memoire insuffisante pour la carte.\n");
        return EXIT_FAILURE;
    }
    carte_placer_murs();

    /* Test : un fruit pour verifier l'affichage. */
    fruit_test.ligne = taille / 2;
    fruit_test.colonne = taille / 2 + 3;
    carte_ecrire(fruit_test, FRUIT);

    /* Le serpent (3 cases) demarre au centre de la carte. */
    depart.ligne = taille / 2;
    depart.colonne = taille / 2;
    if (!serpent_creer(&serpent, depart, 3)) {
        fprintf(stderr, "Erreur : memoire insuffisante pour le serpent.\n");
        carte_detruire();
        return EXIT_FAILURE;
    }

    taille_case = affichage_ouvrir(taille);

    /* Boucle de jeu : clavier, deplacement, dessin, pause.
       (La touche Echap quitte le programme via get_arrow.) */
    while (en_cours) {
        jeu_lire_clavier(&serpent);
        en_cours = jeu_tour(&serpent);
        affichage_carte(taille_case);
        affichage_serpent(&serpent, taille_case);
        affiche_all();
        attendre(DELAI_TOUR);
    }
    serpent_detruire(&serpent);
    carte_detruire();
    return EXIT_SUCCESS;
}

/* Renvoie la taille passee en argument, ou la taille par defaut
   si aucun argument valide n'est donne. */
int lire_taille(int argc, char *argv[])
{
    int taille = TAILLE_DEFAUT;

    if (argc > 1) {
        taille = atoi(argv[1]);
        if (taille < TAILLE_MIN || taille > TAILLE_MAX) {
            fprintf(stderr, "Taille invalide (entre %d et %d). "
                    "Taille par defaut : %d.\n",
                    TAILLE_MIN, TAILLE_MAX, TAILLE_DEFAUT);
            taille = TAILLE_DEFAUT;
        }
    }
    return taille;
}
