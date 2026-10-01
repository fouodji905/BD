@echo off
rem SmartPress - compilation sous Windows avec MinGW (gcc).
cd /d "%~dp0"
echo Compilation de SmartPress...
if exist sqlite\sqlite3.o goto application
echo Compilation de SQLite : 1 a 2 minutes, une seule fois...
gcc -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c sqlite\sqlite3.c -o sqlite\sqlite3.o
if errorlevel 1 goto erreur

:application
gcc -std=c99 -O2 -Wall -Isqlite -o smartpress.exe src\base.c src\caisse.c src\catalogue.c src\clients.c src\commandes.c src\console.c src\main.c src\notifications.c src\parametres.c src\securite.c src\sha256.c src\tableau_bord.c src\traitement.c src\utilisateurs.c sqlite\sqlite3.o
if errorlevel 1 goto erreur
echo.
echo Compilation reussie : double-cliquez sur smartpress.exe
pause
exit /b 0

:erreur
echo.
echo ECHEC de la compilation. Verifiez que gcc (MinGW) est installe.
pause
exit /b 1
