// The whole renderer: chrome, the card grid, the 21 drawn faces, the menu and
// the win panel. There is no second layout to dispatch to -- a grid reflows to
// any window, so this file serves desktop, both phone orientations and an iPad,
// re-deriving everything from the live view size every frame (layout.c).
#include "render.h"
#include "gfx.h"
#include "safe_area.h"
#include "menu.h"
#include "present.h"
#include "window.h"


#include <math.h>
#include <stdio.h>

// -std=c99 does not expose M_PI.
#define OP_PI 3.14159265358979323846f


// --------------------------------------------------------------------------
// Palette
// --------------------------------------------------------------------------
// The openklondike set, value for value, so the two card games in the family
// look like they come off the same table.
static const Color FELT       = { 12,  92,  52, 255};  // classic green table
static const Color FELT_DARK  = { 10,  76,  44, 255};  // title bar
static const Color SLOT_LINE  = { 30, 110,  66, 255};  // rule under the title bar
static const Color CARD_FACE  = {248, 248, 242, 255};
static const Color CARD_EDGE  = { 40,  40,  40, 255};
static const Color CARD_BACK  = { 36,  72, 156, 255};
static const Color CARD_BACK2 = { 80, 130, 220, 255};  // the plaid on the back
static const Color HILITE     = {255, 235, 120, 255};  // cursor, selection
static const Color MENU_BG    = { 16,  40,  28, 255};
static const Color TEXT_LIGHT = {235, 235, 225, 255};
static const Color TEXT_DIM   = {170, 190, 175, 255};
// openpairs' one addition: a matched card's face, tinted so a found pair reads
// as finished at a glance while it stays on the board.
static const Color CARD_MATCH = {222, 240, 222, 255};

// Corner radius as a fraction of the card's short side: openklondike's value,
// scale-invariant, so every card size has the same silhouette.
#define CARD_ROUND 0.12f

// One colour per face, tied to the shape so the two never disagree. Matching is
// by SHAPE, so colour is decoration: a colour-blind player loses nothing.
static const Color FACE_COLOR[MAX_FACES] = {
    {214,  64,  64, 255}, {232, 138,  40, 255}, {238, 196,  52, 255},
    { 96, 176,  72, 255}, { 48, 156, 140, 255}, { 62, 128, 214, 255},
    {130,  96, 200, 255}, {206,  86, 166, 255}, {120, 120, 132, 255},
    {186,  96,  56, 255}, { 70, 178, 196, 255}, {150, 178,  60, 255},
    {226, 110, 110, 255}, { 88, 140, 232, 255}, {236, 164,  72, 255},
    { 74, 158, 104, 255}, {178, 118, 212, 255}, {212, 154,  92, 255},
    { 92, 190, 168, 255}, {226, 118,  58, 255}, {124, 152, 220, 255},
};

// --------------------------------------------------------------------------
// Face shapes. Twenty-one distinct silhouettes, drawn from the gfx primitives;
// no asset files anywhere in this family.
// --------------------------------------------------------------------------
static Vector2 pt(float x, float y) { Vector2 v = {x, y}; return v; }

// Filled convex polygon as a fan from its first vertex.
static void poly(const Vector2* p, int n, Color c) {
    for (int i = 1; i + 1 < n; i++) gfx_triangle(p[0], p[i], p[i + 1], c);
}

static void ngon(float cx, float cy, float r, int sides, float rot, Color c) {
    Vector2 p[12];
    if (sides > 12) sides = 12;
    for (int i = 0; i < sides; i++) {
        float a = rot + (float)i * 2.0f * OP_PI / (float)sides;
        p[i] = pt(cx + cosf(a) * r, cy + sinf(a) * r);
    }
    poly(p, sides, c);
}

