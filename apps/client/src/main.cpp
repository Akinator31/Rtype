#include "rtype/render/Window.hpp"

#include <raylib.h>

int main()
{
    const rtype::render::Window window;

    while (!window.shouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        const char *title = "R-Type";
        const int fontSize = 60;
        DrawText(title, (window.width() - MeasureText(title, fontSize)) / 2, window.height() / 2 - fontSize,
                 fontSize, RAYWHITE);
        DrawFPS(10, 10);
        EndDrawing();
    }
    return 0;
}
