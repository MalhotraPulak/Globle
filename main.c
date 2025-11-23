#include "raylib/src/raylib.h"
#include "raylib/src/raymath.h"
#include "raylib/src/rlgl.h"
#include "geodata.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef PLATFORM_WEB
  #include <emscripten/emscripten.h>
  #include <emscripten/html5.h>
#endif

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define GLOBE_RADIUS 1.5f
#define COUNTRY_SCALE_FACTOR 1.0f  // No scaling - render at exact geographic size

// Convert lat/lon to 3D point on sphere
// Must match par_shapes coordinate system used by GenMeshSphere
Vector3 latLonToSphere(float lat, float lon, float radius) {
  // Convert lat/lon to UV parameters (same as par_shapes)
  float u = (90.0f - lat) / 180.0f;      // Latitude → UV [0,1]
  float v = (lon + 180.0f) / 360.0f;     // Longitude → UV [0,1]

  // Convert UV to spherical coordinates (matches par_shapes__sphere)
  float phi = u * PI;                     // Polar angle [0, π]
  float theta = v * 2.0f * PI;           // Azimuthal angle [0, 2π]

  // Generate 3D coordinates (same formula as par_shapes)
  return (Vector3){
    radius * cosf(theta) * sinf(phi),    // X
    radius * sinf(theta) * sinf(phi),    // Y
    radius * cosf(phi)                    // Z
  };
}

// Convert lat/lon to 3D point on sphere with scaling around centroid
Vector3 latLonToSphereScaled(float lat, float lon, GeoPoint centroid,
                              float radius, float scaleFactor) {
  // Convert vertex and centroid to 3D
  Vector3 vertex3D = latLonToSphere(lat, lon, radius);
  Vector3 centroid3D = latLonToSphere(centroid.lat, centroid.lon, radius);

  // Calculate vector from centroid to vertex
  Vector3 offset = Vector3Subtract(vertex3D, centroid3D);

  // Scale the offset vector
  offset = Vector3Scale(offset, scaleFactor);

  // Return scaled position
  return Vector3Add(centroid3D, offset);
}

// Ear clipping triangulation helpers
// Check if three consecutive vertices form a convex angle (left turn in 2D)
bool isConvexVertex(GeoPoint *prev, GeoPoint *curr, GeoPoint *next) {
  // Use cross product to determine if we have a left turn (convex)
  // Cross product: (curr - prev) × (next - curr)
  float dx1 = curr->lon - prev->lon;
  float dy1 = curr->lat - prev->lat;
  float dx2 = next->lon - curr->lon;
  float dy2 = next->lat - curr->lat;

  float cross = dx1 * dy2 - dy1 * dx2;
  return cross > 0; // Positive cross product means counter-clockwise (convex)
}

// Check if point P is inside triangle ABC using barycentric coordinates
bool pointInTriangle(GeoPoint *p, GeoPoint *a, GeoPoint *b, GeoPoint *c) {
  // Compute barycentric coordinates
  float denominator = ((b->lat - c->lat) * (a->lon - c->lon) +
                       (c->lon - b->lon) * (a->lat - c->lat));

  if (fabsf(denominator) < 0.0000001f) {
    return false; // Degenerate triangle
  }

  float u = ((b->lat - c->lat) * (p->lon - c->lon) +
             (c->lon - b->lon) * (p->lat - c->lat)) / denominator;
  float v = ((c->lat - a->lat) * (p->lon - c->lon) +
             (a->lon - c->lon) * (p->lat - c->lat)) / denominator;
  float w = 1.0f - u - v;

  // Point is inside if all barycentric coordinates are non-negative
  return (u >= 0) && (v >= 0) && (w >= 0);
}

// Check if a vertex forms a valid "ear" that can be clipped
bool isEar(GeoPoint *points, int count, int prev, int curr, int next,
           bool *active) {
  GeoPoint *p1 = &points[prev];
  GeoPoint *p2 = &points[curr];
  GeoPoint *p3 = &points[next];

  // Must be a convex vertex
  if (!isConvexVertex(p1, p2, p3)) {
    return false;
  }

  // Check that no other vertices are inside this triangle
  for (int i = 0; i < count; i++) {
    if (!active[i] || i == prev || i == curr || i == next) {
      continue;
    }

    if (pointInTriangle(&points[i], p1, p2, p3)) {
      return false;
    }
  }

  return true;
}

