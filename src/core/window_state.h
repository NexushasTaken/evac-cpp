#ifndef EVAC_WINDOW_STATE_H
#define EVAC_WINDOW_STATE_H

#include "core/settings.h"

// What we remember about the main window between runs. Only the size and
// whether it is maximized: on Wayland, programs can't place their own windows.
struct WindowState {
  bool valid = false;  // False if nothing usable was saved.
  int width = 0;
  int height = 0;
  bool maximized = false;
};

// Reads the "window_state" setting (the same one the original launcher
// wrote, with size_x and size_y). Nonsensical sizes count as not saved.
WindowState load_window_state(Settings& settings);

// Stores the state. Other keys in "window_state" (like the old position) are
// kept.
void store_window_state(Settings& settings, const WindowState& state);

#endif
