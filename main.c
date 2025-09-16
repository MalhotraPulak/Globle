#include "raylib.h"

int main(void) {
    InitWindow(800, 600, "Rotating Globe");
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ -5.0f, 0.0f, 0.0f }; // Back up a bit
    camera.target   = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Image img = LoadImage("earth2.jpg");
    Texture2D earthTex = LoadTextureFromImage(img);
    UnloadImage(img);

    Mesh sphere = GenMeshSphere(1.5f, 32, 32);  // radius, rings, slices
    Model globe = LoadModelFromMesh(sphere);

    globe.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = earthTex;
    float rotation = 0.0f;

    while (!WindowShouldClose()) {
        rotation += 0.2f; // Rotate speed

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                // Rotate around Y-axis
                DrawModelEx(globe,
                            (Vector3){0, 0, 0},     // position
                            (Vector3){0, 1, 0},     // rotation axis (Y)
                            rotation,               // rotation angle
                            (Vector3){1, 1, 1},     // scale
                            WHITE);

            EndMode3D();

            DrawText("Rotating Earth", 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(earthTex);
    UnloadModel(globe);
    CloseWindow();

    return 0;
}
