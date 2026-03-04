#include "WidgetFactory.h"
#include "Widgets.h"
#include "config_parser.hpp"
#include "raylib.h"
#include "shared_memory.hpp"

#include <csignal>
#include <cstdio>
#include <string>

static volatile sig_atomic_t reload_flag = 0;

static void sighup_handler(int) { reload_flag = 1; }

int main(int argc, char *argv[]) {
  const char *config_path = (argc > 1) ? argv[1] : "data.json";

  struct sigaction sa{};
  sa.sa_handler = sighup_handler;
  sigaction(SIGHUP, &sa, nullptr);

  DisplayConfig display_cfg = load_display_config(config_path);
  auto widgets = build_widgets(display_cfg);

  const int W = 800, H = 480;
  InitWindow(W, H, "FSAE Display");
  SetTargetFPS(60);

  std::string fontPath = std::string(GetApplicationDirectory()) + "assets/fonts/InterVariable.ttf";
  Font uiFont = LoadFontEx(fontPath.c_str(), 256, 0, 0);
  SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

  TelemetryQueue *queue = open_shared_queue(false);
  std::size_t consumer_pos = queue ? queue->current_pos() : 0;
  int queue_retry = 0;

  while (!WindowShouldClose()) {
    if (reload_flag) {
      reload_flag = 0;
      display_cfg = load_display_config(config_path);
      widgets = build_widgets(display_cfg);
      printf(" reloaded config from %s\n", config_path);
    }

    if (!queue && ++queue_retry >= 60) {
      queue_retry = 0;
      queue = open_shared_queue(false);
      if (queue)
        consumer_pos = queue->current_pos();
    }

    if (queue) {
      queue->consume(consumer_pos, [&widgets](const TelemetryMessage &msg) {
        for (auto &lw : widgets) {
          if (lw.can_id == msg.can_id && lw.signal == msg.signal_name)
            lw.set_value(msg.value);
        }
      });
    }

    BeginDrawing();
    ClearBackground(BLACK);

    for (const auto &lw : widgets)
      lw.draw(uiFont);

    EndDrawing();
  }

  if (queue)
    close_shared_queue(queue, false);

  UnloadFont(uiFont);
  CloseWindow();
  return 0;
}
