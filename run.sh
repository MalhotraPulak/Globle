#!/bin/bash
eval cc -std=c11 main.c $(pkg-config --libs --cflags raylib) -o main && ./main
