# Cahier des charges — Application de mise en relation Services / Entrepreneurs (langage C)

| Élément | Valeur |
|---|---|
| Projet | **MeR** — Mise en Relation prestataires de services ↔ entrepreneurs |
| Langage | C (norme C99), compilateur `gcc` |
| Type d'application | Programme console (menus texte), données persistées dans des fichiers |
| Version du document | 1.0 — 06/10/2026 |

---

## 1. Contexte et objectif

De nombreux entrepreneurs (créateurs d'entreprise, commerçants, PME) ont besoin de
services ponctuels : comptabilité, développement web, graphisme, transport,
juridique, marketing, etc. De leur côté, les prestataires (indépendants ou
sociétés) cherchent des clients.

L'objectif est de développer **en C** une application qui :

1. recense les **prestataires** et les **services** qu'ils proposent ;
2. recense les **entrepreneurs** et leurs **besoins** ;
3. **met automatiquement en relation** un besoin avec les prestataires les plus
   pertinents (score de compatibilité) ;
4. suit le cycle de vie d'une mise en relation (proposée → acceptée → terminée →
   évaluée) ;
5. **sauvegarde** toutes les données dans des fichiers pour les retrouver au
   lancement suivant.

---

## 2. Périmètre

### 2.1 Inclus
- Gestion des comptes (prestataires, entrepreneurs, administrateur).
- Catalogue de services classés par catégorie.
- Publication de besoins par les entrepreneurs.
- Algorithme de mise en relation et classement des résultats.
- Gestion des demandes / réponses et de leur statut.
- Notation et avis après une prestation.
- Statistiques simples pour l'administrateur.
- Persistance dans des fichiers texte.

### 2.2 Exclus (hors périmètre de la version 1)
- Interface graphique ou web, application mobile.
- Paiement en ligne, facturation.
- Messagerie temps réel, réseau (client/serveur).
- Base de données SQL externe.

---

## 3. Acteurs

| Acteur | Rôle |
|---|---|
| **Prestataire** | Crée son profil, publie ses services (catégorie, tarif, zone, disponibilité), répond aux demandes. |
| **Entrepreneur** | Crée son profil, publie un besoin, consulte les prestataires proposés, envoie une demande, note le prestataire. |
| **Administrateur** | Gère les catégories, modère (suspend) les comptes, consulte les statistiques. |

---

## 4. Besoins fonctionnels

Chaque exigence porte un identifiant pour pouvoir être suivie dans les tests.

### 4.1 Gestion des comptes
- **F1** — Inscription : nom, e-mail (unique), téléphone, ville, type de compte,
  mot de passe.
- **F2** — Connexion par e-mail + mot de passe (3 tentatives maximum).
- **F3** — Le mot de passe n'est **jamais** stocké en clair : on stocke une
  empreinte (hachage, ex. djb2 ou SHA-256 si bibliothèque autorisée) avec un sel.
- **F4** — Modification et suppression de son propre profil.
- **F5** — L'administrateur peut suspendre / réactiver un compte.

### 4.2 Catalogue de services (prestataire)
- **F6** — Ajouter un service : titre, catégorie, description courte, tarif
  (min / max en FCFA ou €), unité (heure, jour, forfait), ville / zone
  d'intervention, possibilité « à distance », disponibilité (oui/non).
- **F7** — Modifier, désactiver, supprimer un de ses services.
- **F8** — Lister ses services.

### 4.3 Catégories (administrateur)
- **F9** — Ajouter, renommer, supprimer une catégorie (refus si des services y
  sont rattachés).
- **F10** — Catégories fournies par défaut : Comptabilité, Juridique,
  Informatique / Web, Graphisme, Marketing, Transport / Logistique, BTP,
  Formation.

### 4.4 Besoins (entrepreneur)
- **F11** — Publier un besoin : titre, catégorie, description, budget maximum,
  ville, accepte le « à distance » (oui/non), date limite.
- **F12** — Modifier, clôturer ou supprimer un besoin.
- **F13** — Lister ses besoins avec leur statut (`OUVERT`, `EN_COURS`, `CLOS`).

### 4.5 Recherche et mise en relation (cœur du projet)
- **F14** — Recherche manuelle de services par catégorie, ville, budget,
  mot-clé dans le titre.
- **F15** — Mise en relation automatique : pour un besoin donné, le programme
  calcule un **score de compatibilité** pour chaque service actif et affiche les
  **N meilleurs** (N = 5 par défaut), triés par score décroissant.
