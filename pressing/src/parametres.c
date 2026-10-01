#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "catalogue.h"
#include "console.h"
#include "parametres.h"
#include "securite.h"
#include "utilisateurs.h"

Config config;

#define MODELE_PRET "Bonjour {client}, votre linge (ticket {numero}) est " \
    "prêt. Reste à payer : {reste}. {pressing} - {telephone}"
#define MODELE_RAPPEL "Bonjour {client}, votre linge (ticket {numero}) " \
    "vous attend. Reste à payer : {reste}. {pressing} - {telephone}"

/* Prototypes des fonctions internes au module. */
static void lire_param(const char *cle, const char *defaut,
                       char *dest, size_t taille);
static long long lire_param_entier(const char *cle, long long defaut);
static void param_ecrire_entier(const char *cle, long long valeur);
static void afficher_parametres(void);
static void modifier_entreprise(void);
static void modifier_regles(void);
static void modifier_messages(void);
static void sauvegarder_base(void);
static int lire_taux(const char *invite, int defaut);

static void lire_param(const char *cle, const char *defaut,
                       char *dest, size_t taille)
{
    if (!req_texte(dest, taille,
                   "SELECT valeur FROM parametres WHERE cle = ?", "t", cle))
        copier(dest, defaut, taille);
}

static long long lire_param_entier(const char *cle, long long defaut)
{
    char tampon[32];

    lire_param(cle, "", tampon, sizeof tampon);
    if (tampon[0] == '\0')
        return defaut;
    return atoll(tampon);
}

void param_ecrire(const char *cle, const char *valeur)
{
    req_executer("INSERT INTO parametres (cle, valeur) VALUES (?, ?)"
                 " ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur",
                 "tt", cle, valeur);
}

static void param_ecrire_entier(const char *cle, long long valeur)
{
    char tampon[32];

    snprintf(tampon, sizeof tampon, "%lld", valeur);
    param_ecrire(cle, tampon);
}

void config_charger(void)
{
    lire_param("nom", "Mon Pressing", config.nom, sizeof config.nom);
    lire_param("adresse", "", config.adresse, sizeof config.adresse);
    lire_param("telephone", "", config.telephone, sizeof config.telephone);
    lire_param("identifiant_fiscal", "", config.identifiant_fiscal,
               sizeof config.identifiant_fiscal);
    lire_param("devise", "FCFA", config.devise, sizeof config.devise);
    config.decimales = (int)lire_param_entier("decimales", 0);
    config.taux_tva = (int)lire_param_entier("taux_tva", 0);
    lire_param("pied_recu", "Merci de votre confiance !",
               config.pied_recu, sizeof config.pied_recu);
    config.express_pct = (int)lire_param_entier("express_pct", 50);
    config.remise_max = (int)lire_param_entier("remise_max", 10);
    config.seuil_vip = lire_param_entier("seuil_vip", 10000000);
    config.tentatives_max = (int)lire_param_entier("tentatives_max", 5);
    config.blocage_minutes = (int)lire_param_entier("blocage_minutes", 15);
    config.rappel_jours = (int)lire_param_entier("rappel_jours", 3);
    lire_param("modele_pret", MODELE_PRET,
               config.modele_pret, sizeof config.modele_pret);
    lire_param("modele_rappel", MODELE_RAPPEL,
               config.modele_rappel, sizeof config.modele_rappel);
}

/* ---------- Montants ---------- */

const char *fmt_montant(Montant m)
{
    static char tampons[8][64];
    static int suivant = 0;
    char *res = tampons[suivant];
    char chiffres[32], groupe[40];
    unsigned long long absolu = (unsigned long long)(m < 0 ? -m : m);
    unsigned long long unites = absolu / 100;
    int centimes = (int)(absolu % 100);
    int n, i, j = 0;

    suivant = (suivant + 1) % 8;
    n = snprintf(chiffres, sizeof chiffres, "%llu", unites);
    /* Separateur de milliers : une espace tous les 3 chiffres. */
    for (i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0)
            groupe[j++] = ' ';
        groupe[j++] = chiffres[i];
    }
    groupe[j] = '\0';
    if (config.decimales > 0)
        snprintf(res, 64, "%s%s,%02d %s", m < 0 ? "-" : "", groupe,
                 centimes, config.devise);
    else
        snprintf(res, 64, "%s%s %s", m < 0 ? "-" : "", groupe,
                 config.devise);
    return res;
}

