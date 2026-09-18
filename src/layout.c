#include "layout.h"
#include "game.h"

#include <stdbool.h>
#include "safe_area.h"

static int imax(int a, int b) { return (a > b) ? a : b; }
static int imin(int a, int b) { return (a < b) ? a : b; }

// Chrome is sized from the device's LONG edge, never from the live height.
// This game rotates, and height-based divisors halve every number the moment
// the phone turns sideways -- the wordmark and status text would resize on a
// screen that has not changed. Same rule as openklondike's chrome_ref().
static int chrome_ref(int view_w, int view_h) { return imax(view_w, view_h); }

static int title_fs_of(int ref)  { int fs = ref / 45; return (fs < 10) ? 10 : fs; }
static int title_bar_of(int ref) { int fs = title_fs_of(ref); return fs + fs / 2; }
static int status_fs_of(int ref) { int fs = ref / 38; return (fs < 9) ? 9 : fs; }

// The wordmark bar, grown to clear a display cutout when the surface draws
// under one. iOS hands the game a viewport that already excludes the notch, so
// this only fires on Android.
static int top_bar_of(int ref) {
    int bar = title_bar_of(ref);
    SafeArea s = safe_area_get();
    return (s.top > bar) ? s.top : bar;
}

int layout_min_card(int view_w, int view_h) {
    int shortd = imin(view_w, view_h);
    int m = shortd / 6;
    if (m < MIN_CARD_FLOOR)   m = MIN_CARD_FLOOR;
    if (m > MIN_CARD_CEILING) m = MIN_CARD_CEILING;
    return m;
}

Layout layout_for(int view_w, int view_h, int cards) {
    Layout l = (Layout){0};
    l.view_w = view_w;
    l.view_h = view_h;
    if (cards < 1) cards = 1;

    int shortd = imin(view_w, view_h);
    int ref    = chrome_ref(view_w, view_h);

    l.margin     = imax(shortd / 28, 6);
    l.titlebar_h = top_bar_of(ref);
    l.title_fs   = title_fs_of(ref);
    l.status_fs  = status_fs_of(ref);
    l.status_h   = l.status_fs * 3 / 2;
    l.status_y   = l.titlebar_h + l.margin / 2;
    l.gap        = imax(shortd / 90, 4);

    // Space left for the grid, below the status band and inside the margins.
    SafeArea sa = safe_area_get();
    int avail_x = l.margin + sa.left;
    int avail_y = l.status_y + l.status_h + l.margin;
    int avail_w = view_w - avail_x - l.margin - sa.right;
    int avail_h = view_h - avail_y - l.margin - sa.bottom;
    if (avail_w < 1) avail_w = 1;
    if (avail_h < 1) avail_h = 1;

    // Try every column count and keep the arrangement with the largest card.
    // Ties go to the grid whose shape is closest to the space it sits in, which
    // is what makes the same deal read as 3x2 upright and 2x3 sideways.
    int best_card = 0, best_rows = 1, best_cols = cards, best_holes = 0;
    long best_skew = 0;
    for (int cols = 1; cols <= cards; cols++) {
        int rows = (cards + cols - 1) / cols;
        // Reject arrangements that waste a whole column: 7 cards in 4x2 is
        // fine (one hole), 3x3 is not.
        if (rows * cols - cards >= cols) continue;

        int cw = (avail_w - (cols - 1) * l.gap) / cols;
        int ch = (avail_h - (rows - 1) * l.gap) / rows;
        int card = imin(cw, ch);
        if (card < 1) continue;

        // How far this grid's aspect is from the available area's, compared
        // without division so the tie-break is exact in integers.
        long skew = (long)cols * avail_h - (long)rows * avail_w;
        if (skew < 0) skew = -skew;
        int holes = rows * cols - cards;

        // Biggest card wins. On a tie prefer the grid with no empty slots --
        // 6 cards as 3x2, not 4x2 with two gaps at the end -- and only then the
        // one shaped most like the space it sits in.
        bool better = card > best_card
                   || (card == best_card && holes < best_holes)
                   || (card == best_card && holes == best_holes && skew < best_skew);
        if (better) {
            best_card = card;
            best_rows = rows;
            best_cols = cols;
            best_holes = holes;
            best_skew = skew;
        }
    }

    l.rows = best_rows;
    l.cols = best_cols;
    l.card = imax(best_card, 1);
    l.board_w = l.cols * l.card + (l.cols - 1) * l.gap;
    l.board_h = l.rows * l.card + (l.rows - 1) * l.gap;
    l.board_x = avail_x + (avail_w - l.board_w) / 2;
    l.board_y = avail_y + (avail_h - l.board_h) / 2;
    return l;
}

int layout_pairs_that_fit(int view_w, int view_h) {
    // Both orientations, because the device can be turned mid-game and the deal
    // is fixed once it starts: a board that only fits upright would have to
    // shrink below the floor the moment the player rotated.
    int floor_px = layout_min_card(view_w, view_h);
    for (int pairs = MAX_PAIRS; pairs > 1; pairs--) {
        if (layout_for(view_w, view_h, pairs * 2).card >= floor_px &&
            layout_for(view_h, view_w, pairs * 2).card >= floor_px) return pairs;
    }
    return 1;
}

// How far the last row is indented when it holds fewer cards than the others.
// A short row left flush against the left edge reads as a mistake; centring it
// under the block looks deliberate, which matters on a board a child is asked
// to scan.
static int last_row_offset(Layout l, int cards) {
    int rem = (l.cols > 0) ? cards % l.cols : 0;
    if (rem == 0) return 0;
    return ((l.cols - rem) * (l.card + l.gap)) / 2;
}

void layout_card_pos_of(Layout l, int cards, int index, int* x, int* y) {
    int r = (l.cols > 0) ? index / l.cols : 0;
    int c = (l.cols > 0) ? index % l.cols : 0;
    int off = (r == (cards - 1) / (l.cols > 0 ? l.cols : 1)) ? last_row_offset(l, cards) : 0;
    *x = l.board_x + off + c * (l.card + l.gap);
    *y = l.board_y + r * (l.card + l.gap);
}

int layout_card_at(Layout l, int cards, int x, int y) {
    if (l.card <= 0 || l.cols <= 0) return -1;
    int step = l.card + l.gap;
    int dy = y - l.board_y;
    if (dy < 0) return -1;
    int r = dy / step;
    if (r >= l.rows) return -1;
    if (dy - r * step >= l.card) return -1;   // in the gap below a row

    int off = (r == (cards - 1) / l.cols) ? last_row_offset(l, cards) : 0;
    int dx = x - l.board_x - off;
    if (dx < 0) return -1;
    int c = dx / step;
    if (c >= l.cols) return -1;
    if (dx - c * step >= l.card) return -1;   // in the gap after a card

    int index = r * l.cols + c;
    return (index < cards) ? index : -1;
}
