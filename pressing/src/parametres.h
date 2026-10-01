#ifndef PARAMETRES_H
#define PARAMETRES_H

#include "base.h"

/* Parametres de l'entreprise (module 20), gardes en memoire. */
typedef struct {
    char nom[80];
    char adresse[120];
    char telephone[40];
    char identifiant_fiscal[60];
    char devise[16];
    int decimales;            /* 0 pour le FCFA, 2 pour l'euro */
    int taux_tva;             /* en centiemes de % : 1925 = 19,25 % */
    char pied_recu[200];
    int express_pct;          /* majoration du service express */
    int remise_max;           /* remise maximale d'un caissier, en % */
    Montant seuil_vip;        /* total depense pour etre client VIP */
    int tentatives_max;       /* echecs de connexion avant blocage */
    int blocage_minutes;
    int rappel_jours;         /* rappel si le linge pret n'est pas retire */
    char modele_pret[400];
    char modele_rappel[400];
} Config;

extern Config config;

void config_charger(void);
void param_ecrire(const char *cle, const char *valeur);

/* Renvoie 1 si aucun utilisateur n'existe encore. */
int premier_lancement(void);
void assistant_installation(void);
void menu_parametres(void);

/* Montants. */
const char *fmt_montant(Montant m);
const char *fmt_taux(int centiemes);
int analyser_montant(const char *texte, Montant *m);
Montant lire_montant(const char *invite);
Montant lire_montant_defaut(const char *invite, Montant defaut);

#endif
