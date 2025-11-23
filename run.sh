#!/bin/bash
eval cc -std=c11 main.c geodata.c game.c $(pkg-config --libs --cflags raylib) -lm -o main && ./main
