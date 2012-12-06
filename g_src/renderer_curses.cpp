#include <ncursesw/curses.h>
WINDOW **stdscr_p;

class renderer_curses : public renderer {
  std::map<std::pair<int,int>,int> color_pairs;

  // Map from DF color to ncurses color
  static int ncurses_map_color(int color) {
    if (color < 0) abort();
    switch (color) {
    case 0: return 0;
    case 1: return 4;
    case 2: return 2;
    case 3: return 6;
    case 4: return 1;
    case 5: return 5;
    case 6: return 3;
    case 7: return 7;
    default: return ncurses_map_color(color - 7);
    }
  }

  // Look up, or create, a curses color pair
  int lookup_pair(pair<int,int> color) {
    map<pair<int,int>,int>::iterator it = color_pairs.find(color);
    if (it != color_pairs.end()) return it->second;
    // We don't already have it. Make sure it's in range.
    if (color.first < 0 || color.first > 7 || color.second < 0 || color.second > 7) return 0;
    // We don't already have it. Generate a new pair if possible.
    if (color_pairs.size() < COLOR_PAIRS - 1) {
      const short pair = color_pairs.size() + 1;
      init_pair(pair, ncurses_map_color(color.first), ncurses_map_color(color.second));
      color_pairs[color] = pair;
      return pair;
    }
    // We don't have it, and there's no space for more. Find the closest equivalent.
    int score = 999, pair = 0;
    int rfg = color.first % 16, rbg = color.second % 16;
    for (auto it = color_pairs.cbegin(); it != color_pairs.cend(); ++it) {
      int fg = it->first.first;
      int bg = it->first.second;
      int candidate = it->second;
      int candidate_score = 0;  // Lower is better.
      if (rbg != bg) {
        if (rbg == 0 || rbg == 15)
          candidate_score += 3;  // We would like to keep the background black/white.
        if ((rbg == 7 || rbg == 8)) {
          if (bg == 7 || bg == 8)
            candidate_score += 1; // Well, it's still grey.
          else
            candidate_score += 2;
        }
      }
      if (rfg != fg) {
        if (rfg == 0 || rfg == 15)
          candidate_score += 5; // Keep the foreground black/white if at all possible.
        if (rfg == 7 || rfg == 8) {
          if (fg == 7 || fg == 8)
            candidate_score += 1; // Still grey. Meh.
          else
            candidate_score += 3;
        }
      }
      if (candidate_score < score) {
        score = candidate_score;
        pair = candidate;
      }
    }
    color_pairs[color] = pair;
    return pair;
  }

public:

  void update_tile(int x, int y) {
    const int ch   = gps.screen[x*gps.dimy*4 + y*4 + 0];
    const int fg   = gps.screen[x*gps.dimy*4 + y*4 + 1];
    const int bg   = gps.screen[x*gps.dimy*4 + y*4 + 2];
    const int bold = gps.screen[x*gps.dimy*4 + y*4 + 3];

    const int pair = lookup_pair(make_pair(fg,bg));

    if (ch == 219 && !bold) {
      // It's █, which is used for borders and digging designations.
      // A_REVERSE space looks better if it isn't completely tall.
      // Which is most of the time, for me at least.
      // █ <-- Do you see gaps?
      // █
      // The color can't be bold.
      wattrset(*stdscr_p, COLOR_PAIR(pair) | A_REVERSE);
      mvwaddstr(*stdscr_p, y, x, " ");
    } else {
      wattrset(*stdscr_p, COLOR_PAIR(pair) | (bold ? A_BOLD : 0));
      wchar_t chs[2] = {charmap[ch],0};
      mvwaddwstr(*stdscr_p, y, x, chs);
    }
  }

  void update_all() {
    for (int x = 0; x < init.display.grid_x; x++)
      for (int y = 0; y < init.display.grid_y; y++)
        update_tile(x, y);
  }

  void render() {
    refresh();
  }

  void resize(int w, int h) {
    if (enabler.overridden_grid_sizes.size() == 0)
      gps_allocate(w, h);
    erase();
    // Force a full display cycle
    gps.force_full_display_count = 1;
    enabler.flag |= ENABLERFLAG_RENDER;
  }

  void grid_resize(int w, int h) {
    gps_allocate(w, h);
  }

  renderer_curses() { stdscr_p = &stdscr; }
  bool get_mouse_coords(int &x, int &y) {
    return false;
  }
};

// Reads from getch, collapsing utf-8 encoding to the actual unicode
// character.  Ncurses symbols (left arrow, etc.) are returned as
// positive values, unicode as negative. Error returns 0.
static int getch_utf8() {
  int byte = wgetch(*stdscr_p);
  if (byte == ERR) return 0;
  if (byte > 0xff) return byte;
  int len = decode_utf8_predict_length(byte);
  if (!len) return 0;
  string input(len,0); input[0] = byte;
  for (int i = 1; i < len; i++) input[i] = wgetch(*stdscr_p);
  return -decode_utf8(input);
}

void enablerst::eventLoop_ncurses() {
  int x, y, oldx = 0, oldy = 0;
  renderer_curses *renderer = static_cast<renderer_curses*>(this->renderer);
  
  while (loopvar) {
    // Check for terminal resize
    getmaxyx(*stdscr_p, y, x);
    if (y != oldy || x != oldx) {
      pause_async_loop();
      renderer->resize(x, y);
      unpause_async_loop();
      oldx = x; oldy = y;
    }
    
    // Deal with input
    Uint32 now = SDL_GetTicks();
    // Read keyboard input, if any, and transform to artificial SDL
    // events for enabler_input.
    int key;
    bool paused_loop = false;
    while ((key = getch_utf8())) {
      if (!paused_loop) {
        pause_async_loop();
        paused_loop = true;
      }
      bool esc = false;
      if (key == KEY_MOUSE) {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
          // TODO: Deal with curses mouse input. And turn it on above.
        }
      } else if (key == -27) { // esc
        int second = getch_utf8();
        if (second) { // That was an escape sequence
          esc = true;
          key = second;
        }
      }
      add_input_ncurses(key, now, esc);
    }

    if (paused_loop)
      unpause_async_loop();

    // Run the common logic
    do_frame();
  }
}
