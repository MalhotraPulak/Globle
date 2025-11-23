#include "game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EARTH_RADIUS_KM 6371.0

// Haversine formula for great circle distance
float calculateDistance(GeoPoint p1, GeoPoint p2) {
  float lat1 = p1.lat * DEG2RAD;
  float lon1 = p1.lon * DEG2RAD;
  float lat2 = p2.lat * DEG2RAD;
  float lon2 = p2.lon * DEG2RAD;

  float dlat = lat2 - lat1;
  float dlon = lon2 - lon1;

  float a = sin(dlat / 2) * sin(dlat / 2) +
            cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);

  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  float distance = EARTH_RADIUS_KM * c;

  return distance;
}

// Color gradient: white -> blue -> yellow -> orange -> red -> green (for correct)
Color getColorForDistance(float distance, float maxDistance) {
  if (distance < 1.0f) {
    return GREEN; // Correct!
  }

  // Normalize distance to 0-1 range
  float t = distance / maxDistance;
  if (t > 1.0f)
    t = 1.0f;

  // Define color stops
  Color white = {255, 255, 255, 255};
  Color blue = {100, 149, 237, 255};    // Cornflower blue
  Color yellow = {255, 255, 0, 255};
  Color orange = {255, 165, 0, 255};
  Color red = {220, 20, 60, 255};       // Crimson

  // 5 segments: white->blue->yellow->orange->red
  if (t > 0.8f) {
    // Far away: white to light blue
    float segment = (t - 0.8f) / 0.2f;
    return (Color){
      (uint8_t)(white.r + (blue.r - white.r) * (1 - segment)),
      (uint8_t)(white.g + (blue.g - white.g) * (1 - segment)),
      (uint8_t)(white.b + (blue.b - white.b) * (1 - segment)),
      255
    };
  } else if (t > 0.6f) {
    // Blue to yellow
    float segment = (t - 0.6f) / 0.2f;
    return (Color){
      (uint8_t)(yellow.r + (blue.r - yellow.r) * segment),
      (uint8_t)(yellow.g + (blue.g - yellow.g) * segment),
      (uint8_t)(yellow.b + (blue.b - yellow.b) * segment),
      255
    };
  } else if (t > 0.4f) {
    // Yellow to orange
    float segment = (t - 0.4f) / 0.2f;
    return (Color){
      (uint8_t)(orange.r + (yellow.r - orange.r) * segment),
      (uint8_t)(orange.g + (yellow.g - orange.g) * segment),
      (uint8_t)(orange.b + (yellow.b - orange.b) * segment),
      255
    };
  } else if (t > 0.2f) {
    // Orange to red
    float segment = (t - 0.2f) / 0.2f;
    return (Color){
      (uint8_t)(red.r + (orange.r - red.r) * segment),
      (uint8_t)(red.g + (orange.g - red.g) * segment),
      (uint8_t)(red.b + (orange.b - red.b) * segment),
      255
    };
  } else {
    // Very close: red
    return red;
  }
}

// Initialize game state
void initGame(GameState *game, CountryDatabase *db) {
  game->db = db;
  game->mysteryCountry = NULL;
  game->guessCount = 0;
  game->won = false;
  game->closestGuessIndex = -1;
  game->searchTextLength = 0;
  game->searchActive = false;
  memset(game->searchText, 0, sizeof(game->searchText));
  memset(game->guesses, 0, sizeof(game->guesses));
}

// Select random mystery country
void selectRandomMysteryCountry(GameState *game) {
  if (!game->db || game->db->count == 0) {
    return;
  }

  srand(time(NULL));
  uint64_t randomIndex = rand() % game->db->count;
  game->mysteryCountry = &game->db->countries[randomIndex];

  printf("Mystery country selected: %s (at %.2f, %.2f)\n",
         game->mysteryCountry->englishName,
         game->mysteryCountry->centroid.lat,
         game->mysteryCountry->centroid.lon);
}

// Check if country has already been guessed
bool hasGuessed(GameState *game, CountryData *country) {
  for (int i = 0; i < game->guessCount; i++) {
    if (game->guesses[i].country == country) {
      return true;
    }
  }
  return false;
}

// Update which guess is closest
void updateClosestGuess(GameState *game) {
  if (game->guessCount == 0) {
    game->closestGuessIndex = -1;
    return;
  }

  float minDistance = game->guesses[0].distance;
  game->closestGuessIndex = 0;

  for (int i = 1; i < game->guessCount; i++) {
    if (game->guesses[i].distance < minDistance) {
      minDistance = game->guesses[i].distance;
      game->closestGuessIndex = i;
    }
  }
}

// Make a guess
bool makeGuess(GameState *game, CountryData *country) {
  if (!country || !game->mysteryCountry || game->won) {
    return false;
  }

  // Check if already guessed
  if (hasGuessed(game, country)) {
    printf("Already guessed: %s\n", country->englishName);
    return false;
  }

  // Calculate distance
  float distance = calculateDistance(country->centroid,
                                    game->mysteryCountry->centroid);

  // Maximum possible distance on Earth is ~20,000 km (half circumference)
  float maxDistance = 20000.0f;
  Color color = getColorForDistance(distance, maxDistance);

  // Add guess
  if (game->guessCount < MAX_GUESSES) {
    game->guesses[game->guessCount].country = country;
    game->guesses[game->guessCount].distance = distance;
    game->guesses[game->guessCount].color = color;
    game->guessCount++;
  }

  // Update closest guess
  updateClosestGuess(game);

  // Check if won
  if (country == game->mysteryCountry) {
    game->won = true;
    printf("Congratulations! You found %s in %d guesses!\n",
           game->mysteryCountry->englishName, game->guessCount);
  } else {
    printf("Guessed: %s - Distance: %.0f km\n",
           country->englishName, distance);
  }

  return true;
}
