@echo off
cd /d "C:\Users\User\Desktop\PF LAB project"
C:\msys64\msys2_shell.cmd -mingw64 -no-start -c "cd 'C:/Users/User/Desktop/PF LAB project/build' && gcc -o car_game -I../include $(pkg-config --cflags gtk+-3.0) ../src/main.c ../src/game.c ../src/player.c ../src/obstacle.c ../src/graphics.c $(pkg-config --libs gtk+-3.0) -lm"
pause