const char *fmt_taux(int centiemes)
{
    static char tampons[4][24];
    static int suivant = 0;
    char *res = tampons[suivant];

    suivant = (suivant + 1) % 4;
    if (centiemes % 100 == 0)
        snprintf(res, 24, "%d %%", centiemes / 100);
    else
        snprintf(res, 24, "%d,%02d %%", centiemes / 100, centiemes % 100);
    return res;
}

/* Accepte "1500", "1 500", "15,50" ou "15.50". */
int analyser_montant(const char *texte, Montant *m)
{
    long long unites = 0;
    int centimes = 0, decimales = 0, chiffres = 0, virgule = 0;
    const char *p;

    for (p = texte; *p != '\0'; p++) {
        if (*p == ' ')
            continue;
        if ((*p == ',' || *p == '.') && !virgule) {
            virgule = 1;
            continue;
        }
        if (!isdigit((unsigned char)*p))
            return 0;
        if (virgule) {
            if (decimales == 2)
                return 0;
            centimes = centimes * 10 + (*p - '0');
            decimales++;
        } else {
            if (unites > 100000000000LL)
                return 0;
            unites = unites * 10 + (*p - '0');
        }
        chiffres++;
    }
    if (chiffres == 0)
        return 0;
    if (decimales == 1)
        centimes *= 10;
    *m = unites * 100 + centimes;
    return 1;
}

Montant lire_montant(const char *invite)
{
    char tampon[64];
    Montant m;

    for (;;) {
        lire_ligne(invite, tampon, sizeof tampon);
        if (analyser_montant(tampon, &m))
            return m;
        printf("  Montant invalide (exemple : 1500 ou 15,50).\n");
    }
}

Montant lire_montant_defaut(const char *invite, Montant defaut)
{
    char tampon[64];
    Montant m;

    for (;;) {
        printf("%s [%s] : ", invite, fmt_montant(defaut));
        if (lire_ligne("", tampon, sizeof tampon) == 0)
            return defaut;
        if (analyser_montant(tampon, &m))
            return m;
        printf("  Montant invalide (exemple : 1500 ou 15,50).\n");
    }
}

/* Lit un pourcentage avec jusqu'a 2 decimales ("19,25"). */
static int lire_taux(const char *invite, int defaut)
{
    char tampon[32];
    Montant m;

    for (;;) {
        printf("%s [%s] : ", invite, fmt_taux(defaut));
        if (lire_ligne("", tampon, sizeof tampon) == 0)
            return defaut;
        if (analyser_montant(tampon, &m) && m <= 10000)
            return (int)m;
        printf("  Taux invalide (exemple : 19,25).\n");
    }
}

/* ---------- Installation ---------- */

int premier_lancement(void)
{
    return req_entier("SELECT COUNT(*) FROM utilisateurs", NULL) == 0;
}

void assistant_installation(void)
{
    char tampon[200];

    titre("SMARTPRESS - Première installation");
    printf("Bienvenue ! Configurons votre pressing.\n\n");

    lire_obligatoire("Nom du pressing : ", tampon, sizeof tampon);
    param_ecrire("nom", tampon);
    lire_ligne("Adresse : ", tampon, sizeof tampon);
    param_ecrire("adresse", tampon);
    lire_ligne("Téléphone : ", tampon, sizeof tampon);
    param_ecrire("telephone", tampon);
    lire_ligne("N° contribuable / RCCM (facultatif) : ", tampon,
               sizeof tampon);
    param_ecrire("identifiant_fiscal", tampon);
    if (lire_ligne("Devise [FCFA] : ", tampon, sizeof tampon) == 0)
        copier(tampon, "FCFA", sizeof tampon);
    param_ecrire("devise", tampon);
    param_ecrire_entier("decimales",
                        lire_entier_defaut("Nombre de décimales des prix "
                                           "(0 pour FCFA, 2 pour EUR) [0] : ",
                                           0, 2, 0));
    config_charger();
    param_ecrire_entier("taux_tva",
                        lire_taux("Taux de TVA inclus dans les prix "
                                  "(0 si non applicable)", 0));
    config_charger();

    printf("\nCréation du compte Propriétaire (accès complet).\n");
    creer_proprietaire();

    if (confirmer("\nCharger un catalogue d'exemple (chemises, costumes, "
                  "draps...) que vous pourrez modifier ?"))
        catalogue_exemple();

    printf("\nInstallation terminée. Connectez-vous avec le compte créé.\n");
}

