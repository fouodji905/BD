# SmartPress – Gestion de pressing en C

Application de gestion de pressing en mode console, écrite en C avec une base
de données SQLite intégrée. Elle reprend le **MVP (phases 0 et 1)** du document
technique SmartPress, adapté au langage C.

Aucune installation de base de données n'est nécessaire : SQLite est fourni
dans le dossier `sqlite/` et toutes les données sont enregistrées dans un seul
fichier, `smartpress.db`.

---

## 1. Compilation

### Windows

1. Installez **MinGW-w64** (gcc pour Windows), par exemple via
   [MSYS2](https://www.msys2.org/) : `pacman -S mingw-w64-ucrt-x86_64-gcc`,
   puis ajoutez le dossier `bin` de MinGW au `PATH`.
2. Double-cliquez sur **`compiler.bat`**. La première compilation prend 1 à
   2 minutes (compilation de SQLite).
3. Lancez **`smartpress.exe`**. Pour un affichage correct des accents,
   utilisez de préférence le *Terminal Windows*.

### Linux / macOS

```sh
make
./smartpress
```

`make clean` supprime les fichiers compilés.

---

## 2. Premier lancement

Au premier démarrage, un assistant vous demande :

- le nom, l'adresse et le téléphone du pressing, le n° contribuable ;
- la devise (FCFA par défaut) et le nombre de décimales (0 pour le FCFA) ;
- le taux de TVA inclus dans les prix (0 si non applicable) ;
- la création du compte **Propriétaire** (identifiant + mot de passe) ;
- s'il faut charger un **catalogue d'exemple** (chemise, costume, drap…),
  modifiable ensuite.

---

## 3. Une journée type

| Étape | Menu |
|---|---|
| Ouvrir la caisse avec le fond de caisse | 5. Caisse → Ouvrir |
| Recevoir un client : articles, options, express, remise, acompte → reçu | 1. Nouvelle commande |
| Faire avancer le linge : Reçu → Tri → Lavage → Séchage → Repassage → Contrôle → Prêt | 3. Suivi du traitement |
| Prévenir le client que son linge est prêt | 7. Notifications |
| Remettre le linge : encaissement du reste, puis retrait | 2. Commandes → Retrait |
| Enregistrer une dépense (lessive, transport…) | 5. Caisse → Dépense |
| Fermer la caisse : comptage, écart, rapport de clôture | 5. Caisse → Fermer |
| Consulter le chiffre d'affaires et les retards | 8. Tableaux de bord |

**Astuce atelier :** dans *Suivi du traitement → Changement en lot*, on choisit
une étape (ex. « Lavage ») puis on tape ou on **scanne** les numéros de ticket à
la suite. Une douchette code-barres USB fonctionne comme un clavier.

---

## 4. Rôles et droits

| Rôle | Accès |
|---|---|
| **Propriétaire** | Tout, y compris les paramètres et la sauvegarde |
| **Gérant** | Tout sauf les paramètres : annulations, remises libres, rapports, utilisateurs, catalogue |
| **Caissier** | Caisse, commandes, encaissements, retraits, clients, suivi atelier. Remise limitée (10 % par défaut) |
| **Opérateur** | Suivi du traitement en atelier uniquement |

Sécurité :

- mots de passe salés et hachés (SHA-256 itéré, jamais stockés en clair) ;
- au moins 8 caractères, avec des lettres et des chiffres ;
- le compte est bloqué après 5 échecs de connexion (15 minutes, réglable) ;
- un nouvel utilisateur reçoit un mot de passe provisoire, à changer à la
  première connexion ;
- un journal des connexions et un journal des actions (audit) sont tenus.

---

## 5. Fichiers produits

| Fichier / dossier | Contenu |
|---|---|
| `smartpress.db` | **Toutes les données** (à sauvegarder régulièrement) |
| `recus/` | Reçus des commandes (`P261001-001.txt`) et rapports de clôture de caisse, prêts à imprimer |
| `exports/` | Exports CSV (clients, commandes, messages), lisibles dans Excel |
| `sauvegardes/` | Copies de la base faites depuis *Paramètres → Sauvegarder* |

Pour **restaurer** une sauvegarde : fermez l'application, puis remplacez
`smartpress.db` par le fichier de sauvegarde (en le renommant).

Reçus : ils sont formatés sur 42 colonnes, ce qui convient aux imprimantes
thermiques 80 mm. Ouvrez le fichier `.txt` puis imprimez-le.

---

## 6. Correspondance avec le document technique

| Module du document | Dans cette version |
|---|---|
| 1. Authentification | Connexion, mots de passe hachés, blocage, journal des connexions |
| 2. Organisation | Un pressing par installation (nom, adresse, devise, TVA) |
| 3. Rôles et permissions | 4 rôles système, permissions par action |
| 4. Utilisateurs | Création, rôle, désactivation, réinitialisation, audit |
| 9. Clients (CRM) | Fiche, recherche, historique, notes, segments (Nouveau / Régulier / VIP / Inactif), fusion de doublons, export CSV |
| 11. Catalogue | Catégories, services, délais, options, historique des prix |
| 12. Point de vente | Panier, options, express, remise, paiement en plusieurs fois et modes multiples (espèces, Mobile Money, carte, virement), rendu monnaie, reçu, annulation avec remboursement |
| 13. Cycle de traitement | 8 statuts, historique (qui / quand), changement en lot, retards |
| 16. Trésorerie et caisse | Ouverture, encaissements, dépenses, clôture avec écart, historique |
| 18. Tableaux de bord | Tableau du jour, CA par jour et par mois, top services et clients, taux de retard, performance par employé, export |
| 19. Notifications (de base) | Messages « linge prêt » et rappels générés automatiquement, à envoyer par SMS ou WhatsApp, puis marqués comme envoyés |
| 20. Paramètres | Entreprise, TVA, reçus, règles, modèles de messages, sauvegarde |

### Différences avec le document

- **Console au lieu de Flutter** : pas d'application mobile ni web, pas de
  multi-tenant cloud (Supabase). Chaque pressing a sa propre installation.
- **Envoi des SMS et WhatsApp** : il n'est pas automatique, car il faudrait un
  compte Twilio ou équivalent. L'application prépare les messages et vous les
  envoyez vous-même.
- **QR codes** : ils ne sont pas générés. Le numéro de ticket se tape ou se
  scanne s'il est imprimé en code-barres.
- **Phases 2 à 4 non incluses** : stocks, achats, comptabilité, RH, planning,
  pointage, paie et fidélité.
- **Un seul poste** : la base est locale. Plusieurs postes sur un dossier
  réseau partagé sont possibles, mais déconseillés.

---

## 7. Organisation du code

```
pressing/
├── Makefile, compiler.bat
├── sqlite/          SQLite 3 (domaine public), compilé avec l'application
└── src/
    ├── main.c           démarrage, menu principal selon le rôle
    ├── base.c           connexion SQLite, schéma, requêtes paramétrées
    ├── console.c        saisies, mot de passe masqué, colonnes UTF-8
    ├── securite.c       rôles, permissions, connexion, hachage
    ├── sha256.c         algorithme SHA-256
    ├── parametres.c     paramètres, installation, montants, sauvegarde
    ├── utilisateurs.c   comptes et journaux
    ├── clients.c        CRM
    ├── catalogue.c      services, options, prix
    ├── commandes.c      point de vente, paiements, retrait, annulation, reçus
    ├── traitement.c     suivi atelier
    ├── caisse.c         sessions de caisse et mouvements
    ├── notifications.c  messages clients
    └── tableau_bord.c   rapports et exports
```

Les montants sont stockés en centimes (entiers) pour éviter les erreurs
d'arrondi. Toutes les requêtes SQL utilisent des paramètres liés.
