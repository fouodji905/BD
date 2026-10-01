#ifndef CLIENTS_H
#define CLIENTS_H

#include <stdio.h>

/* Recherche un client ou en cree un. Renvoie son id, 0 si abandon. */
int client_choisir(void);

/* Segment du client : Nouveau, Regulier, VIP, Inactif. */
const char *client_segment(int client_id);

void menu_clients(void);

/* Ecrit un champ CSV (separateur ;) avec les guillemets necessaires. */
void csv_champ(FILE *f, const char *texte, int dernier);

/* Ecrit un montant (centimes) au format Excel francais : 7200,00. */
void csv_montant(FILE *f, long long centimes, int dernier);

#endif
