/**
 * Widgets.h            Widget Type Definitions
 *
 * @author      Justin Busker '26
 * @author      Jack Williams '26
 *
 * @copyright   Texas A&M University
 */

#pragma once
#include "raylib.h"
#include <vector>
#include <string>
#include "WidgetHelpers.h"

// Base grid tile (your screen is divisible into these)
static constexpr float BASE_TILE = 80.0f;

struct NumberWidget
{
    int gx = 0, gy = 0;
    int wTiles = 1, hTiles = 1;

    std::string label = "VALUE";
    int value = 0;
    Color valueColor = GREEN;

    float scale = 1.0f;

    Color panelFill   = Color{0, 0, 0, 180};
    Color panelBorder = Color{255, 255, 255, 80};

    float labelSize = 20.0f;
    float valueSize = 34.0f;
    float pad = 6.0f;
    float gapBelowLabel = 6.0f;
    float border = 2.0f;

    void Draw(const Font& font) const;
};

struct IndicatorLight
{
    int gx = 0, gy = 0;
    int wTiles = 1, hTiles = 1;

    std::string label = "WARN";
    bool on = false;

    float scale = 1.0f;

    Color onColor  = RED;
    Color offColor = DARKGRAY;

    Color panelFill   = Color{0, 0, 0, 180};
    Color panelBorder = Color{255, 255, 255, 80};

    float labelSize = 14.0f;
    float pad = 6.0f;
    float gapBelowLabel = 6.0f;
    float border = 2.0f;

    void Draw(const Font& font) const;
};

struct GaugeThreshold
{
    float value = 0.0f;   // threshold upper bound
    Color color = GREEN;  // used when widget.value <= value
};

struct GaugeWidget
{
    int gx = 0, gy = 0;
    int wTiles = 2, hTiles = 2;

    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 100.0f;
    std::string units;
    int decimals = 0;

    int tickCount = 6;
    bool showTickLabels = true;

    GaugeThreshold thresholds[8];
    int thresholdCount = 0;

    bool  alarm = false;
    float criticalThreshold = 0.0f;

    float scale = 1.0f;

    Color panelFill   = Color{0, 0, 0, 180};
    Color panelBorder = Color{255, 255, 255, 80};

    Color ringBackColor = Color{60, 60, 60, 255};
    Color tickColor     = Color{255, 255, 255, 160};
    Color textColor     = RAYWHITE;

    float border = 2.0f;
    float pad = 6.0f;

    float startDeg = 135.0f;
    float endDeg   = 405.0f;

    float ringThickness = 9.0f;
    float ringMargin = 20.0f;       // extra inset from panel edge — increase to shrink the ring
    float tickLabelOffset = 14.0f;  // gap between ring outer edge and tick label — increase for more spacing

    float tickLabelSize = 15.0f;
    float valueTextSize = 32.0f;
    float unitsTextSize = 30.0f;

    void Draw(const Font& font) const;
};

// -----------------------------
// BarGraphWidget (minimum 2x3 tiles)
// Vertical bar with ticks + labels + thresholds + center value + units
// -----------------------------
struct BarGraphWidget
{
    int gx = 0, gy = 0;
    int wTiles = 2, hTiles = 3;   // minimum 160x240

    // Data
    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 100.0f;
    std::string units;
    int decimals = 0;

    // Ticks
    int tickCount = 6;            // e.g. 6 => min..max evenly spaced
    bool showTickLabels = true;

    // Threshold colors (sorted ascending)
    GaugeThreshold thresholds[8];
    int thresholdCount = 0;

    bool  alarm = false;
    float criticalThreshold = 0.0f;

    float scale = 1.0f;

    // Style
    Color panelFill   = Color{0, 0, 0, 180};
    Color panelBorder = Color{255, 255, 255, 80};

    Color barBackColor = Color{60, 60, 60, 255};
    Color tickColor    = Color{255, 255, 255, 160};
    Color textColor    = RAYWHITE;

    float border = 2.0f;

    // ---- Permanent layout changes you wanted ----
    float pad = 12.0f;            // more padding (top/bottom/left/right)

    // Wider bar
    float barWidth = 26.0f;       // was 18

    // Put bar more centered (still leaving room for tick labels)
    float barXFrac = 0.45f;       // was 0.30

    // Bigger tick labels
    float tickLabelSize = 16.0f;  // was 9

    // Text sizing (bottom value + units)
    float valueTextSize = 28.0f;
    float unitsTextSize = 14.0f;

    void Draw(const Font& font) const;
};

struct HorizontalBarGraphWidget
{
    int gx = 0, gy = 0;
    int wTiles = 3, hTiles = 2;   // minimum 240x160

    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 100.0f;
    const char* units = "";
    int decimals = 0;

    int tickCount = 6;
    bool showTickLabels = true;

    GaugeThreshold thresholds[8];
    int thresholdCount = 0;

    bool  alarm = false;
    float criticalThreshold = 0.0f;

    float scale = 1.0f;

    Color panelFill   = Color{0, 0, 0, 180};
    Color panelBorder = Color{255, 255, 255, 80};

    Color barBackColor = Color{60, 60, 60, 255};
    Color tickColor    = Color{255, 255, 255, 160};
    Color textColor    = RAYWHITE;

    float border = 2.0f;
    float pad = 12.0f;

    float barHeight = 26.0f;
    float tickLabelSize = 16.0f;

    float valueTextSize = 28.0f;
    float unitsTextSize = 14.0f;

    void Draw(const Font& font) const;
};

struct GraphMarker
{
    float x = 0.0f;          // graph-space x
    float y = 0.0f;          // graph-space y
    float radius = 4.0f;     // px @ scale=1
    Color color = YELLOW;
    bool filled = true;
};

struct GraphSeries
{
    std::vector<Vector2> points;          // each point is graph-space (x,y)
    std::vector<GraphMarker> markers;     // optional dots on this line

    Color color = GREEN;
    float thickness = 2.0f;               // px @ scale=1
    bool visible = true;
};

struct GraphWidget
{
    int gx = 0, gy = 0;
    int wTiles = 4, hTiles = 3;           // default 320x240

    float scale = 1.0f;

    // Axis ranges
    float xMin = 0.0f;
    float xMax = 100.0f;
    float yMin = 0.0f;
    float yMax = 100.0f;

    // Axis unit labels
    const char* xUnits = "";
    const char* yUnits = "";

    // Tick counts
    int xTickCount = 6;
    int yTickCount = 6;

    // Optional labeling
    bool showXTickLabels = true;
    bool showYTickLabels = true;
    bool showXUnits = true;
    bool showYUnits = true;

    // Styling
    Color panelFill   = Color{0, 0, 0, 180};
    Color panelBorder = Color{255, 255, 255, 80};

    Color axisColor = RAYWHITE;
    Color gridColor = Color{255, 255, 255, 50};
    Color textColor = RAYWHITE;
    Color plotBackColor = Color{20, 20, 20, 220};

    bool showGrid = true;

    float border = 2.0f;
    float pad = 10.0f;

    // Text sizing
    float tickLabelSize = 16.0f;
    float axisLabelSize = 16.0f;

    // Plot margin inside panel
    float leftMargin = 36.0f;
    float rightMargin = 12.0f;
    float topMargin = 12.0f;
    float bottomMargin = 34.0f;

    // Data
    std::vector<GraphSeries> series;

    void Draw(const Font& font) const;
};