#include "raylib.h"
#include "Widgets.h"

int main(void)
{
    const int W = 800, H = 480;
    InitWindow(W, H, "All Widgets Demo");
    SetTargetFPS(60);

    Font uiFont = LoadFontEx("assets/fonts/InterVariable.ttf", 256, 0, 0);
    SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

    float scale = 1.0f;

    // ---- Number widget (80x80) ----
    NumberWidget num;
    num.gx = 0; num.gy = 0;
    num.label = "VALUE";
    num.value = 42;

    // ---- Indicator light (80x80) ----
    IndicatorLight ind;
    ind.gx = 1; ind.gy = 0;
    ind.label = "WARN";
    ind.onColor = RED;
    ind.offColor = DARKGRAY;

    // ---- Gauge (minimum 160x160) ----
    GaugeWidget gauge;
    gauge.gx = 0; gauge.gy = 1;     // start at tile row 1
    gauge.tilePx = 160;             // 2x2 tiles minimum (80*2)
    gauge.minValue = 0;
    gauge.maxValue = 100;
    gauge.value = 35;
    gauge.units = "psi";
    gauge.decimals = 0;

    gauge.tickCount = 6;            // 0,20,40,60,80,100
    gauge.showTickLabels = true;    // label the tick marks

    // Thresholds (ascending by value)
    gauge.thresholdCount = 3;
    gauge.thresholds[0] = GaugeThreshold{ 60.0f, GREEN };
    gauge.thresholds[1] = GaugeThreshold{ 85.0f, YELLOW };
    gauge.thresholds[2] = GaugeThreshold{ 100.0f, RED };

    while (!WindowShouldClose())
    {
        // Demo input: change values
        if (IsKeyPressed(KEY_UP)) num.value++;
        if (IsKeyPressed(KEY_DOWN)) num.value--;

        if (IsKeyDown(KEY_RIGHT)) gauge.value += 0.5f;
        if (IsKeyDown(KEY_LEFT))  gauge.value -= 0.5f;

        // Apply shared scaling to all widgets
        num.scale = scale;
        ind.scale = scale;
        gauge.scale = scale;

        // Example: number color rule
        if (num.value < 50) num.valueColor = GREEN;
        else if (num.value < 80) num.valueColor = YELLOW;
        else num.valueColor = RED;

        // Indicator follows threshold condition
        ind.on = (num.value >= 80);

        BeginDrawing();
        ClearBackground(BLACK);

        // Draw all widgets
        num.Draw(uiFont);
        ind.Draw(uiFont);
        gauge.Draw(uiFont);

        EndDrawing();
    }

    UnloadFont(uiFont);
    CloseWindow();
    return 0;
}