- **F16** — Règles éliminatoires (score = 0, service exclu) :
  - catégorie différente ;
  - service ou prestataire inactif / suspendu ;
  - tarif minimum du service > budget maximum du besoin ;
  - ville différente **et** ni le service ni le besoin n'acceptent le « à distance ».
- **F17** — Calcul du score (sur 100) pour les services retenus :

  | Critère | Points |
  |---|---|
  | Même ville | 30 |
  | À distance accepté des deux côtés (si villes différentes) | 15 |
  | Tarif dans le budget : `30 × (1 − tarif_min / budget)` + 10 | 10 à 40 |
  | Note moyenne du prestataire : `note × 4` (note sur 5) | 0 à 20 |
  | Mot-clé du titre du besoin présent dans le titre/description du service | 10 |

  Le score est plafonné à 100. En cas d'égalité, on classe par nombre d'avis
  décroissant puis par ordre alphabétique du prestataire.
- **F18** — Les pondérations sont des constantes regroupées dans un seul fichier
  d'en-tête (`matching.h`) pour pouvoir être ajustées facilement.

### 4.6 Demandes et suivi
- **F19** — Depuis la liste des résultats, l'entrepreneur envoie une **demande**
  à un prestataire (lien besoin ↔ service).
- **F20** — Le prestataire consulte ses demandes reçues et les **accepte** ou les
  **refuse** (avec un message court facultatif).
- **F21** — Statuts d'une demande : `EN_ATTENTE` → `ACCEPTEE` / `REFUSEE` →
  `TERMINEE`. Toute autre transition est refusée par le programme.
- **F22** — Quand une demande est acceptée, le besoin passe en `EN_COURS` et les
  coordonnées (e-mail, téléphone) des deux parties deviennent visibles l'une pour
  l'autre.
- **F23** — Un entrepreneur ne peut pas envoyer deux demandes identiques
  (même besoin, même service).

### 4.7 Évaluations
- **F24** — Après une demande `TERMINEE`, l'entrepreneur peut noter le
  prestataire (1 à 5) avec un commentaire (200 caractères max). Une seule note
  par demande.
- **F25** — La note moyenne et le nombre d'avis du prestataire sont mis à jour et
  utilisés dans l'algorithme de mise en relation.

### 4.8 Administration et statistiques
- **F26** — Nombre de prestataires, d'entrepreneurs, de services, de besoins
  ouverts.
- **F27** — Taux d'acceptation des demandes, top 5 des prestataires les mieux
  notés, catégories les plus demandées.
- **F28** — Export des statistiques dans un fichier texte `rapport.txt`.

---

## 5. Besoins non fonctionnels

| Id | Exigence |
|---|---|
| **NF1** | Code en C99, compilation sans aucun avertissement avec `gcc -std=c99 -Wall -Wextra`. |
| **NF2** | Programme modulaire : un couple `.c/.h` par module, pas de variable globale sauf justification. |
| **NF3** | Zéro fuite mémoire : vérification avec `valgrind --leak-check=full`. Toute allocation (`malloc`) est testée et libérée. |
| **NF4** | Robustesse des saisies : lecture avec `fgets` (jamais `gets` ni `scanf("%s")` sans limite), contrôle des types, des bornes et des longueurs, aucune saisie invalide ne doit faire planter le programme. |
| **NF5** | Les fichiers de données absents ou corrompus sont détectés : message clair, le programme démarre avec des données vides au lieu de planter. |
| **NF6** | Sauvegarde automatique à chaque modification (ou à la sortie propre du programme) via un fichier temporaire puis `rename` pour ne jamais perdre les données. |
| **NF7** | Performance : la mise en relation sur 10 000 services s'exécute en moins d'une seconde. |
| **NF8** | Code commenté en français, noms explicites, fonctions courtes (≤ 50 lignes conseillé). |
| **NF9** | Portabilité Linux (prioritaire) et Windows (MinGW) : pas d'appel système spécifique hors `stdio.h`, `stdlib.h`, `string.h`, `time.h`, `ctype.h`. |

---

## 6. Conception technique

### 6.1 Structures de données