/* ---------- Menu Parametres ---------- */

static void afficher_parametres(void)
{
    titre("Paramètres actuels");
    printf("Entreprise\n");
    printf("  Nom                  : %s\n", config.nom);
    printf("  Adresse              : %s\n", config.adresse);
    printf("  Téléphone            : %s\n", config.telephone);
    printf("  N° contribuable      : %s\n", config.identifiant_fiscal);
    printf("  Devise               : %s (%d décimale(s))\n",
           config.devise, config.decimales);
    printf("  TVA incluse          : %s\n", fmt_taux(config.taux_tva));
    printf("  Pied de reçu         : %s\n", config.pied_recu);
    printf("Règles\n");
    printf("  Majoration express   : %d %%\n", config.express_pct);
    printf("  Remise max caissier  : %d %%\n", config.remise_max);
    printf("  Seuil client VIP     : %s dépensés\n",
           fmt_montant(config.seuil_vip));
    printf("  Rappel linge prêt    : après %d jour(s)\n", config.rappel_jours);
    printf("Sécurité\n");
    printf("  Tentatives max       : %d\n", config.tentatives_max);
    printf("  Durée de blocage     : %d minutes\n", config.blocage_minutes);
    printf("Messages\n");
    printf("  Linge prêt           : %s\n", config.modele_pret);
    printf("  Rappel               : %s\n", config.modele_rappel);
}

static void modifier_entreprise(void)
{
    char tampon[200];

    titre("Entreprise et reçus");
    printf("(Entrée = garder la valeur, « - » = vider le champ)\n\n");
    lire_avec_defaut("Nom", config.nom, tampon, sizeof tampon);
    if (tampon[0] != '\0')
        param_ecrire("nom", tampon);
    lire_avec_defaut("Adresse", config.adresse, tampon, sizeof tampon);
    param_ecrire("adresse", tampon);
    lire_avec_defaut("Téléphone", config.telephone, tampon, sizeof tampon);
    param_ecrire("telephone", tampon);
    lire_avec_defaut("N° contribuable / RCCM", config.identifiant_fiscal,
                     tampon, sizeof tampon);
    param_ecrire("identifiant_fiscal", tampon);
    lire_avec_defaut("Devise", config.devise, tampon, sizeof tampon);
    if (tampon[0] != '\0')
        param_ecrire("devise", tampon);
    if (req_entier("SELECT COUNT(*) FROM commandes", NULL) == 0) {
        snprintf(tampon, sizeof tampon, "Décimales (0-2) [%d] : ",
                 config.decimales);
        param_ecrire_entier("decimales",
                            lire_entier_defaut(tampon, 0, 2,
                                               config.decimales));
    }
    param_ecrire_entier("taux_tva",
                        lire_taux("Taux de TVA inclus dans les prix",
                                  config.taux_tva));
    lire_avec_defaut("Pied de reçu", config.pied_recu, tampon, sizeof tampon);
    param_ecrire("pied_recu", tampon);
    config_charger();
    journaliser("Paramètres de l'entreprise modifiés");
    printf("  Enregistré.\n");
}

