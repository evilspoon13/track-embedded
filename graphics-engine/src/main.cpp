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

#include <cmath>
#include <csignal>
#include <cstdio>
#include <string>
#include <unistd.h>

static constexpr const char* PIDFILE = "/run/track/graphics.pid";

static volatile sig_atomic_t reload_flag = 0;
static volatile sig_atomic_t screen_move_flag = 0;

static void sighup_handler(int) { reload_flag = 1; }
static void sigusr1_handler(int) { screen_move_flag = 1; }

static void write_pidfile() {
    FILE* f = fopen(PIDFILE, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

static void remove_pidfile() {
    unlink(PIDFILE);
}

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

  bool move = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT);

  if (screen_move_flag) {
    screen_move_flag = 0;
    move = true;
  }

  if (move) {
    active_screen = (active_screen + 1) % screens.size();
  }
}


int main(int argc, char *argv[]) {
  const char *config_path = (argc > 1) ? argv[1] : "data.json";

  struct sigaction sa{};
  sa.sa_handler = sighup_handler;
  sigaction(SIGHUP, &sa, nullptr);

  struct sigaction sa_usr{};
  sa_usr.sa_handler = sigusr1_handler;
  sigaction(SIGUSR1, &sa_usr, nullptr);

  write_pidfile();

  DisplayConfig display_cfg;
  std::vector<LiveScreen> screens;
  std::size_t active_screen = 0;
  display_cfg = load_display_config(config_path);
  screens = build_screens(display_cfg);

  const int W = 800, H = 480;
  InitWindow(W, H, "TRACK Display");
  SetTargetFPS(60);

  std::string fontPath = std::string(GetApplicationDirectory()) + "assets/fonts/InterVariable.ttf";
  Font uiFont = LoadFontEx(fontPath.c_str(), 512, 0, 0);
  SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);

  TelemetryQueue *queue = nullptr;
  std::size_t consumer_pos = 0;
  int queue_retry = 0;
  queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
  consumer_pos = queue ? queue->current_pos() : 0;


  while (!WindowShouldClose()) {
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

    BeginDrawing();
    ClearBackground(BLACK);

    if (!screens.empty()) {
      for (const auto &lw : screens[active_screen].widgets) {
        lw.draw(uiFont);
      }
    }
    else {
      const char* msg = "No screens configured";
      Vector2 size = MeasureTextEx(uiFont, msg, 28.0f, 1.0f);
      DrawTextEx(uiFont, msg, Vector2{(W - size.x) * 0.5f, (H - size.y) * 0.5f}, 28.0f, 1.0f, RAYWHITE);
    }

    EndDrawing();
  }

  if (queue)
    close_shared_queue(queue, TELEMETRY_SHM, false);

  UnloadFont(uiFont);
  CloseWindow();
  remove_pidfile();
  return 0;
}