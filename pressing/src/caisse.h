#ifndef CAISSE_H
#define CAISSE_H

#include "base.h"

typedef enum {
    MODE_ESPECES = 1,
    MODE_MOBILE_MONEY = 2,
    MODE_CARTE = 3,
    MODE_VIREMENT = 4
} ModePaiement;

#define NB_MODES 4

const char *mode_nom(int mode);
int choisir_mode_paiement(void);

/* Id de la session de caisse ouverte, 0 si la caisse est fermee. */
int caisse_session_ouverte(void);

/* Renvoie la session ouverte ; propose de l'ouvrir si besoin. */
int caisse_exiger_session(void);

void menu_caisse(void);

#endif
