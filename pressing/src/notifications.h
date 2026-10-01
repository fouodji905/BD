#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

/* Prepare le message "votre linge est pret" pour le client. */
void notification_commande_prete(int commande_id);

/* Nombre de messages en attente d'envoi. */
int notifications_en_attente(void);

void menu_notifications(void);

#endif
