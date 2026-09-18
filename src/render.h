#ifndef OPENPAIRS_RENDER_H
#define OPENPAIRS_RENDER_H

#include "game.h"
#include "layout.h"
#include "platform.h"
#include "op_types.h"
#include <stdbool.h>

// Window setup and teardown (window.c) plus the recorder's capture canvas. The
// board fits itself to whatever the window is; a small window simply caps the
// level to fewer pairs, exactly as a small phone does.
void render_init(void);
void render_cleanup(void);

// Scenes -------------------------------------------------------------------
// The board. `cursor` is the keyboard highlight index, or -1 when the player is
// using a pointer.
void render_frame(const Game* g, int cursor);
// The board with the "all found" panel over it.
void render_win(const Game* g, int cursor);
// The family menu (menu.c) on the felt. gap_before, if >= 0, inserts a blank
// line before that item index. Hit-test its rows with menu_hit_test().
void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before);

// Hit tests ----------------------------------------------------------------
// Card index at a screen point for the board `g` is laid out with, or -1.
int render_card_at(const Game* g, int x, int y);
// Top edge of the card grid for `g` in the live window. Everything above it --
// the title bar and the status line -- is the tap-for-menu area.
int render_board_top(const Game* g);

// Queries ------------------------------------------------------------------
// Card side in pixels for the current window and board. The touch layer scales
// its tap-movement tolerance from it.
int render_card_size(void);
// The most pairs that fit the live window at a readable card size. main.c caps
// the level with this when dealing, and the Options screen shows it.
int render_pairs_that_fit(void);

#endif
