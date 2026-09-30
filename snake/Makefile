CC=gcc
CFLAGS=-O2 -Wall -Wextra `sdl-config --cflags`
LIBS=`sdl-config --libs` -lm -lSDL_ttf

OBJS=main.o carte.o serpent.o affichage.o graphics.o

.PHONY: all clean
all: snake

snake: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

main.o: main.c carte.h serpent.h affichage.h graphics.h couleur.h
	$(CC) $(CFLAGS) -c main.c

carte.o: carte.c carte.h
	$(CC) $(CFLAGS) -c carte.c

serpent.o: serpent.c serpent.h carte.h
	$(CC) $(CFLAGS) -c serpent.c

affichage.o: affichage.c affichage.h carte.h serpent.h graphics.h couleur.h
	$(CC) $(CFLAGS) -c affichage.c

# Bibliotheque graphics fournie en cours (compilee sans -Wextra).
graphics.o: graphics.c graphics.h
	rm -f police.h
	touch police.h
	if test -e /usr/include/SDL_ttf.h;           then echo "#define SDL_TTF_OK" > police.h; fi
	if test -e /usr/include/SDL/SDL_ttf.h;       then echo "#define SDL_TTF_OK" > police.h; fi
	if test -e /usr/local/include/SDL_ttf.h;     then echo "#define SDL_TTF_OK" > police.h; fi
	if test -e /usr/local/include/SDL/SDL_ttf.h; then echo "#define SDL_TTF_OK" > police.h; fi
	$(CC) -O2 -Wall `sdl-config --cflags` -c graphics.c

clean:
	rm -f *.o police.h snake