// Ear clipping triangulation
// Returns triangle indices (3 per triangle)
typedef struct {
  int *indices;
  int count; // Number of indices (triangles * 3)
} TriangleList;

TriangleList earClipTriangulate(GeoPoint *points, int count) {
  TriangleList result = {NULL, 0};

  if (count < 3) {
    return result;
  }

  // Allocate max possible triangles (n-2 triangles, 3 indices each)
  result.indices = (int *)malloc((count - 2) * 3 * sizeof(int));
  if (!result.indices) {
    return result;
  }

  // Track which vertices are still active (not yet clipped)
  bool *active = (bool *)malloc(count * sizeof(bool));
  if (!active) {
    free(result.indices);
    result.indices = NULL;
    return result;
  }

  for (int i = 0; i < count; i++) {
    active[i] = true;
  }

  int remainingVertices = count;
  int current = 0;
  int iterations = 0;
  const int maxIterations = count * count; // Prevent infinite loops

  while (remainingVertices > 3 && iterations < maxIterations) {
    iterations++;

    // Find next active vertex
    while (!active[current]) {
      current = (current + 1) % count;
    }

    // Find previous and next active vertices
    int prev = current;
    do {
      prev = (prev - 1 + count) % count;
    } while (!active[prev]);

    int next = current;
    do {
      next = (next + 1) % count;
    } while (!active[next]);

    // Check if current vertex is an ear
    if (isEar(points, count, prev, current, next, active)) {
      // Add triangle
      result.indices[result.count++] = prev;
      result.indices[result.count++] = current;
      result.indices[result.count++] = next;

      // Remove current vertex
      active[current] = false;
      remainingVertices--;

      // Move to next vertex
      current = next;
    } else {
      // Try next vertex
      current = next;
    }
  }

  // Add final triangle from remaining 3 vertices
  if (remainingVertices == 3) {
    int remaining[3];
    int idx = 0;
    for (int i = 0; i < count && idx < 3; i++) {
      if (active[i]) {
        remaining[idx++] = i;
      }
    }

    if (idx == 3) {
      result.indices[result.count++] = remaining[0];
      result.indices[result.count++] = remaining[1];
      result.indices[result.count++] = remaining[2];
    }
  }

  free(active);
  return result;
}

// Draw a country's polygon on the sphere (filled with triangles using ear clipping)
void drawCountryPolygonFilled(Polygon *poly, GeoPoint countryCenter,
                               float radius, float scaleFactor, Color color) {
  if (!poly || poly->points->size < 3) {
    return;
  }

  // Convert vec of GeoPoints to array for ear clipping
  int pointCount = (int)poly->points->size;
  GeoPoint *points = (GeoPoint *)poly->points->p;

  // Triangulate the polygon
  TriangleList triangles = earClipTriangulate(points, pointCount);

  if (triangles.indices == NULL || triangles.count == 0) {
    return;
  }

  // Render triangles using immediate mode
  rlBegin(RL_TRIANGLES);
  rlColor4ub(color.r, color.g, color.b, color.a);

  for (int i = 0; i < triangles.count; i += 3) {
    int idx0 = triangles.indices[i];
    int idx1 = triangles.indices[i + 1];
    int idx2 = triangles.indices[i + 2];

    GeoPoint *p0 = &points[idx0];
    GeoPoint *p1 = &points[idx1];
    GeoPoint *p2 = &points[idx2];

    Vector3 v0 = latLonToSphereScaled(p0->lat, p0->lon, countryCenter,
                                      radius, scaleFactor);
    Vector3 v1 = latLonToSphereScaled(p1->lat, p1->lon, countryCenter,
                                      radius, scaleFactor);
    Vector3 v2 = latLonToSphereScaled(p2->lat, p2->lon, countryCenter,
                                      radius, scaleFactor);

    rlVertex3f(v0.x, v0.y, v0.z);
    rlVertex3f(v1.x, v1.y, v1.z);
    rlVertex3f(v2.x, v2.y, v2.z);
  }

  rlEnd();

  // Clean up
  free(triangles.indices);
}

