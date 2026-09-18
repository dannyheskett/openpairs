// Unit tests for the board layout — no raylib, no window. layout.c is included
// directly, with a stub safe area, so every device shape can be asserted
// without a device.
//
// The invariants that matter here are the ones a rotating, phone-to-iPad game
// gets wrong: cards must never fall below the readable floor, the level cap
// must be honest, chrome must not resize when the device has not, and the grid
// must follow the shape of the window.
//
// Built and run by `make test`. A non-zero exit means a failure.
#include "../src/layout.c"
#include "../src/game.c"

// No device here, so no insets: the stub stands in for safe_area.c, which on
// Android is fed by the Activity and everywhere else returns zeros anyway.
SafeArea safe_area_get(void) { SafeArea s = {0}; return s; }

#include <stdio.h>

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            failures++;                                                      \
        }                                                                    \
    } while (0)

// Device shapes, in the device pixels the game is handed.
typedef struct { const char* name; int w, h; } Shape;
static const Shape SHAPES[] = {
    { "iPhone SE",        750, 1334 },
    { "iPhone 15",       1179, 2556 },
    { "iPhone 15 Pro Max",1290, 2796 },
    { "Pixel 7",         1080, 2400 },
    { "iPad 10.9",       1640, 2360 },
    { "iPad Pro 13",     2064, 2752 },
    { "desktop 900x700",  900,  700 },
    { "desktop min",      640,  480 },
};
static const int SHAPE_COUNT = (int)(sizeof SHAPES / sizeof SHAPES[0]);

// --- Every level fits, at or above the floor, in both orientations ----------
static void test_levels_fit_everywhere(void) {
    for (int s = 0; s < SHAPE_COUNT; s++) {
        for (int rot = 0; rot < 2; rot++) {
            int w = rot ? SHAPES[s].h : SHAPES[s].w;
            int h = rot ? SHAPES[s].w : SHAPES[s].h;
            int fits = layout_pairs_that_fit(w, h);
            CHECK(fits >= 1);
            CHECK(fits <= MAX_PAIRS);

            for (int lv = 0; lv < LEVEL_COUNT; lv++) {
                int want = LEVEL_PAIRS[lv];
                int pairs = (want < fits) ? want : fits;
                Layout l = layout_for(w, h, pairs * 2);
                int floor_px = layout_min_card(w, h);
                if (l.card < floor_px) {
                    printf("  FAIL %s %s %s: card %d < %d\n", SHAPES[s].name,
                           rot ? "landscape" : "portrait", LEVEL_NAME[lv], l.card, floor_px);
                    failures++;
                }
                CHECK(l.rows * l.cols >= pairs * 2);
                // The board stays inside the window.
                CHECK(l.board_x >= 0 && l.board_y >= 0);
                CHECK(l.board_x + l.board_w <= w);
                CHECK(l.board_y + l.board_h <= h);
            }
        }
    }
}

// --- The cap is a real ceiling ---------------------------------------------
static void test_cap_is_the_largest_that_fits(void) {
    for (int s = 0; s < SHAPE_COUNT; s++) {
        int w = SHAPES[s].w, h = SHAPES[s].h;
        int fits = layout_pairs_that_fit(w, h);
        int floor_px = layout_min_card(w, h);
        CHECK(layout_for(w, h, fits * 2).card >= floor_px);
        CHECK(layout_for(h, w, fits * 2).card >= floor_px);   // and rotated
        if (fits < MAX_PAIRS) {
            // One more pair breaks the floor in one orientation or the other;
            // that is why it is the cap.
            CHECK(layout_for(w, h, (fits + 1) * 2).card < floor_px ||
                  layout_for(h, w, (fits + 1) * 2).card < floor_px);
        }
    }
}

// A phone must not promise the whole Extra Hard board, and a big iPad must.
static void test_phone_caps_and_tablet_does_not(void) {
    CHECK(layout_pairs_that_fit(750, 1334) < LEVEL_PAIRS[LEVEL_EXTRA_HARD]);
    CHECK(layout_pairs_that_fit(2064, 2752) == LEVEL_PAIRS[LEVEL_EXTRA_HARD]);
}

