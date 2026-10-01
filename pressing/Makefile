# SmartPress - compilation avec gcc (Linux, macOS, Windows/MinGW).
#   make          compile l'application
#   make clean    supprime les fichiers compiles

CC = gcc
CFLAGS = -std=c99 -O2 -Wall -Wextra -Isqlite
SQLITE_FLAGS = -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

ifeq ($(OS),Windows_NT)
    EXE = smartpress.exe
    RM = del /Q
else
    EXE = smartpress
    RM = rm -f
endif

SOURCES = $(wildcard src/*.c)
OBJETS = $(SOURCES:.c=.o)

$(EXE): $(OBJETS) sqlite/sqlite3.o
	$(CC) -o $@ $^

src/%.o: src/%.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

# SQLite est compile une seule fois (c'est le plus long).
sqlite/sqlite3.o: sqlite/sqlite3.c
	$(CC) $(SQLITE_FLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) src\*.o sqlite\sqlite3.o $(EXE)
else
	$(RM) src/*.o sqlite/sqlite3.o $(EXE)
endif

.PHONY: clean
