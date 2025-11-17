#!/bin/bash
export PATH=/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH
cd '/c/Users/User/Desktop/PF LAB project/build'
gcc -o car_game -I../include $(pkg-config --cflags gtk+-3.0) ../src/main.c ../src/game.c ../src/player.c ../src/obstacle.c ../src/graphics.c $(pkg-config --libs gtk+-3.0) -lm 2>&1
echo "Build status: $?"
ls -lh car_game.exe 2>&1 || echo "Build failed"
