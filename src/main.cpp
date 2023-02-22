#include "raylib.h"

int main()
{
    // Initialize Raylib window
    const int screenWidth = 800;
    const int screenHeight = 640;
    InitWindow(screenWidth, screenHeight, "Moving Square");

    // Initialize square position and size
    Rectangle square = {screenWidth / 2 - 20, screenHeight / 2 - 20, 40, 40};

    SetTargetFPS(60); // Set the target frame rate to 60 FPS

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update square position based on arrow key input
        if (IsKeyDown(KEY_LEFT))
            square.x -= 5;
        if (IsKeyDown(KEY_RIGHT))
            square.x += 5;
        if (IsKeyDown(KEY_UP))
            square.y -= 5;
        if (IsKeyDown(KEY_DOWN))
            square.y += 5;

        // Draw square
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRectangleRec(square, MAROON);
        EndDrawing();
    }

    // Close Raylib window and clean up resources
    CloseWindow();
    return 0;
}
