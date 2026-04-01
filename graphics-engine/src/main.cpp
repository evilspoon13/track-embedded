/**
 * main.cpp             Graphics Engine Process
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 * @author      Justin Busker '26
 * @author      Jack Williams '26
 *
 * @copyright   Texas A&M University
 */

#include "WidgetFactory.h"
#include "Widgets.h"
#include "config_parser.hpp"
#include "raylib.h"
#include "telemetry_queue.hpp"
#include "shared_memory.hpp"

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <string>

static volatile sig_atomic_t reload_flag = 0;

static void sighup_handler(int) { reload_flag = 1; }

static void update_all_screens(std::vector<LiveScreen>& screens, const TelemetryMessage& msg) {
  for (auto& screen : screens) {
    for (auto& lw : screen.widgets) {
      if (lw.can_id == msg.can_id && lw.signal == msg.signal_name) {
        lw.set_value(msg.value);
      }
    }
  }
}

static void handle_screen_navigation(const std::vector<LiveScreen>& screens, std::size_t& active_screen) {
  if (screens.empty()) return;

  if (IsKeyPressed(KEY_RIGHT)) {
    active_screen = (active_screen + 1) % screens.size();
  }

  if (IsKeyPressed(KEY_LEFT)) {
    active_screen = (active_screen + screens.size() - 1) % screens.size();
  }
}

