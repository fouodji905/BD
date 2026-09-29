#include <stdio.h>
#include <stdlib.h>

#include "carte.h"
#include "affichage.h"
#include "graphics.h"

/* Prototypes des fonctions de ce fichier. */
int lire_taille(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    int taille = lire_taille(argc, argv);
    int taille_case;
    Position fruit_test;

    if (!carte_creer(taille)) {
        fprintf(stderr, "Erreur : memoire insuffisante pour la carte.\n");
        return EXIT_FAILURE;
    }
    carte_placer_murs();

    /* Brique 1 : un fruit de test au centre pour verifier l'affichage. */
    fruit_test.ligne = taille / 2;
    fruit_test.colonne = taille / 2;
    carte_ecrire(fruit_test, FRUIT);

    taille_case = affichage_ouvrir(taille);
    affichage_carte(taille_case);
    affiche_all();

    /* Appuyer sur Echap pour fermer la fenetre. */
    wait_escape();
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