static void modifier_regles(void)
{
    char invite[96];

    titre("Règles de gestion et sécurité");
    snprintf(invite, sizeof invite, "Majoration express en %% [%d] : ",
             config.express_pct);
    param_ecrire_entier("express_pct",
                        lire_entier_defaut(invite, 0, 500,
                                           config.express_pct));
    snprintf(invite, sizeof invite, "Remise max d'un caissier en %% [%d] : ",
             config.remise_max);
    param_ecrire_entier("remise_max",
                        lire_entier_defaut(invite, 0, 100, config.remise_max));
    param_ecrire_entier("seuil_vip",
                        lire_montant_defaut("Seuil client VIP (total dépensé)",
                                            config.seuil_vip));
    snprintf(invite, sizeof invite,
             "Rappel si linge prêt non retiré après (jours) [%d] : ",
             config.rappel_jours);
    param_ecrire_entier("rappel_jours",
                        lire_entier_defaut(invite, 1, 365,
                                           config.rappel_jours));
    snprintf(invite, sizeof invite,
             "Tentatives de connexion avant blocage [%d] : ",
             config.tentatives_max);
    param_ecrire_entier("tentatives_max",
                        lire_entier_defaut(invite, 1, 20,
                                           config.tentatives_max));
    snprintf(invite, sizeof invite, "Durée de blocage en minutes [%d] : ",
             config.blocage_minutes);
    param_ecrire_entier("blocage_minutes",
                        lire_entier_defaut(invite, 1, 1440,
                                           config.blocage_minutes));
    config_charger();
    journaliser("Règles de gestion modifiées");
    printf("  Enregistré.\n");
}

static void modifier_messages(void)
{
    char tampon[400];

    titre("Modèles de messages clients");
    printf("Variables : {client} {numero} {reste} {pressing} {telephone}\n\n");
    lire_avec_defaut("Linge prêt", config.modele_pret, tampon, sizeof tampon);
    param_ecrire("modele_pret", tampon[0] ? tampon : MODELE_PRET);
    lire_avec_defaut("Rappel", config.modele_rappel, tampon, sizeof tampon);
    param_ecrire("modele_rappel", tampon[0] ? tampon : MODELE_RAPPEL);
    config_charger();
    journaliser("Modèles de messages modifiés");
    printf("  Enregistré.\n");
}

static void sauvegarder_base(void)
{
    char chemin[128], horodatage[32];
    sqlite3 *copie;
    sqlite3_backup *sauvegarde;
    time_t t = time(NULL);
    int ok = 0;

    if (!creer_dossier("sauvegardes")) {
        printf("  Impossible de créer le dossier « sauvegardes ».\n");
        return;
    }
    strftime(horodatage, sizeof horodatage, "%Y%m%d_%H%M%S", localtime(&t));
    snprintf(chemin, sizeof chemin, "sauvegardes/smartpress_%s.db",
             horodatage);
    if (sqlite3_open(chemin, &copie) == SQLITE_OK) {
        sauvegarde = sqlite3_backup_init(copie, "main", db, "main");
        if (sauvegarde != NULL) {
            ok = sqlite3_backup_step(sauvegarde, -1) == SQLITE_DONE;
            sqlite3_backup_finish(sauvegarde);
        }
    }
    sqlite3_close(copie);
    if (ok) {
        journaliser("Sauvegarde de la base : %s", chemin);
        printf("  Sauvegarde créée : %s\n", chemin);
        printf("  Copiez ce fichier sur une clé USB ou un cloud.\n");
        printf("  Pour restaurer : remplacez smartpress.db par ce fichier "
               "(application fermée).\n");
    } else {
        printf("  Échec de la sauvegarde.\n");
    }
}

void menu_parametres(void)
{
    int choix;

    for (;;) {
        titre("Paramètres");
        printf("1. Voir les paramètres\n");
        printf("2. Entreprise, TVA et reçus\n");
        printf("3. Règles de gestion et sécurité\n");
        printf("4. Modèles de messages clients\n");
        printf("5. Sauvegarder la base de données\n");
        printf("0. Retour\n");
        choix = lire_entier("Votre choix : ", 0, 5);
        switch (choix) {
        case 0: return;
        case 1: afficher_parametres(); break;
        case 2: modifier_entreprise(); break;
        case 3: modifier_regles(); break;
        case 4: modifier_messages(); break;
        case 5: sauvegarder_base(); break;
        }
        pause_console();
    }
}
