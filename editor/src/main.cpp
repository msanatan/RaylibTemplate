#include "raylib.h"

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(1280, 720, "Tuni");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(DARKGRAY);
    DrawText("Tuni", 24, 24, 24, RAYWHITE);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