// Draw a country's polygon on the sphere (outline only)
void drawCountryPolygonOutline(Polygon *poly, GeoPoint countryCenter,
                                float radius, float scaleFactor, Color color) {
  if (!poly || poly->points->size < 2) {
    return;
  }

  // Draw lines connecting the points with scaled coordinates
  for (uint64_t i = 0; i < poly->points->size - 1; i++) {
    GeoPoint *p1 = (GeoPoint *)poly->points->p + i;
    GeoPoint *p2 = (GeoPoint *)poly->points->p + i + 1;

    Vector3 v1 =
        latLonToSphereScaled(p1->lat, p1->lon, countryCenter, radius, scaleFactor);
    Vector3 v2 =
        latLonToSphereScaled(p2->lat, p2->lon, countryCenter, radius, scaleFactor);

    DrawLine3D(v1, v2, color);
  }

  // Close the polygon
  GeoPoint *first = (GeoPoint *)poly->points->p;
  GeoPoint *last = (GeoPoint *)poly->points->p + (poly->points->size - 1);
  Vector3 vFirst =
      latLonToSphereScaled(first->lat, first->lon, countryCenter, radius, scaleFactor);
  Vector3 vLast =
      latLonToSphereScaled(last->lat, last->lon, countryCenter, radius, scaleFactor);
  DrawLine3D(vLast, vFirst, color);
}

// Draw a country with all its polygons (filled)
void drawCountryFilled(CountryData *country, float radius, float scaleFactor,
                       Color color) {
  if (!country || !country->polygons) {
    return;
  }

  for (uint64_t i = 0; i < country->polygons->size; i++) {
    Polygon **polyPtr = (Polygon **)country->polygons->p + i;
    Polygon *poly = *polyPtr;
    drawCountryPolygonFilled(poly, country->centroid, radius, scaleFactor, color);
  }
}

// Draw a country with all its polygons (outline only)
void drawCountryOutline(CountryData *country, float radius, float scaleFactor,
                        Color color) {
  if (!country || !country->polygons) {
    return;
  }

  for (uint64_t i = 0; i < country->polygons->size; i++) {
    Polygon **polyPtr = (Polygon **)country->polygons->p + i;
    Polygon *poly = *polyPtr;
    drawCountryPolygonOutline(poly, country->centroid, radius, scaleFactor, color);
  }
}

// Filter countries by search text
int filterCountries(CountryDatabase *db, const char *searchText,
                    CountryData **results, int maxResults) {
  int count = 0;
  int searchLen = strlen(searchText);

  if (searchLen == 0) {
    return 0;
  }

  // Convert search text to lowercase
  char lowerSearch[100];
  for (int i = 0; i < searchLen && i < 99; i++) {
    lowerSearch[i] = tolower(searchText[i]);
  }
  lowerSearch[searchLen] = '\0';

  for (uint64_t i = 0; i < db->count && count < maxResults; i++) {
    char lowerName[200];
    int nameLen = strlen(db->countries[i].englishName);
    for (int j = 0; j < nameLen && j < 199; j++) {
      lowerName[j] = tolower(db->countries[i].englishName[j]);
    }
    lowerName[nameLen] = '\0';

    if (strstr(lowerName, lowerSearch) != NULL) {
      results[count++] = &db->countries[i];
    }
  }

  return count;
}

