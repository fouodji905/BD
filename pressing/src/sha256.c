/* Implementation de SHA-256 (FIPS 180-4), utilisee pour proteger
   les mots de passe. */
#include <string.h>

#include "sha256.h"

#define ROTD(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void traiter_bloc(Sha256 *c, const unsigned char *b)
{
    uint32_t w[64], a, bb, cc, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[4 * i] << 24) | ((uint32_t)b[4 * i + 1] << 16)
             | ((uint32_t)b[4 * i + 2] << 8) | (uint32_t)b[4 * i + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTD(w[i - 15], 7) ^ ROTD(w[i - 15], 18)
                    ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTD(w[i - 2], 17) ^ ROTD(w[i - 2], 19)
                    ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = c->etat[0]; bb = c->etat[1]; cc = c->etat[2]; d = c->etat[3];
    e = c->etat[4]; f = c->etat[5]; g = c->etat[6]; h = c->etat[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROTD(e, 6) ^ ROTD(e, 11) ^ ROTD(e, 25))
           + ((e & f) ^ (~e & g)) + K[i] + w[i];
        t2 = (ROTD(a, 2) ^ ROTD(a, 13) ^ ROTD(a, 22))
           + ((a & bb) ^ (a & cc) ^ (bb & cc));
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = bb; bb = a; a = t1 + t2;
    }
    c->etat[0] += a; c->etat[1] += bb; c->etat[2] += cc; c->etat[3] += d;
    c->etat[4] += e; c->etat[5] += f; c->etat[6] += g; c->etat[7] += h;
}

void sha256_init(Sha256 *c)
{
    static const uint32_t initial[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    memcpy(c->etat, initial, sizeof initial);
    c->longueur = 0;
    c->rempli = 0;
}

void sha256_ajouter(Sha256 *c, const void *donnees, size_t n)
{
    const unsigned char *p = donnees;

    c->longueur += n;
    while (n > 0) {
        size_t morceau = 64 - c->rempli;

        if (morceau > n)
            morceau = n;
        memcpy(c->bloc + c->rempli, p, morceau);
        c->rempli += morceau;
        p += morceau;
        n -= morceau;
        if (c->rempli == 64) {
            traiter_bloc(c, c->bloc);
            c->rempli = 0;
        }
    }
}

void sha256_finir(Sha256 *c, unsigned char empreinte[32])
{
    uint64_t bits = c->longueur * 8;
    unsigned char fin[8];
    static const unsigned char un = 0x80, zero = 0x00;
    int i;

    sha256_ajouter(c, &un, 1);
    while (c->rempli != 56)
        sha256_ajouter(c, &zero, 1);
    for (i = 0; i < 8; i++)
        fin[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha256_ajouter(c, fin, 8);
    for (i = 0; i < 8; i++) {
        empreinte[4 * i] = (unsigned char)(c->etat[i] >> 24);
        empreinte[4 * i + 1] = (unsigned char)(c->etat[i] >> 16);
        empreinte[4 * i + 2] = (unsigned char)(c->etat[i] >> 8);
        empreinte[4 * i + 3] = (unsigned char)c->etat[i];
    }
}