static void star(float cx, float cy, float r, int points, float inner, Color c) {
    Vector2 p[16];
    int n = points * 2;
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) {
        float a = -OP_PI / 2 + (float)i * OP_PI / points;
        float rr = (i % 2 == 0) ? r : r * inner;
        p[i] = pt(cx + cosf(a) * rr, cy + sinf(a) * rr);
    }
    // Not convex: fan from the centre instead of from a vertex.
    for (int i = 0; i < n; i++) {
        gfx_triangle(pt(cx, cy), p[i], p[(i + 1) % n], c);
    }
}

static void bar(float cx, float cy, float w, float h, Color c) {
    gfx_rect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h, c);
}

// Draw face `face` centred in a box of side `s`.
static void draw_face(int face, float cx, float cy, float s) {
    Color c = FACE_COLOR[face % MAX_FACES];
    float r = s * 0.34f;          // nominal shape radius
    switch (face % MAX_FACES) {
    case 0:  gfx_circle(cx, cy, r, c); break;                            // circle
    case 1:  bar(cx, cy, r * 1.8f, r * 1.8f, c); break;                  // square
    case 2:  ngon(cx, cy, r, 3, -OP_PI / 2, c); break;             // triangle
    case 3:  ngon(cx, cy, r, 4, 0, c); break;                            // diamond
    case 4:  star(cx, cy, r, 5, 0.45f, c); break;                        // star
    case 5: {                                                            // heart
        gfx_circle(cx - r * 0.45f, cy - r * 0.25f, r * 0.52f, c);
        gfx_circle(cx + r * 0.45f, cy - r * 0.25f, r * 0.52f, c);
        Vector2 h[3] = { pt(cx - r * 0.95f, cy - r * 0.05f),
                         pt(cx + r * 0.95f, cy - r * 0.05f),
                         pt(cx, cy + r) };
        poly(h, 3, c);
        break;
    }
    case 6: {                                                            // moon
        gfx_circle(cx + r * 0.15f, cy, r, c);
        gfx_circle(cx + r * 0.62f, cy - r * 0.18f, r * 0.86f, CARD_FACE);
        break;
    }
    case 7:                                                              // plus
        bar(cx, cy, r * 1.9f, r * 0.66f, c);
        bar(cx, cy, r * 0.66f, r * 1.9f, c);
        break;
    case 8:                                                              // ring
        gfx_circle(cx, cy, r, c);
        gfx_circle(cx, cy, r * 0.5f, CARD_FACE);
        break;
    case 9:  ngon(cx, cy, r, 6, 0, c); break;                            // hexagon
    case 10: ngon(cx, cy, r, 5, -OP_PI / 2, c); break;             // pentagon
    case 11: {                                                           // arrow up
        Vector2 a[3] = { pt(cx, cy - r), pt(cx - r, cy), pt(cx + r, cy) };
        poly(a, 3, c);
        bar(cx, cy + r * 0.45f, r * 0.62f, r * 0.9f, c);
        break;
    }
    case 12: {                                                           // bowtie
        Vector2 l[3] = { pt(cx, cy), pt(cx - r, cy - r * 0.8f), pt(cx - r, cy + r * 0.8f) };
        Vector2 rr[3] = { pt(cx, cy), pt(cx + r, cy - r * 0.8f), pt(cx + r, cy + r * 0.8f) };
        poly(l, 3, c); poly(rr, 3, c);
        break;
    }
    case 13: {                                                           // teardrop
        gfx_circle(cx, cy + r * 0.25f, r * 0.75f, c);
        Vector2 t[3] = { pt(cx, cy - r), pt(cx - r * 0.7f, cy + r * 0.3f),
                         pt(cx + r * 0.7f, cy + r * 0.3f) };
        poly(t, 3, c);
        break;
    }
    case 14:                                                             // flower
        for (int i = 0; i < 4; i++) {
            float a = (float)i * OP_PI / 2;
            gfx_circle(cx + cosf(a) * r * 0.55f, cy + sinf(a) * r * 0.55f, r * 0.5f, c);
        }
        gfx_circle(cx, cy, r * 0.32f, CARD_FACE);
        break;
    case 15: {                                                           // chevron
        Vector2 up[3] = { pt(cx, cy - r), pt(cx - r, cy), pt(cx + r, cy) };
        Vector2 dn[3] = { pt(cx, cy), pt(cx - r, cy + r * 0.9f), pt(cx + r, cy + r * 0.9f) };
        poly(up, 3, c); poly(dn, 3, c);
        break;
    }
    case 16: bar(cx, cy, r * 2.0f, r * 0.7f, c); break;                  // bar
    case 17:                                                             // cross
        for (int i = -1; i <= 1; i += 2) {
            Vector2 d[4] = { pt(cx - r, cy - r + (i < 0 ? 0 : 2 * r)),
                             pt(cx - r + r * 0.5f, cy - r + (i < 0 ? 0 : 2 * r)),
                             pt(cx + r, cy + r - (i < 0 ? 0 : 2 * r)),
                             pt(cx + r - r * 0.5f, cy + r - (i < 0 ? 0 : 2 * r)) };
            poly(d, 4, c);
        }
        break;
    case 18:                                                             // three dots
        for (int i = -1; i <= 1; i++)
            gfx_circle(cx + (float)i * r * 0.72f, cy, r * 0.32f, c);
        break;
    case 19:                                                             // sun
        gfx_circle(cx, cy, r * 0.55f, c);
        for (int i = 0; i < 8; i++) {
            float a = (float)i * OP_PI / 4;
            gfx_circle(cx + cosf(a) * r * 0.92f, cy + sinf(a) * r * 0.92f, r * 0.16f, c);
        }
        break;
    default:                                                             // six-point star
        star(cx, cy, r, 6, 0.55f, c);
        break;
    }
}