int main(void) {
  // Initialize window
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Globle Game - Guess the Country!");
  SetTargetFPS(60);

  // Load country database
  printf("Loading country database...\n");
  CountryDatabase *db = loadCountryDatabase("./coordinates/ccc.csv");
  if (!db) {
    printf("Failed to load country database!\n");
    CloseWindow();
    return 1;
  }

  // Initialize game
  GameState game;
  initGame(&game, db);
  // Mystery country will be selected after mode selection

  // Setup 3D camera
  Camera3D camera = {0};
  float cameraDistance = 5.0f;  // Initial zoom distance
  camera.position = (Vector3){-cameraDistance, 0.0f, 0.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  // Load earth texture
  // earth2.jpg is 4096x8192 (1:2 portrait ratio)
  // This matches par_shapes UV mapping which swaps U/V from standard equirectangular
  Image img = LoadImage("earth2.jpg");
  Texture2D earthTex = LoadTextureFromImage(img);
  UnloadImage(img);

  // Create globe model
  Mesh sphere = GenMeshSphere(GLOBE_RADIUS, 128, 128);
  Model globe = LoadModelFromMesh(sphere);
  globe.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = earthTex;

  // Globe rotation
  Matrix M0 = MatrixRotateX(DEG2RAD * 270.0f);
  float spinX = 0.0f;  // User rotation around X-axis
  float spinY = 0.0f;  // User rotation around Y-axis
  float autoSpin = 0.0f;  // Auto-rotation (separate from user control)

  // Search results
  CountryData *searchResults[20];
  int searchResultCount = 0;
  int selectedSearchResult = 0;

  // Restart hold tracking
  float restartHoldTime = 0.0f;
  const float RESTART_HOLD_DURATION = 1.5f; // Seconds to hold R

  // Mode selection state
  bool modeSelectionActive = true;
  int selectedMode = 1; // Default to Border-to-Border (index 1)

  // Main game loop
  while (!WindowShouldClose()) {
    // Auto-rotation always increments (in local space)
    autoSpin += 0.001f;

    // Mouse wheel zoom (works with trackpad pinch on macOS)
    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0) {
      cameraDistance -= wheelMove * 0.5f;  // Zoom in/out
      // Clamp camera distance (min 2.0, max 10.0)
      if (cameraDistance < 2.0f) cameraDistance = 2.0f;
      if (cameraDistance > 10.0f) cameraDistance = 10.0f;
    }

    // Mouse drag rotation
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      Vector2 mouseDelta = GetMouseDelta();
      spinY += mouseDelta.x * 0.005f;  // Horizontal drag rotates around Y-axis
      spinX += mouseDelta.y * 0.005f;  // Vertical drag rotates around X-axis
    }

    // Handle keyboard input for globe rotation
    if (!modeSelectionActive) {
      if (IsKeyDown(KEY_LEFT)) spinY -= 0.02f;
      if (IsKeyDown(KEY_RIGHT)) spinY += 0.02f;
      if (IsKeyDown(KEY_UP)) spinX += 0.02f;
      if (IsKeyDown(KEY_DOWN)) spinX -= 0.02f;
    }

    // Mode selection input
    if (modeSelectionActive) {
      if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        selectedMode = (selectedMode + 1) % DISTANCE_MODE_COUNT;
      }
      if (IsKeyPressed(KEY_ENTER)) {
        game.currentDistanceMode = (DistanceMode)selectedMode;
        modeSelectionActive = false;
        selectRandomMysteryCountry(&game);  // Select mystery country after mode choice
        game.startTime = GetTime();  // Start the timer
        const char *modeNames[] = {"Centroid", "Border-to-Border"};
        printf("Distance mode selected: %s\n", modeNames[game.currentDistanceMode]);
      }
    }

    // Toggle search mode (only when not in mode selection)
    if (IsKeyPressed(KEY_ENTER) && !game.searchActive && !modeSelectionActive) {
      game.searchActive = true;
      game.searchTextLength = 0;
      game.searchText[0] = '\0';
      searchResultCount = 0;
      selectedSearchResult = 0;
    }

    // Handle search input (only when game is started, not in mode selection)
    if (game.searchActive && !modeSelectionActive) {
      // Get character input
      int key = GetCharPressed();
      while (key > 0) {
        if (key >= 32 && key <= 125 && game.searchTextLength < 99) {
          game.searchText[game.searchTextLength++] = (char)key;
          game.searchText[game.searchTextLength] = '\0';

          // Update search results
          searchResultCount = filterCountries(db, game.searchText,
                                             searchResults, 20);
          selectedSearchResult = 0;
        }
        key = GetCharPressed();
      }

      // Backspace
      if (IsKeyPressed(KEY_BACKSPACE) && game.searchTextLength > 0) {
        game.searchTextLength--;
        game.searchText[game.searchTextLength] = '\0';
        searchResultCount = filterCountries(db, game.searchText,
                                           searchResults, 20);
        selectedSearchResult = 0;
      }

      // Navigate search results
      if (IsKeyPressed(KEY_DOWN) && searchResultCount > 0) {
        selectedSearchResult = (selectedSearchResult + 1) % searchResultCount;
      }
      if (IsKeyPressed(KEY_UP) && searchResultCount > 0) {
        selectedSearchResult = (selectedSearchResult - 1 + searchResultCount) %
                               searchResultCount;
      }

      // Select country
      if (IsKeyPressed(KEY_ENTER) && searchResultCount > 0) {
        makeGuess(&game, searchResults[selectedSearchResult]);

        // If game was won, calculate score
        if (game.won && game.finalScore == 0) {
          game.elapsedTime = GetTime() - game.startTime;
          game.finalScore = calculateScore(&game);
        }

        game.searchActive = false;
        game.searchTextLength = 0;
        game.searchText[0] = '\0';
        searchResultCount = 0;
      }
    }

    // Restart game with long press 'R' key (hold for 1.5 seconds)
    if (game.won) {
      if (IsKeyDown(KEY_R)) {
        restartHoldTime += GetFrameTime();
        if (restartHoldTime >= RESTART_HOLD_DURATION) {
          // Reset game state and return to mode selection
          initGame(&game, db);
          modeSelectionActive = true;
          selectedMode = 1; // Reset to default Border-to-Border
          restartHoldTime = 0.0f;
        }
      } else {
        restartHoldTime = 0.0f; // Reset if key released
      }
    }

    // Render
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Update camera position based on zoom
    camera.position = (Vector3){-cameraDistance, 0.0f, 0.0f};

    // Draw 3D globe
    BeginMode3D(camera);

    // Apply rotation: base orientation * user rotation * auto-rotation (local)
    Matrix M_user_x = MatrixRotateZ(spinX);
    Matrix M_user_y = MatrixRotateY(spinY);
    Matrix M_auto = MatrixRotateY(autoSpin);  // Auto-rotation in local space

    // Compose: M0 (base) * M_user_x * M_user_y * M_auto (local spin)
    Matrix M_temp = MatrixMultiply(M0, M_user_x);
    M_temp = MatrixMultiply(M_temp, M_user_y);
    Matrix M = MatrixMultiply(M_temp, M_auto);
    globe.transform = M;

    // Draw globe
    DrawModel(globe, (Vector3){0, 0, 0}, 1.0f, WHITE);

    // Apply the same rotation transform for country rendering
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(M));

    // Draw guessed countries (only if game is started)
    if (!modeSelectionActive && game.mysteryCountry != NULL) {
      // Keep depth test enabled so countries on back side are hidden
      // rlDisableDepthTest();

      for (int i = 0; i < game.guessCount; i++) {
      Color drawColor = game.guesses[i].color;

      // Make closest guess brighter
      if (i == game.closestGuessIndex && !game.won) {
        drawColor.r = (drawColor.r + 255) / 2;
        drawColor.g = (drawColor.g + 255) / 2;
        drawColor.b = (drawColor.b + 255) / 2;
      }

      CountryData *country = game.guesses[i].country;

      // TODO: Re-enable filled rendering with optimized triangulation
      // For now, using only outlines for performance
      // Color fillColor = drawColor;
      // fillColor.a = 160;
      // drawCountryFilled(country, GLOBE_RADIUS, COUNTRY_SCALE_FACTOR, fillColor);

      // Draw multiple outline layers with gradient effect
      // Using more layers to create a "filled" effect (THICC mode)
      for (int j = 0; j < 40; j++) {
        float radiusOffset = 0.001f + j * 0.0003f;

        // Create gradient: darker at base, brighter at top
        float t = (float)j / 40.0f;
        Color layerColor = drawColor;

        // Darken the inner layers for depth effect
        float brightness = 0.6f + 0.4f * t;  // Range from 60% to 100%
        layerColor.r = (uint8_t)(drawColor.r * brightness);
        layerColor.g = (uint8_t)(drawColor.g * brightness);
        layerColor.b = (uint8_t)(drawColor.b * brightness);

        drawCountryOutline(country, GLOBE_RADIUS + radiusOffset,
                           COUNTRY_SCALE_FACTOR, layerColor);
      }
      }

      // Depth test stays enabled throughout
      // rlEnableDepthTest();
    }

    rlPopMatrix();  // Restore previous transform

    EndMode3D();

    // Draw UI
    const int uiMargin = 10;
    const int uiWidth = 300;

    // Title
    DrawText("GLOBLE GAME", uiMargin, uiMargin, 30, DARKBLUE);
    DrawText("Guess the mystery country!", uiMargin, uiMargin + 35, 16, GRAY);

    // Distance mode and timer display (only show if not in mode selection)
    if (!modeSelectionActive && game.mysteryCountry != NULL) {
      const char *modeNames[] = {"Centroid", "Border-to-Border"};
      DrawText(TextFormat("Mode: %s", modeNames[game.currentDistanceMode]),
               uiMargin, uiMargin + 55, 14, DARKGRAY);

      // Show timer (only when game is active)
      if (!game.won) {
        double currentTime = GetTime() - game.startTime;
        int minutes = (int)(currentTime / 60.0);
        int seconds = (int)currentTime % 60;
        DrawText(TextFormat("Time: %d:%02d", minutes, seconds),
                 uiMargin, uiMargin + 75, 14, DARKGRAY);
      } else {
        // Show final time when won
        int minutes = (int)(game.elapsedTime / 60.0);
        int seconds = (int)game.elapsedTime % 60;
        DrawText(TextFormat("Time: %d:%02d", minutes, seconds),
                 uiMargin, uiMargin + 75, 14, DARKGREEN);
      }
    }

    // Mode selection screen
    if (modeSelectionActive) {
      int boxWidth = 500;
      int boxHeight = 300;
      int boxX = (SCREEN_WIDTH - boxWidth) / 2;
      int boxY = (SCREEN_HEIGHT - boxHeight) / 2;

      // Semi-transparent background overlay
      DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 150});

      // Mode selection box
      DrawRectangle(boxX, boxY, boxWidth, boxHeight, WHITE);
      DrawRectangleLines(boxX, boxY, boxWidth, boxHeight, DARKBLUE);

      // Title
      DrawText("SELECT DISTANCE MODE", boxX + 80, boxY + 20, 24, DARKBLUE);

      // Mode options
      const char *modeNames[] = {"Centroid", "Border-to-Border"};
      const char *modeDescriptions[] = {
        "Distance from country center to center",
        "Distance from closest borders"
      };

      for (int i = 0; i < DISTANCE_MODE_COUNT; i++) {
        int optionY = boxY + 80 + i * 80;
        Color bgColor = (i == selectedMode) ? SKYBLUE : LIGHTGRAY;
        Color textColor = (i == selectedMode) ? WHITE : BLACK;

        // Option box
        DrawRectangle(boxX + 30, optionY, boxWidth - 60, 60, bgColor);
        DrawRectangleLines(boxX + 30, optionY, boxWidth - 60, 60, DARKGRAY);

        // Mode name
        DrawText(modeNames[i], boxX + 40, optionY + 10, 20, textColor);
        // Mode description
        DrawText(modeDescriptions[i], boxX + 40, optionY + 35, 14, textColor);
      }

      // Instructions
      DrawText("Use UP/DOWN to select, ENTER to confirm", boxX + 70, boxY + 260, 16, DARKGRAY);
    }

    // Instructions
    if (!game.searchActive && game.guessCount == 0 && !modeSelectionActive) {
      DrawText("Press ENTER to guess", uiMargin, uiMargin + 100, 16, DARKGRAY);
      DrawText("Drag mouse or use arrow keys", uiMargin, uiMargin + 120, 16,
               DARKGRAY);
      DrawText("to rotate globe", uiMargin, uiMargin + 140, 16, DARKGRAY);
    }

    // Search box
    if (game.searchActive) {
      DrawRectangle(uiMargin, 100, uiWidth, 40, WHITE);
      DrawRectangleLines(uiMargin, 100, uiWidth, 40, BLUE);
      DrawText(game.searchText, uiMargin + 10, 110, 20, BLACK);
      DrawText("Type country name (ESC to cancel)", uiMargin, 145, 14, GRAY);

      // Search results dropdown
      if (searchResultCount > 0) {
        int dropdownHeight = searchResultCount * 25 + 10;
        DrawRectangle(uiMargin, 150, uiWidth, dropdownHeight, WHITE);
        DrawRectangleLines(uiMargin, 150, uiWidth, dropdownHeight, DARKGRAY);

        for (int i = 0; i < searchResultCount; i++) {
          Color bgColor = (i == selectedSearchResult) ? LIGHTGRAY : WHITE;
          DrawRectangle(uiMargin + 5, 155 + i * 25, uiWidth - 10, 23, bgColor);
          DrawText(searchResults[i]->englishName, uiMargin + 10,
                   158 + i * 25, 16, BLACK);
        }
      }
    } else if (game.guessCount > 0) {
      DrawText("Press ENTER for next guess", uiMargin, 110, 16, DARKGRAY);
    }

    // Guess history (right side) - sorted by distance (only show if game started)
    if (!modeSelectionActive && game.mysteryCountry != NULL) {
      int historyX = SCREEN_WIDTH - uiWidth - uiMargin;
      int historyY = uiMargin;

      DrawText("GUESSES", historyX, historyY, 20, DARKBLUE);
      DrawText(TextFormat("Total: %d", game.guessCount), historyX,
               historyY + 25, 16, GRAY);

    // Create sorted index array (sort by distance, ascending)
    int sortedIndices[MAX_GUESSES];
    for (int i = 0; i < game.guessCount; i++) {
      sortedIndices[i] = i;
    }

    // Simple selection sort by distance
    for (int i = 0; i < game.guessCount - 1; i++) {
      for (int j = i + 1; j < game.guessCount; j++) {
        if (game.guesses[sortedIndices[j]].distance <
            game.guesses[sortedIndices[i]].distance) {
          int temp = sortedIndices[i];
          sortedIndices[i] = sortedIndices[j];
          sortedIndices[j] = temp;
        }
      }
    }

    int displayCount = game.guessCount > 15 ? 15 : game.guessCount;
    for (int i = 0; i < displayCount; i++) {
      int idx = sortedIndices[i]; // Show sorted by distance (closest first)
      int yPos = historyY + 50 + i * 40;

      // Background
      DrawRectangle(historyX, yPos, uiWidth, 35, game.guesses[idx].color);

      // Country name
      const char *name = game.guesses[idx].country->englishName;
      DrawText(name, historyX + 5, yPos + 3, 14, BLACK);

      // Distance
      if (game.guesses[idx].distance < 1.0f) {
        DrawText("CORRECT!", historyX + 5, yPos + 18, 12, DARKGREEN);
      } else {
        DrawText(TextFormat("%.0f km", game.guesses[idx].distance),
                 historyX + 5, yPos + 18, 12, BLACK);
      }

      // Closest marker
      if (idx == game.closestGuessIndex && !game.won) {
        DrawText("CLOSEST", historyX + uiWidth - 60, yPos + 10, 12, DARKBLUE);
      }
      }
    }

    // Win message
    if (game.won && game.mysteryCountry != NULL) {
      int msgWidth = 400;
      int msgHeight = 230;
      int msgX = (SCREEN_WIDTH - msgWidth) / 2;
      int msgY = (SCREEN_HEIGHT - msgHeight) / 2;

      DrawRectangle(msgX, msgY, msgWidth, msgHeight, Fade(WHITE, 0.95f));
      DrawRectangleLines(msgX, msgY, msgWidth, msgHeight, GREEN);

      DrawText("CONGRATULATIONS!", msgX + 60, msgY + 30, 28, GREEN);
      DrawText(TextFormat("You found %s!", game.mysteryCountry->englishName),
               msgX + 40, msgY + 70, 18, DARKGREEN);
      DrawText(TextFormat("Guesses: %d", game.guessCount), msgX + 130,
               msgY + 95, 18, DARKGREEN);

      // Display time
      int minutes = (int)(game.elapsedTime / 60.0);
      int seconds = (int)game.elapsedTime % 60;
      DrawText(TextFormat("Time: %d:%02d", minutes, seconds), msgX + 130,
               msgY + 115, 18, DARKGREEN);

      // Display score
      DrawText(TextFormat("SCORE: %d / 10000", game.finalScore), msgX + 90,
               msgY + 145, 20, DARKBLUE);

      DrawText("Hold 'R' to restart", msgX + 110, msgY + 170, 18, DARKGRAY);

      // Progress bar for restart hold
      if (restartHoldTime > 0.0f) {
        int barWidth = 300;
        int barHeight = 20;
        int barX = msgX + (msgWidth - barWidth) / 2;
        int barY = msgY + 195;
        float progress = restartHoldTime / RESTART_HOLD_DURATION;

        DrawRectangle(barX, barY, barWidth, barHeight, LIGHTGRAY);
        DrawRectangle(barX, barY, (int)(barWidth * progress), barHeight, GREEN);
        DrawRectangleLines(barX, barY, barWidth, barHeight, DARKGRAY);
      }
    }

    EndDrawing();
  }

  // Cleanup
  UnloadTexture(earthTex);
  UnloadModel(globe);
  freeCountryDatabase(db);
  CloseWindow();

  return 0;
}