```c
#define TAILLE_NOM    50
#define TAILLE_EMAIL  80
#define TAILLE_TEL    20
#define TAILLE_VILLE  40
#define TAILLE_TITRE  80
#define TAILLE_DESC  300

typedef enum { PRESTATAIRE, ENTREPRENEUR, ADMIN } TypeCompte;
typedef enum { OUVERT, EN_COURS, CLOS } StatutBesoin;
typedef enum { EN_ATTENTE, ACCEPTEE, REFUSEE, TERMINEE } StatutDemande;
typedef enum { HEURE, JOUR, FORFAIT } Unite;

typedef struct {
    int  id;
    char nom[TAILLE_NOM];
    char email[TAILLE_EMAIL];
    char telephone[TAILLE_TEL];
    char ville[TAILLE_VILLE];
    unsigned long empreinte_mdp;   /* hachage du mot de passe + sel */
    unsigned long sel;
    TypeCompte type;
    int  actif;                    /* 0 = suspendu */
    float note_moyenne;            /* prestataires uniquement */
    int  nb_avis;
} Utilisateur;

typedef struct {
    int  id;
    char nom[TAILLE_NOM];
} Categorie;

typedef struct {
    int   id;
    int   id_prestataire;
    int   id_categorie;
    char  titre[TAILLE_TITRE];
    char  description[TAILLE_DESC];
    float tarif_min, tarif_max;
    Unite unite;
    char  ville[TAILLE_VILLE];
    int   a_distance;
    int   actif;
} Service;

typedef struct {
    int   id;
    int   id_entrepreneur;
    int   id_categorie;
    char  titre[TAILLE_TITRE];
    char  description[TAILLE_DESC];
    float budget_max;
    char  ville[TAILLE_VILLE];
    int   a_distance;
    time_t date_limite;
    StatutBesoin statut;
} Besoin;

typedef struct {
    int   id;
    int   id_besoin;
    int   id_service;
    StatutDemande statut;
    time_t date_creation;
    char  message[TAILLE_DESC];
} Demande;

typedef struct {
    int  id;
    int  id_demande;
    int  note;                     /* 1 à 5 */
    char commentaire[200];
} Avis;

/* Résultat de la mise en relation */
typedef struct {
    const Service *service;
    int score;
} Correspondance;
```

### 6.2 Stockage en mémoire
- Chaque collection est un **tableau dynamique** (`malloc` / `realloc`, capacité
  doublée à chaque agrandissement) encapsulé dans une structure :

  ```c
  typedef struct {
      Service *elements;
      int taille;
      int capacite;
      int prochain_id;
  } ListeServices;
  ```
- Les identifiants sont générés de façon croissante et ne sont jamais réutilisés.
- Le classement des correspondances utilise `qsort` avec une fonction de
  comparaison dédiée.

### 6.3 Persistance (fichiers)
Répertoire `donnees/`, un fichier texte par collection, une ligne par
enregistrement, champs séparés par `;` (le caractère `;` est interdit à la saisie) :

| Fichier | Exemple de ligne |
|---|---|
| `utilisateurs.txt` | `3;Awa Diallo;awa@mail.com;699000000;Douala;123456789;987;0;1;4.5;12` |
| `categories.txt` | `2;Informatique / Web` |
| `services.txt` | `7;3;2;Site vitrine;Création de site...;150000;300000;2;Douala;1;1` |
| `besoins.txt` | `4;9;2;Site e-commerce;Boutique en ligne...;250000;Yaoundé;1;1767225600;0` |
| `demandes.txt` | `1;4;7;1;1759744800;Disponible dès lundi` |
| `avis.txt` | `1;1;5;Travail rapide et soigné` |

Chaque fichier commence par une ligne d'en-tête `#version=1` pour détecter un
format incompatible.

### 6.4 Découpage en modules

```
mise_en_relation/
├── Makefile
├── README.txt
├── donnees/              fichiers de données (créés au premier lancement)
├── main.c                lancement, chargement/sauvegarde, boucle des menus
├── saisie.c/.h           lecture sécurisée (entier, réel, chaîne, oui/non, date)
├── utilisateur.c/.h      comptes, connexion, hachage du mot de passe
├── categorie.c/.h        gestion des catégories
├── service.c/.h          catalogue des services
├── besoin.c/.h           besoins des entrepreneurs
├── matching.c/.h         filtres éliminatoires, calcul du score, classement
├── demande.c/.h          demandes et transitions de statut
├── avis.c/.h             notes et mise à jour des moyennes
├── stats.c/.h            statistiques et export du rapport
├── fichier.c/.h          lecture / écriture des fichiers de données
├── menu.c/.h             affichage des menus selon le type de compte
└── tests/
    └── test_matching.c   tests unitaires (assert)
```

### 6.5 Interface utilisateur (console)

```
===== MISE EN RELATION SERVICES / ENTREPRENEURS =====
1. Se connecter
2. Créer un compte
0. Quitter
Votre choix :
```

