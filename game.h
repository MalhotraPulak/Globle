#ifndef GAME_H
#define GAME_H

#include "geodata.h"
#include "raylib/src/raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_GUESSES 500

// Distance calculation modes
typedef enum {
  DISTANCE_MODE_CENTROID,
  DISTANCE_MODE_BORDER_TO_BORDER,
  DISTANCE_MODE_COUNT  // Keep track of number of modes
} DistanceMode;

// Single guess in the game
typedef struct {
  CountryData *country;
  float distance; // Distance in kilometers
  Color color;    // Color based on distance
} Guess;

// Game state
typedef struct {
  CountryDatabase *db;
  CountryData *mysteryCountry;
  Guess guesses[MAX_GUESSES];
  int guessCount;
  bool won;
  int closestGuessIndex; // Index of closest guess so far
  char searchText[100];  // Text being typed for search
  int searchTextLength;
  bool searchActive;
  DistanceMode currentDistanceMode;  // Current distance calculation mode
} GameState;

// Distance calculation (haversine formula)
float calculateDistance(GeoPoint p1, GeoPoint p2);

// Border-to-border distance calculation
float calculateBorderToBorderDistance(CountryData *c1, CountryData *c2);

// Color calculation based on distance
Color getColorForDistance(float distance, float maxDistance);

// Game functions
void initGame(GameState *game, CountryDatabase *db);
void selectRandomMysteryCountry(GameState *game);
bool makeGuess(GameState *game, CountryData *country);
bool hasGuessed(GameState *game, CountryData *country);
void updateClosestGuess(GameState *game);

#endif // GAME_H
