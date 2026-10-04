#include "raylib.h"

int main()
{
    constexpr int screenWidth = 1280;
    constexpr int screenHeight = 720;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "R-Type");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        const char *title = "R-Type";
        const int fontSize = 60;
        DrawText(title, (GetScreenWidth() - MeasureText(title, fontSize)) / 2,
                 GetScreenHeight() / 2 - fontSize, fontSize, RAYWHITE);
        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