// --------------------------------------------------------------------------
// Card flip animation. The renderer owns it: the simulation has no notion of
// time beyond the mismatch pause, and a card that is up is up. Progress runs
// 0 (back) to 1 (face) and the card is drawn squeezed horizontally through the
// middle of the turn.
// --------------------------------------------------------------------------
#define FLIP_RATE 0.16f   // progress per frame; ~6 frames end to end

static float s_flip[MAX_CARDS];
static int   s_flip_count = 0;

static void flip_advance(const Game* g) {
    if (s_flip_count != g->card_count) {   // new deal: everything face down
        for (int i = 0; i < MAX_CARDS; i++) s_flip[i] = 0.0f;
        s_flip_count = g->card_count;
    }
    for (int i = 0; i < g->card_count; i++) {
        float target = (g->cards[i].state == CARD_DOWN) ? 0.0f : 1.0f;
        if (s_flip[i] < target) s_flip[i] = fminf(target, s_flip[i] + FLIP_RATE);
        else if (s_flip[i] > target) s_flip[i] = fmaxf(target, s_flip[i] - FLIP_RATE);
    }
}

// The card back: openklondike's blue and edge, with a white inset frame and a
// diagonal crosshatch inside it -- the classic playing-card back. The hatch is
// clipped to the frame's rectangle so it never spills onto the border.
static void hatch_line(float x0, float y0, float x1, float y1,
                       float rx0, float ry0, float rx1, float ry1, Color c) {
    // Liang-Barsky clip of the segment to the rectangle.
    float dx = x1 - x0, dy = y1 - y0, t0 = 0.0f, t1 = 1.0f;
    float p[4] = { -dx, dx, -dy, dy };
    float q[4] = { x0 - rx0, rx1 - x0, y0 - ry0, ry1 - y0 };
    for (int i = 0; i < 4; i++) {
        if (p[i] == 0.0f) { if (q[i] < 0.0f) return; continue; }
        float t = q[i] / p[i];
        if (p[i] < 0.0f) { if (t > t1) return; if (t > t0) t0 = t; }
        else             { if (t < t0) return; if (t < t1) t1 = t; }
    }
    gfx_line((int)(x0 + t0 * dx), (int)(y0 + t0 * dy),
             (int)(x0 + t1 * dx), (int)(y0 + t1 * dy), c);
}

