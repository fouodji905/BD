Projet Snake - Remise a niveau
==============================

Compilation : make
Lancement   : ./snake          (carte 20x20 par defaut)
              ./snake 30       (carte 30x30, taille entre 10 et 60)
Quitter     : touche Echap

Organisation du code
--------------------
  carte.c/.h      Module Carte : le tableau du jeu (murs, fruits).
                  C'est la seule variable globale du programme.
  affichage.c/.h  Module Graphique : dessin de la carte (et bientot du serpent).
  main.c          Lecture de la taille et boucle principale.
  graphics.c/.h   Bibliotheque graphique fournie en cours (SDL 1.2).

Avancement (brique par brique)
------------------------------
  [x] Brique 1 : structure du projet, Makefile, carte dynamique, affichage
  [ ] Brique 2 : module Snake (file en liste chainee) + affichage du serpent
  [ ] Brique 3 : deplacement au clavier + traversee des bords
  [ ] Brique 4 : fruits (placement aleatoire, croissance de 1)
  [ ] Brique 5 : collisions (murs, queue) et fin de partie
  [ ] Brique 6 : bonus (score, fruits speciaux, carte depuis un fichier...)
