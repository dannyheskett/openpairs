#ifndef OPENPAIRS_RENDER_H
#define OPENPAIRS_RENDER_H

#include "game.h"
#include "layout.h"
#include "platform.h"
#include "op_types.h"
#include <stdbool.h>

// Minimum desktop window. Both are multiples of 16 so the recorder can capture
// them. The board fits itself to whatever the window is, so this is only a
// floor for the chrome and a sensible starting size -- a small window simply
// caps the level to fewer pairs, exactly as a small phone does.
#define MIN_W 640
#define MIN_H 480

void render_init(void);
void render_cleanup(void);
bool render_window_should_close(void);
void render_toggle_fullscreen(void);
// True while the app window holds input focus. Used to fall back to the menu
// when the app is backgrounded on a touch platform.
bool render_window_focused(void);

// Scenes -------------------------------------------------------------------
// The board. `cursor` is the keyboard highlight index, or -1 when the player is
// using a pointer.
void render_frame(const Game* g, int cursor);
// The board with the "all found" panel over it.
void render_win(const Game* g, int cursor);
// Floating menu: title plus a list of items, one highlighted. gap_before, if
// >= 0, inserts a blank line before that item index.
void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before);

// Hit tests ----------------------------------------------------------------
// Card index at a screen point for the board `g` is laid out with, or -1.
int render_card_at(const Game* g, int x, int y);
// Top edge of the card grid for `g` in the live window. Everything above it --
// the title bar and the status line -- is the tap-for-menu area.
int render_board_top(const Game* g);
// Menu item index at a screen point, or -1. Uses the rectangles captured by the
// last render_menu().
int render_menu_hit_test(Vector2 p);

// Queries ------------------------------------------------------------------
// Card side in pixels for the current window and board. The touch layer scales
// its tap-movement tolerance from it.
int render_card_size(void);
// The most pairs that fit the live window at a readable card size. main.c caps
// the level with this when dealing, and the Options screen shows it.
int render_pairs_that_fit(void);

#endif