// --- Rotation ---------------------------------------------------------------
// Chrome is sized from the long edge, so turning the device must not resize the
// wordmark or the status text — the screen has not changed, only its shape.
static void test_chrome_is_stable_across_rotation(void) {
    for (int s = 0; s < SHAPE_COUNT; s++) {
        Layout up   = layout_for(SHAPES[s].w, SHAPES[s].h, 20);
        Layout side = layout_for(SHAPES[s].h, SHAPES[s].w, 20);
        CHECK(up.title_fs == side.title_fs);
        CHECK(up.status_fs == side.status_fs);
        CHECK(up.titlebar_h == side.titlebar_h);
    }
}

// The same board rotated keeps the same number of cards and stays readable.
static void test_rotation_refits_the_same_board(void) {
    for (int s = 0; s < SHAPE_COUNT; s++) {
        int pairs = layout_pairs_that_fit(SHAPES[s].w, SHAPES[s].h);
        Layout side = layout_for(SHAPES[s].h, SHAPES[s].w, pairs * 2);
        CHECK(side.rows * side.cols >= pairs * 2);
        CHECK(side.card >= layout_min_card(SHAPES[s].w, SHAPES[s].h));
    }
}

// --- The grid follows the window -------------------------------------------
static void test_grid_follows_the_window(void) {
    // Six cards: taller than wide upright, wider than tall sideways.
    Layout up   = layout_for(1080, 2400, 6);
    Layout side = layout_for(2400, 1080, 6);
    CHECK(up.rows >= up.cols);
    CHECK(side.cols >= side.rows);
    CHECK(up.rows * up.cols == 6);
    CHECK(side.rows * side.cols == 6);
}

// No arrangement may waste a whole column (7 cards is 4x2, never 3x3).
static void test_no_wasted_column(void) {
    for (int cards = 2; cards <= MAX_CARDS; cards += 2) {
        for (int s = 0; s < SHAPE_COUNT; s++) {
            Layout l = layout_for(SHAPES[s].w, SHAPES[s].h, cards);
            CHECK(l.rows * l.cols - cards < l.cols);
        }
    }
}

// A short last row is centred, and its empty slots are still misses.
static void test_short_last_row_is_centred(void) {
    Layout l = layout_for(780, 1040, 28);       // 6 columns, last row holds 4
    int rem = 28 % l.cols;
    if (rem != 0) {
        int first_x, first_y, last_x, last_y;
        layout_card_pos_of(l, 28, 0, &first_x, &first_y);
        layout_card_pos_of(l, 28, 28 - rem, &last_x, &last_y);
        CHECK(last_x > first_x);                 // indented, not flush left
        int row_w = rem * l.card + (rem - 1) * l.gap;
        CHECK(last_x - l.board_x == (l.board_w - row_w) / 2);   // and centred
        CHECK(layout_card_at(l, 28, first_x + l.card / 2, last_y + l.card / 2) == -1);
    }
}

// --- Hit testing ------------------------------------------------------------
static void test_hit_test_round_trips(void) {
    Layout l = layout_for(1179, 2556, 20);
    for (int i = 0; i < 20; i++) {
        int x, y;
        layout_card_pos_of(l, 20, i, &x, &y);
        CHECK(layout_card_at(l, 20, x + l.card / 2, y + l.card / 2) == i);
        CHECK(layout_card_at(l, 20, x + 1, y + 1) == i);
    }
    // Gaps, the area outside the board, and the empty slots of a short last row
    // are all misses.
    CHECK(layout_card_at(l, 20, l.board_x - 5, l.board_y + 5) == -1);
    CHECK(layout_card_at(l, 20, l.board_x + l.card + l.gap / 2, l.board_y + 5) == -1);
    Layout odd = layout_for(1179, 2556, 6);
    int x, y;
    layout_card_pos_of(odd, 6, 6, &x, &y);   // one past the last dealt card
    CHECK(layout_card_at(odd, 6, x + odd.card / 2, y + odd.card / 2) == -1);
}

int main(void) {
    printf("test_layout: level caps, rotation, grid shape, hit testing\n");
    test_levels_fit_everywhere();
    test_cap_is_the_largest_that_fits();
    test_phone_caps_and_tablet_does_not();
    test_chrome_is_stable_across_rotation();
    test_rotation_refits_the_same_board();
    test_grid_follows_the_window();
    test_no_wasted_column();
    test_short_last_row_is_centred();
    test_hit_test_round_trips();
    if (failures == 0) { printf("OK: all checks passed\n"); return 0; }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
