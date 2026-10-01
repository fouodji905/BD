#ifndef COMMANDES_H
#define COMMANDES_H

#include <stdio.h>

/* Etapes du cycle de traitement (module 13). */
typedef enum {
    ST_RECU = 0,
    ST_TRI,
    ST_LAVAGE,
    ST_SECHAGE,
    ST_REPASSAGE,
    ST_CONTROLE,
    ST_PRET,
    ST_RECUPERE
} Statut;

#define NB_STATUTS 8

const char *statut_nom(int statut);

/* Point de vente : nouvelle commande avec paiement et recu. */
void commande_nouvelle(void);

/* Menu de recherche et de gestion des commandes. */
void menu_commandes(void);

/* Renvoie l'id de la commande portant ce numero de ticket, 0 sinon. */
int commande_par_numero(const char *numero);

/* Fiche d'une commande avec ses actions (paiement, retrait...). */
void commande_detail(int id);

/* Affiche les commandes de v_commandes completees par fin_sql
   (qui peut contenir un ? remplace par param). Si ids n'est pas NULL,
   les lignes sont numerotees et leurs id ranges dans ids.
   Renvoie le nombre de lignes. */
int commandes_lister(const char *fin_sql, int param, int ids[], int max);

/* Change le statut et l'historise. Cree la notification "linge pret". */
int commande_changer_statut(int id, int nouveau);

/* Propose le statut suivant et l'applique. */
void commande_statut_interactif(int id);

#endif