static void draw_back_pattern(int x, int y, int w, int h) {
    int m = h * 9 / 100;                    // frame inset
    if (m < 3) m = 3;
    int fw = w - 2 * m, fh = h - 2 * m;
    if (fw <= 4 || fh <= 4) return;

    // Crosshatch, pitch a tenth of the card, inside the frame.
    float rx0 = (float)(x + m + 2), ry0 = (float)(y + m + 2);
    float rx1 = (float)(x + m + fw - 2), ry1 = (float)(y + m + fh - 2);
    float step = h / 10.0f;
    if (step < 4.0f) step = 4.0f;
    // Line weight scales with the card (about 1.3% of its side, as in the
    // design mockup), so the hatch reads the same on a phone and an iPad.
    int thick = (h * 13 + 500) / 1000;
    if (thick < 2) thick = 2;
    float span = (rx1 - rx0) + (ry1 - ry0);
    for (float k = -span; k <= span; k += step) {
        for (int t = 0; t < thick; t++) {
            // "\" lines: x - y = const;  "/" lines: x + y = const.
            hatch_line(rx0 + k + t, ry0, rx0 + k + t + (ry1 - ry0), ry1, rx0, ry0, rx1, ry1, CARD_BACK2);
            hatch_line(rx0 + k + t, ry1, rx0 + k + t + (ry1 - ry0), ry0, rx0, ry0, rx1, ry1, CARD_BACK2);
        }
    }

    // White frame, two strokes so it reads at arm's length.
    float round_inner = CARD_ROUND * 0.6f;
    gfx_rect_rounded_lines(x + m, y + m, fw, fh, round_inner, CARD_FACE);
    gfx_rect_rounded_lines(x + m + 1, y + m + 1, fw - 2, fh - 2, round_inner, CARD_FACE);
}

static void draw_card(const Game* g, Layout l, int index, bool cursor) {
    int x, y;
    layout_card_pos_of(l, g->card_count, index, &x, &y);
    float p = s_flip[index];
    // Squeeze: full width at the ends of the turn, none in the middle.
    float squeeze = fabsf(p * 2.0f - 1.0f);
    int w = (int)(l.card * squeeze);
    if (w < 2) w = 2;
    int cx = x + l.card / 2;
    int draw_x = cx - w / 2;
    bool show_face = (p >= 0.5f);

    Color body = show_face
        ? ((g->cards[index].state == CARD_MATCHED) ? CARD_MATCH : CARD_FACE)
        : CARD_BACK;
    gfx_rect_rounded(draw_x, y, w, l.card, CARD_ROUND, body);
    // openklondike's edge: one dark stroke, doubled in yellow when highlighted.
    Color edge = cursor ? HILITE : CARD_EDGE;
    gfx_rect_rounded_lines(draw_x, y, w, l.card, CARD_ROUND, edge);
    if (cursor)
        gfx_rect_rounded_lines(draw_x + 1, y + 1, w - 2, l.card - 2, CARD_ROUND, edge);

    if (show_face) {
        // Only draw the face once the card is wide enough to hold it, so the
        // shape does not smear during the squeeze.
        if (squeeze > 0.55f) draw_face(g->cards[index].face, (float)cx, (float)(y + l.card / 2),
                                       (float)l.card * squeeze);
    } else if (squeeze > 0.55f) {
        draw_back_pattern(draw_x, y, w, l.card);
    }

}

// --------------------------------------------------------------------------
// Chrome
// --------------------------------------------------------------------------
static void draw_title_bar(Layout l) {
    gfx_rect(0, 0, l.view_w, l.titlebar_h, FELT_DARK);
    gfx_line(0, l.titlebar_h, l.view_w, l.titlebar_h, SLOT_LINE);
    const char* title = "OPENPAIRS";
    int fs = l.title_fs;
    int tw = gfx_measure_text(title, fs);
    int ty = (l.titlebar_h - fs) / 2;

    // Keep the wordmark clear of a camera cutout: if the centre is taken, put
    // it on whichever side has room, and if neither has, leave the bar bare.
    SafeArea sa = safe_area_get();
    int cx = (l.view_w - tw) / 2;
    if (sa.cutout_right > sa.cutout_left) {
        int pad = fs / 2;
        bool clash = !(cx + tw + pad <= sa.cutout_left || cx >= sa.cutout_right + pad);
        if (clash) {
            if (sa.cutout_left >= tw + pad) cx = sa.cutout_left - pad - tw;
            else if (l.view_w - sa.cutout_right >= tw + pad) cx = sa.cutout_right + pad;
            else return;
        }
    }
    gfx_text(title, cx, ty, fs, TEXT_LIGHT);
}

