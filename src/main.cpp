#include "raylib.h"
#include "Player.h"

int main()
{
  // Initialize Raylib window
  const int screenWidth = 1024;
  const int screenHeight = 768;
  InitWindow(screenWidth, screenHeight, "Moving Cube");

  // Initialize square position and size
  Rectangle square = {screenWidth / 2 - 20, screenHeight / 2 - 20, 40, 40};

  // Set up camera
  Camera3D camera = {0};
  camera.position = (Vector3){0.0f, 10.0f, 10.0f}; // Camera position
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};     // Camera looking at point
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};         // Camera up vector (rotation towards target)
  camera.fovy = 45.0f;                             // Camera field-of-view Y
  camera.projection = CAMERA_PERSPECTIVE;          // Camera mode type

  float speed = 0.0f;
  Player player = Player(0.0f, 0.0f, 0.0f, 2.0f); // Create a player object
  SetTargetFPS(60);                               // Set the target frame rate to 60 FPS

  // Main game loop
  while (!WindowShouldClose())
  {
    // Update square position based on arrow key input
    speed = GetFrameTime() * 5;
    if (IsKeyDown(KEY_LEFT))
      player.x -= speed;
    if (IsKeyDown(KEY_RIGHT))
      player.x += speed;
    if (IsKeyDown(KEY_UP))
      player.z -= speed;
    if (IsKeyDown(KEY_DOWN))
      player.z += speed;

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);
    player.draw();
    EndMode3D();
    EndDrawing();
  }

  // Close Raylib window and clean up resources
  CloseWindow();
  return 0;
}
