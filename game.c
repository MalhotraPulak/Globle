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

// Calculate minimum distance from a point to a line segment on a sphere
float distanceToSegment(GeoPoint point, GeoPoint segStart, GeoPoint segEnd) {
  // For simplicity, we'll sample points along the segment and find minimum distance
  // This is an approximation but works well for border detection
  float minDist = calculateDistance(point, segStart);
  float endDist = calculateDistance(point, segEnd);
  if (endDist < minDist) minDist = endDist;

  // Sample 20 points along the segment for better accuracy
  for (int i = 1; i < 20; i++) {
    float t = i / 20.0f;
    GeoPoint samplePoint;
    samplePoint.lat = segStart.lat + t * (segEnd.lat - segStart.lat);
    samplePoint.lon = segStart.lon + t * (segEnd.lon - segStart.lon);

    float dist = calculateDistance(point, samplePoint);
    if (dist < minDist) {
      minDist = dist;
    }
  }

  return minDist;
}

// Helper: One-directional border distance
// Finds minimum distance from points in c1 to segments in c2
static float minDistanceOneDirection(CountryData *c1, CountryData *c2) {
  float minDistance = 1000000.0f;  // Very large initial value

  // Check distance from each point in c1 to each segment in c2
  for (uint64_t i = 0; i < c1->polygons->size; i++) {
    Polygon **poly1Ptr = (Polygon **)c1->polygons->p + i;
    Polygon *poly1 = *poly1Ptr;
    if (!poly1 || !poly1->points) continue;

    // For each point in polygon 1
    for (uint64_t j = 0; j < poly1->points->size; j++) {
      GeoPoint *p1 = (GeoPoint *)poly1->points->p + j;

      // Check against all segments in country 2
      for (uint64_t k = 0; k < c2->polygons->size; k++) {
        Polygon **poly2Ptr = (Polygon **)c2->polygons->p + k;
        Polygon *poly2 = *poly2Ptr;
        if (!poly2 || !poly2->points || poly2->points->size < 2) continue;

        // Check distance to each segment in polygon 2
        for (uint64_t l = 0; l < poly2->points->size - 1; l++) {
          GeoPoint *seg1 = (GeoPoint *)poly2->points->p + l;
          GeoPoint *seg2 = (GeoPoint *)poly2->points->p + l + 1;

          float dist = distanceToSegment(*p1, *seg1, *seg2);
          if (dist < minDistance) {
            minDistance = dist;

            // Early exit: if we find points within 5km, that's close enough
            if (minDistance < 5.0f) {
              return minDistance;
            }
          }
        }

        // Check closing segment
        if (poly2->points->size > 2) {
          GeoPoint *first = (GeoPoint *)poly2->points->p;
          GeoPoint *last = (GeoPoint *)poly2->points->p + (poly2->points->size - 1);
          float dist = distanceToSegment(*p1, *last, *first);
          if (dist < minDistance) {
            minDistance = dist;
            if (minDistance < 5.0f) {
              return minDistance;
            }
          }
        }
      }
    }
  }

  return minDistance;
}

// Border-to-border distance calculation (bidirectional)
// Finds minimum distance between borders of two countries
float calculateBorderToBorderDistance(CountryData *c1, CountryData *c2) {
  if (!c1 || !c2 || !c1->polygons || !c2->polygons) {
    return 0.0f;
  }

  // Check both directions to ensure we find shared borders
  float dist1 = minDistanceOneDirection(c1, c2);  // c1 points to c2 segments

  // Early exit if already found very close
  if (dist1 < 5.0f) {
    return dist1;
  }

  float dist2 = minDistanceOneDirection(c2, c1);  // c2 points to c1 segments

  // Return minimum of both directions
  return (dist1 < dist2) ? dist1 : dist2;
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
  game->currentDistanceMode = DISTANCE_MODE_BORDER_TO_BORDER;  // Default to border-to-border mode
  game->startTime = 0.0;
  game->elapsedTime = 0.0;
  game->finalScore = 0;
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

  // Calculate distance based on current mode
  float distance;
  switch (game->currentDistanceMode) {
    case DISTANCE_MODE_BORDER_TO_BORDER:
      distance = calculateBorderToBorderDistance(country, game->mysteryCountry);
      break;
    case DISTANCE_MODE_CENTROID:
    default:
      distance = calculateDistance(country->centroid,
                                   game->mysteryCountry->centroid);
      break;
  }

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

// Calculate final score based on guesses, distance, and time
// Higher score is better (max 10000 points)
int calculateScore(GameState *game) {
  if (!game->won || game->guessCount == 0) {
    return 0;
  }

  // Base score starts at 10000
  int score = 10000;

  // Penalty for number of guesses (1-50 guesses)
  // Perfect (1 guess) = 0 penalty, 50+ guesses = -5000
  int guessPenalty = (game->guessCount - 1) * 100;
  if (guessPenalty > 5000) guessPenalty = 5000;
  score -= guessPenalty;

  // Penalty for time (0-600 seconds = 0-10 minutes)
  // Fast (<60s) = 0 penalty, slow (>600s) = -3000
  int timePenalty = 0;
  if (game->elapsedTime > 60.0) {
    timePenalty = (int)((game->elapsedTime - 60.0) * 5.0);
    if (timePenalty > 3000) timePenalty = 3000;
  }
  score -= timePenalty;

  // Penalty for average distance of guesses
  // Calculate average distance (excluding correct guess with 0 distance)
  float totalDistance = 0.0f;
  int countedGuesses = 0;
  for (int i = 0; i < game->guessCount; i++) {
    if (game->guesses[i].distance > 1.0f) {  // Skip the correct guess
      totalDistance += game->guesses[i].distance;
      countedGuesses++;
    }
  }

  if (countedGuesses > 0) {
    float avgDistance = totalDistance / countedGuesses;
    // Close guesses (<1000km avg) = 0 penalty, far (>10000km avg) = -2000
    int distancePenalty = (int)(avgDistance / 5.0f);
    if (distancePenalty > 2000) distancePenalty = 2000;
    score -= distancePenalty;
  }

  // Ensure score doesn't go negative
  if (score < 0) score = 0;

  return score;
}
