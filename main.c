#include "raylib/src/raylib.h"
#include "raylib/src/raymath.h"
#include "stdio.h"
#include "stdlib.h"

int main(void) {
  InitWindow(800, 600, "Rotating Globe");
  SetTargetFPS(60);

  Camera3D camera = {0};
  camera.position = (Vector3){-5.0f, 0.0f, 0.0f}; // Back up a bit
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  Image img = LoadImage("earth2.jpg");
  Texture2D earthTex = LoadTextureFromImage(img);
  UnloadImage(img);

  Mesh sphere = GenMeshSphere(1.5f, 128, 128); // radius, rings, slices
  Model globe = LoadModelFromMesh(sphere);

  globe.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = earthTex;
  float rotation = 0.0f;

  Vector3 defaultRotation = {1, 0, 0};

  // --- 1. Initial rotation (fixed tilt) ---
  Matrix M0 = MatrixRotateX(DEG2RAD * 270.0f);

  float spinX = 0;
  float spinY = 0;

  while (!WindowShouldClose()) {
    spinX += 0.001;

    // input handling
    if (IsKeyDown(KEY_LEFT)) {
      spinY -= 0.02;
    }
    if (IsKeyDown(KEY_RIGHT)) {
      spinY += 0.02;
    }

    if (IsKeyDown(KEY_UP)) {
      spinX += 0.02;
    }
    if (IsKeyDown(KEY_DOWN)) {
      spinX -= 0.02;
    }

    int scale = 1;
    if (IsKeyDown(KEY_Z)) {
      scale += 1;
    }
    if (IsKeyDown(KEY_X)) {
      scale -= 1;
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);

    Matrix M1 = MatrixRotateZ(spinX);
    Matrix M2 = MatrixRotateY(spinY);
    Matrix M = MatrixMultiply(MatrixMultiply(M0, M1), M2);
    globe.transform = M;
    // Rotate around Y-axis
    DrawModelEx(globe, (Vector3){0, 0, 0}, // position
                defaultRotation,           // rotation axis (Y)
                0.0f,                      // rotation angle
                (Vector3){scale, 1, 1},    // scale
                WHITE);

    EndMode3D();
    char *text = malloc(sizeof(char) * 100);
    sprintf(text, "Rotating Earth %f", rotation);

    DrawText(text, 10, 10, 20, DARKGRAY);
    EndDrawing();
  }

  UnloadTexture(earthTex);
  UnloadModel(globe);
  CloseWindow();

  return 0;
}