static void draw_status(const Game* g, Layout l) {
    char buf[64];
    int fs = l.status_fs;
    int y = l.status_y + (l.status_h - fs) / 2;
    int left = l.board_x, right = l.board_x + l.board_w;

    snprintf(buf, sizeof buf, "Pairs %d/%d", g->pairs_found, g->pair_count);
    gfx_text(buf, left, y, fs, TEXT_LIGHT);

    snprintf(buf, sizeof buf, "Moves %d", g->moves);
    gfx_text(buf, right - gfx_measure_text(buf, fs), y, fs, TEXT_DIM);
}

// --------------------------------------------------------------------------
// Menu + win panel (menu.c, the same in every game in this family)
// --------------------------------------------------------------------------
static MenuTheme menu_theme(void) {
    MenuTheme t = { .background = FELT, .panel = MENU_BG, .edge = TEXT_DIM,
                    .title = TEXT_LIGHT, .item = TEXT_DIM, .selected = HILITE };
    return t;
}

// --------------------------------------------------------------------------
// Scenes
// --------------------------------------------------------------------------
typedef struct {
    const Game* g;
    int cursor;
    const char* panel_title;
    const char* panel_sub;
} BoardCtx;

static void draw_board_scene(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    const Game* g = ctx->g;
    Layout l = layout_for(view_w, view_h, g->card_count);

    gfx_clear(FELT);
    draw_title_bar(l);
    draw_status(g, l);
    for (int i = 0; i < g->card_count; i++) draw_card(g, l, i, i == ctx->cursor);

    if (ctx->panel_title) {
        MenuTheme t = menu_theme();
        menu_draw_notice(&t, view_w, view_h, ctx->panel_title, ctx->panel_sub);
    }
}

// --------------------------------------------------------------------------
// Public entry points
// --------------------------------------------------------------------------
void render_frame(const Game* g, int cursor) {
    flip_advance(g);
    BoardCtx ctx = { g, cursor, NULL, NULL };
    present(draw_board_scene, &ctx);
}

void render_win(const Game* g, int cursor) {
    flip_advance(g);
#ifdef OP_TOUCH
    const char* sub = "Tap to continue";
#else
    const char* sub = "Press any key";
#endif
    BoardCtx ctx = { g, cursor, "ALL PAIRS FOUND", sub };
    present(draw_board_scene, &ctx);
}

void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before) {
    MenuTheme t = menu_theme();
    menu_show(&t, title, labels, count, selected, gap_before);
}

int render_card_at(const Game* g, int x, int y) {
    Layout l = layout_for(GetScreenWidth(), GetScreenHeight(), g->card_count);
    return layout_card_at(l, g->card_count, x, y);
}

int render_board_top(const Game* g) {
    return layout_for(GetScreenWidth(), GetScreenHeight(), g->card_count).board_y;
}

int render_card_size(void) {
    // Sized for a full board, so the tap tolerance does not swing with the deal.
    Layout l = layout_for(GetScreenWidth(), GetScreenHeight(),
                          layout_pairs_that_fit(GetScreenWidth(), GetScreenHeight()) * 2);
    return l.card;
}

int render_pairs_that_fit(void) {
    return layout_pairs_that_fit(GetScreenWidth(), GetScreenHeight());
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------
void render_init(void) {
    window_init(GAME_NAME);
    present_init();
}

void render_cleanup(void) {
    present_cleanup();
    window_close();
}
