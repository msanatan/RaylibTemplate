#include "raylib.h"
#include "raymath.h"
#include "Player.h"

// Global variables
static Camera3D camera = {0};
static Player player = Player(0.0f, 0.0f, 0.0f, 2.0f);
static Vector3 playerVelocity = {0.0f, 0.0f, 0.0f};
static const float playerSpeed = 5.0f;

// Function declarations
static void UpdateGame();
static void DrawGame();

int main()
{
  // Initialize Raylib window
  const int screenWidth = 1024;
  const int screenHeight = 768;
  InitWindow(screenWidth, screenHeight, "Moving Cube");

  // Initialize square position and size
  Rectangle square = {screenWidth / 2 - 20, screenHeight / 2 - 20, 40, 40};

  // Set up camera
  camera.position = (Vector3){0.0f, 10.0f, 10.0f}; // Camera position
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};     // Camera looking at point
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};         // Camera up vector (rotation towards target)
  camera.fovy = 45.0f;                             // Camera field-of-view Y
  camera.projection = CAMERA_PERSPECTIVE;          // Camera mode type

  SetTargetFPS(60); // Set the target frame rate to 60 FPS

  // Main game loop
  while (!WindowShouldClose())
  {
    UpdateGame();
    DrawGame();
  }

  // Close Raylib window and clean up resources
  CloseWindow();
  return 0;
}

void UpdateGame()
{
  playerVelocity = {0.0f, 0.0f, 0.0f};
  // Update player position based on arrow key input
  if (IsKeyDown(KEY_LEFT))
    playerVelocity.x -= 1.0;
  if (IsKeyDown(KEY_RIGHT))
    playerVelocity.x += 1.0;
  if (IsKeyDown(KEY_UP))
    playerVelocity.z -= 1.0;
  if (IsKeyDown(KEY_DOWN))
    playerVelocity.z += 1.0;

  playerVelocity = Vector3Scale(Vector3Normalize(playerVelocity), playerSpeed * GetFrameTime());
  player.move(playerVelocity);
}

void DrawGame()
{
  BeginDrawing();
  ClearBackground(RAYWHITE);
  BeginMode3D(camera);
  player.draw();
  EndMode3D();
  EndDrawing();
}
