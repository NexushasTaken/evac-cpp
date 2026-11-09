#include "core/window_state.h"

#include <nlohmann/json_fwd.hpp>

#include "core/settings.h"

static const int MIN_SIZE = 300;
static const int MAX_SIZE = 20000;

static bool sane(int size) { return size >= MIN_SIZE && size <= MAX_SIZE; }

WindowState load_window_state(Settings& settings) {
  WindowState state;
  nlohmann::json json = settings.get_json("window_state");
  if (!json.is_object() || !json.contains("size_x") ||
      !json.contains("size_y") || !json["size_x"].is_number() ||
      !json["size_y"].is_number()) {
    return state;
  }

  const int width = static_cast<int>(json["size_x"].get<double>());
  const int height = static_cast<int>(json["size_y"].get<double>());
  if (!sane(width) || !sane(height)) {
    return state;
  }

  state.valid = true;
  state.width = width;
  state.height = height;
  state.maximized = json.value("maximized", false);
  return state;
}

void store_window_state(Settings& settings, const WindowState& state) {
  nlohmann::json json = settings.get_json("window_state");
  if (!json.is_object()) {
    json = nlohmann::json::object();
  }

  json["size_x"] = state.width;
  json["size_y"] = state.height;
  json["maximized"] = state.maximized;
  settings.set_json("window_state", json);
}
