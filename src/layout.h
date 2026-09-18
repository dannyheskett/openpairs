#ifndef OPENPAIRS_LAYOUT_H
#define OPENPAIRS_LAYOUT_H

// Board geometry, derived from the live view size every frame. Pure: no raylib,
// no globals, no caching -- which is what makes rotation free (the next frame
// simply fits the same board to the new shape) and makes the whole thing
// unit-testable without a window (tests/test_layout.c).

// The readable floor, in device pixels: no board is dealt with cards smaller
// than this, which is what caps the harder levels on a small screen instead of
// letting the cards shrink away.
//
// It scales with the screen's short edge rather than being a flat pixel count,
// because a pixel is not a fixed size: 108px is a comfortable 54pt target on a
// 2x phone and a fiddly 36pt one on a 3x phone. A sixth of the short edge
// tracks density closely enough (phones are ~2-3x, tablets ~2x at far more
// pixels), and the ceiling stops a large tablet from demanding huge cards and
// capping itself below a phone.
#define MIN_CARD_FLOOR   96
#define MIN_CARD_CEILING 200
int layout_min_card(int view_w, int view_h);

// Cards are square: a square packs equally well in both orientations, and the
// faces are drawn shapes that want a square box.
typedef struct {
    int view_w, view_h;

    int margin;        // breathing room on every edge
    int titlebar_h;    // wordmark bar, grown to clear a display cutout
    int title_fs;
    int status_h, status_y, status_fs;

    int rows, cols;
    int card, gap;     // card side and the gap between cards, both in px
    int board_x, board_y, board_w, board_h;
} Layout;

// Fit `cards` cards into the view. Never returns a card smaller than 1px, but
// may return one smaller than MIN_CARD if the caller asks for more cards than
// fit -- callers size the deal with layout_pairs_that_fit() first, so that only
// happens in tests.
Layout layout_for(int view_w, int view_h, int cards);

// The most pairs that fit at or above MIN_CARD in this view. At least 1.
int layout_pairs_that_fit(int view_w, int view_h);

// Card index at a point, or -1. `cards` is the number dealt, so the empty slots
// of a partly-filled last row are not hit.
int layout_card_at(Layout l, int cards, int x, int y);

// Top-left corner of a card index. `cards` is the number dealt, so a short last
// row can be centred under the rest of the block.
void layout_card_pos_of(Layout l, int cards, int index, int* x, int* y);

#endif