Menu **Entrepreneur** : publier un besoin · mes besoins · trouver des
prestataires (mise en relation) · rechercher un service · mes demandes · noter
un prestataire · mon profil · déconnexion.

Menu **Prestataire** : ajouter un service · mes services · demandes reçues ·
mes avis · mon profil · déconnexion.

Menu **Administrateur** : catégories · comptes (suspendre/réactiver) ·
statistiques · exporter le rapport · déconnexion.

Exemple de résultat de mise en relation :

```
Besoin #4 « Site e-commerce » — budget 250 000 FCFA — Yaoundé
Rang  Score  Prestataire        Service              Tarif           Ville    Note
 1     86    Awa Diallo         Site vitrine         150 000-300 000 Yaoundé  4.5 (12)
 2     71    Studio Kmer        Boutique WooCommerce 200 000-400 000 Douala   4.8 (30)
 3     54    J. Ngono           Dev web freelance    120 000/jour    Bafoussam 3.9 (5)
Entrez le rang pour envoyer une demande (0 = retour) :
```

---

## 7. Gestion des erreurs

| Situation | Comportement attendu |
|---|---|
| Saisie non numérique / hors bornes | Message d'erreur, nouvelle saisie. |
| Chaîne trop longue | Troncature + vidage du tampon d'entrée. |
| E-mail déjà utilisé / format invalide | Refus de l'inscription avec message. |
| Échec de `malloc` | Message, sauvegarde des données si possible, sortie propre (`EXIT_FAILURE`). |
| Fichier absent | Création d'un fichier vide au premier lancement. |
| Ligne de fichier corrompue | Ligne ignorée, avertissement avec son numéro. |
| Transition de statut interdite | Refus avec message explicite. |

---

## 8. Tests et recette

### 8.1 Tests unitaires (`make test`)
- Calcul du score : chaque règle éliminatoire, chaque critère, plafond à 100.
- Classement : ordre décroissant, départage des égalités.
- Transitions de statut des demandes (autorisées et interdites).
- Mise à jour de la note moyenne après plusieurs avis.
- Écriture puis relecture d'un fichier → données identiques.

### 8.2 Scénario de recette
1. L'admin crée la catégorie « Graphisme ».
2. Trois prestataires s'inscrivent et publient des services dans cette catégorie
   (villes et tarifs différents).
3. Un entrepreneur publie un besoin « Logo pour ma boutique », budget 50 000.
4. La mise en relation affiche uniquement les services compatibles, dans le bon
   ordre.
5. L'entrepreneur envoie une demande, le prestataire l'accepte, les coordonnées
   deviennent visibles.
6. La demande passe en `TERMINEE`, l'entrepreneur note 5/5, la moyenne est mise
   à jour.
7. On quitte, on relance : toutes les données sont présentes.
8. `valgrind` ne signale aucune fuite ni accès invalide sur ce scénario.

---

## 9. Livrables

1. Code source complet + `Makefile` (cibles `all`, `test`, `clean`).
2. `README.txt` : compilation, lancement, organisation du code, comptes de démo.
3. Jeu de données de démonstration (`donnees/` pré-rempli : 1 admin,
   5 prestataires, 5 entrepreneurs, 15 services).
4. Rapport court (5–10 pages) : choix de conception, algorithme de mise en
   relation, difficultés rencontrées, résultats des tests.

---

## 10. Planning par briques

| Brique | Contenu | Exigences |
|---|---|---|
| 1 | Structure du projet, Makefile, module `saisie`, structures de données | NF1, NF2, NF4 |
| 2 | Comptes, connexion, menus selon le type de compte | F1–F5 |
| 3 | Catégories et catalogue de services | F6–F10 |
| 4 | Besoins des entrepreneurs + recherche manuelle | F11–F14 |
| 5 | **Algorithme de mise en relation** + tests unitaires | F15–F18 |
| 6 | Demandes et suivi des statuts | F19–F23 |
| 7 | Avis et notes | F24–F25 |
| 8 | Persistance fichiers (chargement / sauvegarde) | NF5, NF6 |
| 9 | Statistiques, rapport, recette `valgrind`, documentation | F26–F28, NF3 |

---

## 11. Évolutions possibles (version 2)

- Mode client/serveur avec sockets pour plusieurs utilisateurs simultanés.
- Stockage dans une base SQLite (bibliothèque C `sqlite3`).
- Calcul de distance réelle entre villes (coordonnées GPS).
- Notifications par e-mail lors d'une nouvelle demande.
- Interface graphique (SDL, GTK).
