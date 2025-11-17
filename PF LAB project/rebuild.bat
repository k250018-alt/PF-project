@echo off
cd /d "C:\Users\User\Desktop\PF LAB project\build"
C:\msys64\usr\bin\bash.exe -i -c "gcc -o car_game -I../include $(pkg-config --cflags gtk+-3.0) ../src/main.c ../src/game.c ../src/player.c ../src/obstacle.c ../src/graphics.c $(pkg-config --libs gtk+-3.0) -lm && echo SUCCESS"
