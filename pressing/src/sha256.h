#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t etat[8];
    uint64_t longueur;
    unsigned char bloc[64];
    size_t rempli;
} Sha256;

void sha256_init(Sha256 *c);
void sha256_ajouter(Sha256 *c, const void *donnees, size_t n);
void sha256_finir(Sha256 *c, unsigned char empreinte[32]);

#endif
