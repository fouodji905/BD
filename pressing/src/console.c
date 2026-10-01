#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <conio.h>
#include <direct.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "console.h"

/* Prototypes des fonctions internes au module. */
static void lire_brut(char *dest, size_t taille);
static void enlever_espaces(char *s);

void console_init(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
}

/* Lit une ligne telle quelle (sans le retour a la ligne).
   Quitte proprement le programme si l'entree est terminee. */
static void lire_brut(char *dest, size_t taille)
{
    size_t n;

    if (fgets(dest, (int)taille, stdin) == NULL) {
        printf("\nFin de la saisie. Au revoir.\n");
        exit(0);
    }
    n = strlen(dest);
    if (n > 0 && dest[n - 1] == '\n') {
        dest[--n] = '\0';
    } else {
        /* Ligne trop longue : on jette la suite. */
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;
    }
    if (n > 0 && dest[n - 1] == '\r')
        dest[--n] = '\0';
}

static void enlever_espaces(char *s)
{
    size_t debut = 0, n = strlen(s);

    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
    while (s[debut] != '\0' && isspace((unsigned char)s[debut]))
        debut++;
    if (debut > 0)
        memmove(s, s + debut, n - debut + 1);
}

int lire_ligne(const char *invite, char *dest, size_t taille)
{
    printf("%s", invite);
    lire_brut(dest, taille);
    enlever_espaces(dest);
    return (int)strlen(dest);
}

void lire_obligatoire(const char *invite, char *dest, size_t taille)
{
    while (lire_ligne(invite, dest, taille) == 0)
        printf("  Ce champ est obligatoire.\n");
}

void lire_avec_defaut(const char *invite, const char *actuel,
                      char *dest, size_t taille)
{
    char tampon[512];

    printf("%s [%s] : ", invite, actuel);
    lire_brut(tampon, sizeof tampon);
    enlever_espaces(tampon);
    if (tampon[0] == '\0') {
        strncpy(dest, actuel, taille - 1);
        dest[taille - 1] = '\0';
    } else if (strcmp(tampon, "-") == 0) {
        dest[0] = '\0';
    } else {
        strncpy(dest, tampon, taille - 1);
        dest[taille - 1] = '\0';
    }
}

static int analyser_entier(const char *texte, int min, int max, int *valeur)
{
    char *fin;
    long v;

    errno = 0;
    v = strtol(texte, &fin, 10);
    if (errno != 0 || fin == texte || *fin != '\0' || v < min || v > max)
        return 0;
    *valeur = (int)v;
    return 1;
}

int lire_entier(const char *invite, int min, int max)
{
    char tampon[64];
    int v;

    for (;;) {
        lire_ligne(invite, tampon, sizeof tampon);
        if (analyser_entier(tampon, min, max, &v))
            return v;
        printf("  Entrez un nombre entre %d et %d.\n", min, max);
    }
}

int lire_entier_defaut(const char *invite, int min, int max, int defaut)
{
    char tampon[64];
    int v;

    for (;;) {
        if (lire_ligne(invite, tampon, sizeof tampon) == 0)
            return defaut;
        if (analyser_entier(tampon, min, max, &v))
            return v;
        printf("  Entrez un nombre entre %d et %d.\n", min, max);
    }
}

int confirmer(const char *question)
{
    char tampon[16];

    for (;;) {
        printf("%s (o/n) : ", question);
        lire_brut(tampon, sizeof tampon);
        enlever_espaces(tampon);
        if (tampon[0] == 'o' || tampon[0] == 'O')
            return 1;
        if (tampon[0] == 'n' || tampon[0] == 'N')
            return 0;
    }
}

void lire_mot_de_passe(const char *invite, char *dest, size_t taille)
{
    printf("%s", invite);
#ifdef _WIN32
    {
        size_t n = 0;
        int c;

        while ((c = _getch()) != '\r' && c != '\n') {
            if (c == 3)
                exit(0);
            if (c == '\b') {
                if (n > 0) {
                    n--;
                    printf("\b \b");
                }
            } else if (n + 1 < taille && c >= 32) {
                dest[n++] = (char)c;
                printf("*");
            }
        }
        dest[n] = '\0';
        printf("\n");
    }
#else
    if (isatty(STDIN_FILENO)) {
        struct termios ancien, nouveau;

        tcgetattr(STDIN_FILENO, &ancien);
        nouveau = ancien;
        nouveau.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &nouveau);
        lire_brut(dest, taille);
        tcsetattr(STDIN_FILENO, TCSANOW, &ancien);
        printf("\n");
    } else {
        lire_brut(dest, taille);
    }
#endif
}

void titre(const char *texte)
{
    printf("\n==================================================\n");
    printf("  %s\n", texte);
    printf("==================================================\n");
}

void separateur(void)
{
    printf("--------------------------------------------------\n");
}

void pause_console(void)
{
    char tampon[8];

    printf("\nAppuyez sur Entree pour continuer...");
    lire_brut(tampon, sizeof tampon);
}

int utf8_longueur(const char *s)
{
    int n = 0;

    for (; *s != '\0'; s++)
        if (((unsigned char)*s & 0xC0) != 0x80)
            n++;
    return n;
}

void ecrire_col(FILE *f, const char *texte, int largeur, int a_droite)
{
    int longueur = utf8_longueur(texte);
    int i;

    if (longueur > largeur) {
        /* On coupe sur une frontiere de caractere UTF-8. */
        int vus = 0;
        const char *p = texte;

        while (*p != '\0') {
            if (((unsigned char)*p & 0xC0) != 0x80) {
                if (vus == largeur)
                    break;
                vus++;
            }
            p++;
        }
        fwrite(texte, 1, (size_t)(p - texte), f);
        return;
    }
    if (a_droite)
        for (i = longueur; i < largeur; i++)
            fputc(' ', f);
    fputs(texte, f);
    if (!a_droite)
        for (i = longueur; i < largeur; i++)
            fputc(' ', f);
}

void majuscules(char *s)
{
    for (; *s != '\0'; s++)
        *s = (char)toupper((unsigned char)*s);
}

int creer_dossier(const char *nom)
{
    struct stat infos;

    if (stat(nom, &infos) == 0)
        return 1;
#ifdef _WIN32
    return _mkdir(nom) == 0;
#else
    return mkdir(nom, 0755) == 0;
#endif
}