// ── Demo mode ────────────────────────────────────────────────────────────────
// Run with:  ./graphics-engine --demo
// Shows one example of every widget type so font sizes can be tuned visually.
// Layout uses 800×480 (10×6 BASE_TILE=80 grid):
//
//   col: 0  1  2  3  4  5  6  7  8  9
//   row0 [Gauge  2x2][Bar 2x6][HorizBar      6x2    ]
//   row1 [            ][      ][                     ]
//   row2 [Num    2x2  ][      ][Graph         6x4    ]
//   row3 [            ][      ][                     ]
//   row4 [ON][OFF     ][      ][                     ]
//   row5 [Num    2x1  ][      ][                     ]
//
static void run_demo(const Font& font) {
  // ── Gauge ─────────────────────────────────────────────────────────────────
  GaugeWidget gauge;
  gauge.gx = 0; gauge.gy = 0; gauge.wTiles = 2; gauge.hTiles = 2;
  gauge.value = 62.0f; gauge.minValue = 0.0f; gauge.maxValue = 100.0f;
  gauge.units = "PSI"; gauge.decimals = 0; gauge.tickCount = 6;
  gauge.thresholdCount = 3;
  gauge.thresholds[0] = {33.0f,  GREEN};
  gauge.thresholds[1] = {66.0f,  YELLOW};
  gauge.thresholds[2] = {100.0f, RED};
  gauge.Draw(font);

  // ── NumberWidget (large, RPM) ─────────────────────────────────────────────
  NumberWidget rpm;
  rpm.gx = 0; rpm.gy = 2; rpm.wTiles = 2; rpm.hTiles = 2;
  rpm.label = "RPM"; rpm.value = 6842; rpm.valueColor = GREEN;
  rpm.Draw(font);

  // ── IndicatorLight ON ─────────────────────────────────────────────────────
  IndicatorLight litOn;
  litOn.gx = 0; litOn.gy = 4; litOn.wTiles = 1; litOn.hTiles = 1;
  litOn.label = "OIL"; litOn.on = true; litOn.onColor = RED;
  litOn.Draw(font);

  // ── IndicatorLight OFF ────────────────────────────────────────────────────
  IndicatorLight litOff;
  litOff.gx = 1; litOff.gy = 4; litOff.wTiles = 1; litOff.hTiles = 1;
  litOff.label = "ABS"; litOff.on = false; litOff.onColor = YELLOW;
  litOff.Draw(font);

  // ── NumberWidget (small, voltage) ─────────────────────────────────────────
  NumberWidget volt;
  volt.gx = 0; volt.gy = 5; volt.wTiles = 2; volt.hTiles = 1;
  volt.label = "VOLTS"; volt.value = 13; volt.valueColor = SKYBLUE;
  volt.Draw(font);

  // ── BarGraphWidget (vertical, fuel) ───────────────────────────────────────
  BarGraphWidget bar;
  bar.gx = 2; bar.gy = 0; bar.wTiles = 2; bar.hTiles = 6;
  bar.value = 45.0f; bar.minValue = 0.0f; bar.maxValue = 100.0f;
  bar.units = "%"; bar.decimals = 0; bar.tickCount = 6;
  bar.thresholdCount = 3;
  bar.thresholds[0] = {25.0f,  RED};
  bar.thresholds[1] = {60.0f,  YELLOW};
  bar.thresholds[2] = {100.0f, GREEN};
  bar.Draw(font);

  // ── HorizontalBarGraphWidget (throttle) ───────────────────────────────────
  HorizontalBarGraphWidget hbar;
  hbar.gx = 4; hbar.gy = 0; hbar.wTiles = 6; hbar.hTiles = 2;
  hbar.value = 73.0f; hbar.minValue = 0.0f; hbar.maxValue = 100.0f;
  hbar.units = "%"; hbar.decimals = 0; hbar.tickCount = 6;
  hbar.thresholdCount = 3;
  hbar.thresholds[0] = {40.0f,  GREEN};
  hbar.thresholds[1] = {75.0f,  YELLOW};
  hbar.thresholds[2] = {100.0f, RED};
  hbar.Draw(font);

  // ── GraphWidget (speed history) ───────────────────────────────────────────
  GraphWidget graph;
  graph.gx = 4; graph.gy = 2; graph.wTiles = 6; graph.hTiles = 4;
  graph.xMin = 0.0f; graph.xMax = 10.0f;
  graph.yMin = 0.0f; graph.yMax = 100.0f;
  graph.xUnits = "s"; graph.yUnits = "mph";
  graph.xTickCount = 6; graph.yTickCount = 6;

  GraphSeries speed;
  speed.color = SKYBLUE; speed.thickness = 2.0f;
  for (int i = 0; i <= 40; i++) {
    float t = i * 0.25f;
    float v = 50.0f + 40.0f * sinf(t * 0.8f) * expf(-t * 0.05f);
    speed.points.push_back({t, v});
  }

  GraphSeries accel;
  accel.color = GREEN; accel.thickness = 2.0f;
  for (int i = 0; i <= 40; i++) {
    float t = i * 0.25f;
    float v = 30.0f + 25.0f * cosf(t * 1.1f + 0.5f);
    accel.points.push_back({t, v});
  }

  graph.series.push_back(speed);
  graph.series.push_back(accel);
  graph.Draw(font);
}
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  bool demo_mode = (argc > 1 && std::string(argv[1]) == "--demo");
  const char *config_path = (argc > 1 && !demo_mode) ? argv[1] : "data.json";

  struct sigaction sa{};
  sa.sa_handler = sighup_handler;
  sigaction(SIGHUP, &sa, nullptr);

  DisplayConfig display_cfg;
  std::vector<LiveScreen> screens;
  std::size_t active_screen = 0;
  if (!demo_mode) {
    display_cfg = load_display_config(config_path);
    screens = build_screens(display_cfg);
  }

  const int W = 800, H = 480;
  InitWindow(W, H, demo_mode ? "TRACK Display — Widget Demo" : "TRACK Display");
  SetTargetFPS(60);

  std::string fontPath = std::string(GetApplicationDirectory()) + "assets/fonts/InterVariable.ttf";
  Font uiFont = LoadFontEx(fontPath.c_str(), 512, 0, 0);
  SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

  TelemetryQueue *queue = nullptr;
  std::size_t consumer_pos = 0;
  int queue_retry = 0;
  if (!demo_mode) {
    queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
    consumer_pos = queue ? queue->current_pos() : 0;
  }

  while (!WindowShouldClose()) {
    if (!demo_mode) {
      if (reload_flag) {
        reload_flag = 0;
        display_cfg = load_display_config(config_path);
        screens = build_screens(display_cfg);

        if (screens.empty()) {
          active_screen = 0;
        } else if (active_screen >= screens.size()) {
          active_screen = screens.size() - 1;
        }

        printf("reloaded config from %s\n", config_path);
      }

      handle_screen_navigation(screens, active_screen);

      if (!queue && ++queue_retry >= 60) {
        queue_retry = 0;
        queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
        if (queue)
          consumer_pos = queue->current_pos();
      }

      if (queue) {
        queue->consume(consumer_pos, [&screens](const TelemetryMessage &msg) {
          update_all_screens(screens, msg);
        });
      }
    }

    BeginDrawing();
    ClearBackground(BLACK);

    if (demo_mode) {
      run_demo(uiFont);
    } else if (!screens.empty()) {
      for (const auto &lw : screens[active_screen].widgets) {
        lw.draw(uiFont);
      }
    } else {
      const char* msg = "No screens configured";
      Vector2 size = MeasureTextEx(uiFont, msg, 28.0f, 1.0f);
      DrawTextEx(uiFont, msg, Vector2{(W - size.x) * 0.5f, (H - size.y) * 0.5f},
                 28.0f, 1.0f, RAYWHITE);
    }

    EndDrawing();
  }

  if (queue)
    close_shared_queue(queue, TELEMETRY_SHM, false);

  UnloadFont(uiFont);
  CloseWindow();
  return 0;
}