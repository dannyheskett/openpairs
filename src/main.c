#include "game.h"
#include "layout.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include "app.h"
#include "tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

typedef enum { STATE_MENU, STATE_OPTIONS, STATE_PLAYING, STATE_WON } AppState;

typedef enum {
    ACT_RESUME, ACT_NEW, ACT_OPTIONS, ACT_SOUND, ACT_RECORD, ACT_EXIT,
} MenuAction;

#define MAX_MENU_ITEMS 6

static void play_event_sounds(unsigned ev) {
    if (ev & EV_WIN)   { sound_play(SFX_WIN); return; }
    if (ev & EV_MATCH)   sound_play(SFX_MATCH);
    else if (ev & EV_MISS) sound_play(SFX_MISS);
    else if (ev & EV_FLIP) sound_play(SFX_FLIP);
}

// Fill labels[]/actions[] with the current menu. Returns the item count and
// sets *gap_before to the index that should have a blank line above it -- Exit,
// which is set apart from the rest -- or -1 when this build has no Exit item at
// all (mobile and web, where the OS or the browser tab owns the lifecycle).
static int build_menu(bool resumable, const char** labels, MenuAction* actions,
                      int* gap_before) {
    int n = 0;
    *gap_before = -1;
    if (resumable) { labels[n] = "Resume Game";                     actions[n++] = ACT_RESUME; }
    labels[n] = "New Game";                                         actions[n++] = ACT_NEW;
    labels[n] = "Options";                                          actions[n++] = ACT_OPTIONS;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off";    actions[n++] = ACT_SOUND;
#ifndef OP_TOUCH
    // The mp4 recorder is a desktop-only feature (stubbed out on mobile/web), so
    // the toggle would do nothing there -- omit it.
    labels[n] = recorder_active()  ? "Record: On" : "Record: Off";  actions[n++] = ACT_RECORD;
#endif
#if defined(PLATFORM_WEB)
    // A browser tab can't be closed from code, so no Exit on web.
#elif !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    // Mobile apps don't self-terminate (the OS owns the lifecycle: home gesture /
    // back button on Android, Apple guidelines on iOS), so no Exit on either.
    *gap_before = n;
    labels[n] = "Exit";                                             actions[n++] = ACT_EXIT;
#endif
    return n;
}

// The Options screen: the level, plus Back. The level row shows the number of
// pairs this screen can actually hold, which is the level's ask capped by the
// window -- so a phone reading "Extra Hard: 15 pairs" is telling the truth
// rather than promising 21 and quietly dealing fewer.
#define OPT_ITEMS 2
enum { OPT_LEVEL, OPT_BACK };

static int pairs_for(Level level) {
    int want = LEVEL_PAIRS[level];
    int fits = render_pairs_that_fit();
    return (want < fits) ? want : fits;
}

static int build_options(Level level, const char** labels) {
    static char buf[OPT_ITEMS][40];
    snprintf(buf[OPT_LEVEL], sizeof buf[0], "%s: %d pairs",
             LEVEL_NAME[level], pairs_for(level));
    snprintf(buf[OPT_BACK], sizeof buf[0], "Back");
    for (int i = 0; i < OPT_ITEMS; i++) labels[i] = buf[i];
    return OPT_ITEMS;
}

static void cycle_option(Level* level, int item, int dir) {
    if (item == OPT_LEVEL)
        *level = (Level)((*level + LEVEL_COUNT + dir) % LEVEL_COUNT);
}

// App state carried across frames. Kept in one struct so the web and iOS builds
// can drive the loop from a per-frame callback (neither can block).
typedef struct {
    Game* game;
    AppState state;
    int  selected;     // menu / options cursor
    int  cursor;       // keyboard card cursor, -1 while using a pointer
    Level level;
    bool quit;
    SimClock clock;    // fixed-timestep accumulator (only advanced while playing)
    double prev_time;  // GetTime() at the previous frame; 0 before the first
} AppCtx;

static void app_ctx_init(AppCtx* c) {
    c->game = NULL;
    c->state = STATE_MENU;
    c->selected = 0;
    c->cursor = -1;
    c->level = LEVEL_EASY;
    c->quit = false;
    sim_clock_reset(&c->clock);
    c->prev_time = 0.0;
}

// Deal a fresh board at the level's pair count, capped to what the live window
// can show at a readable card size. The count is fixed for the life of the
// game, so rotating re-fits the same board instead of reshuffling it.
static void start_new_game(AppCtx* c) {
    if (c->game) game_destroy(c->game);
    uint64_t seed = (uint64_t)time(NULL) ^ ((uint64_t)rand() << 16) ^ (uint64_t)rand();
    c->game = game_create(pairs_for(c->level), seed);
    c->cursor = -1;
    c->state = STATE_PLAYING;
    sound_play(SFX_DEAL);
}

// Move the keyboard cursor. The first press adopts the cursor rather than
// moving it, so the highlight appears where the player can see it.
static void move_cursor(AppCtx* c, int dx, int dy) {
    Layout l = layout_for(GetScreenWidth(), GetScreenHeight(), c->game->card_count);
    if (c->cursor < 0) { c->cursor = 0; return; }
    int cols = (l.cols > 0) ? l.cols : 1;
    int r = c->cursor / cols, col = c->cursor % cols;
    int n = c->game->card_count;
    for (int guard = 0; guard < 64; guard++) {
        col += dx; r += dy;
        if (col < 0) col = cols - 1;
        if (col >= cols) col = 0;
        if (r < 0) r = (n - 1) / cols;
        if (r > (n - 1) / cols) r = 0;
        int idx = r * cols + col;
        if (idx >= 0 && idx < n) { c->cursor = idx; return; }
        if (dx == 0 && dy == 0) return;
    }
}

// One iteration of the game loop. `arg` is an AppCtx* (void* to match the
// emscripten_set_main_loop callback signature).
static void frame_step(void* arg) {
    AppCtx* c = (AppCtx*)arg;

    double now = GetTime();
    double dt = (c->prev_time > 0.0) ? now - c->prev_time : SIM_DT;
    c->prev_time = now;
    if (c->state != STATE_PLAYING) sim_clock_reset(&c->clock);

    Input in = input_poll();
    if (in.fullscreen_toggle) render_toggle_fullscreen();

    bool resumable = (c->game != NULL && !game_is_over(c->game));
    const char* labels[MAX_MENU_ITEMS];
    MenuAction actions[MAX_MENU_ITEMS];
    int gap_before = -1;
    int menu_count = build_menu(resumable, labels, actions, &gap_before);

    switch (c->state) {
    case STATE_MENU: {
        if (c->selected >= menu_count) c->selected = 0;
        if (in.escape_pressed) {
            if (resumable) { c->state = STATE_PLAYING; break; }
            c->quit = true; return;
        }
        if (in.menu_up)   { c->selected = (c->selected + menu_count - 1) % menu_count; sound_play(SFX_MENU_MOVE); }
        if (in.menu_down) { c->selected = (c->selected + 1) % menu_count;              sound_play(SFX_MENU_MOVE); }
        bool do_select = in.select_pressed;
        if (in.touch_tap || in.left_pressed) {
            Vector2 p = in.touch_tap ? (Vector2){in.tap_x, in.tap_y}
                                     : (Vector2){(float)in.mouse_x, (float)in.mouse_y};
            int hit = render_menu_hit_test(p);
            if (hit >= 0 && hit < menu_count) { c->selected = hit; do_select = true; }
        }
        if (do_select) {
            sound_play(SFX_MENU_SELECT);
            switch (actions[c->selected]) {
            case ACT_RESUME: c->state = STATE_PLAYING; break;
            case ACT_NEW:
                start_new_game(c);
                if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                break;
            case ACT_OPTIONS: c->state = STATE_OPTIONS; c->selected = 0; break;
            case ACT_SOUND:   sound_toggle(); sound_play(SFX_MENU_SELECT); break;
            case ACT_RECORD:  recorder_toggle(); break;
            case ACT_EXIT:    c->quit = true; return;
            }
        }
        break;
    }

    case STATE_OPTIONS: {
        const char* opt_labels[OPT_ITEMS];
        int opt_count = build_options(c->level, opt_labels);
        if (c->selected >= opt_count) c->selected = 0;
        if (in.escape_pressed) { c->state = STATE_MENU; c->selected = 0; break; }
        if (in.menu_up)   { c->selected = (c->selected + opt_count - 1) % opt_count; sound_play(SFX_MENU_MOVE); }
        if (in.menu_down) { c->selected = (c->selected + 1) % opt_count;             sound_play(SFX_MENU_MOVE); }
        int dir = (in.menu_right ? 1 : 0) - (in.menu_left ? 1 : 0);
        bool do_select = in.select_pressed;
        if (in.touch_tap || in.left_pressed) {
            Vector2 p = in.touch_tap ? (Vector2){in.tap_x, in.tap_y}
                                     : (Vector2){(float)in.mouse_x, (float)in.mouse_y};
            int hit = render_menu_hit_test(p);
            if (hit >= 0 && hit < opt_count) { c->selected = hit; do_select = true; }
        }
        if (do_select && c->selected == OPT_BACK) {
            c->state = STATE_MENU;
            c->selected = 0;
            sound_play(SFX_MENU_SELECT);
        } else if (dir != 0 || do_select) {
            cycle_option(&c->level, c->selected, dir ? dir : 1);
            sound_play(SFX_MENU_SELECT);
        }
        break;
    }

    case STATE_PLAYING: {
        Game* g = c->game;
        if (!g) { c->state = STATE_MENU; break; }
#ifdef OP_TOUCH
        // Backgrounded (Android / iOS) or the browser tab lost focus: fall back
        // to the menu. The game stays resumable.
        if (!render_window_focused()) { c->state = STATE_MENU; c->selected = 0; break; }
#endif
        if (in.escape_pressed) { c->state = STATE_MENU; c->selected = 0; break; }

        game_step_begin(g);
        int steps = sim_clock_advance(&c->clock, dt);
        for (int s = 0; s < steps; s++) game_update(g);

        // Keyboard: arrows move a highlight, Enter/Space turns that card up.
        if (in.menu_up || in.menu_down || in.menu_left || in.menu_right) {
            move_cursor(c, (in.menu_right ? 1 : 0) - (in.menu_left ? 1 : 0),
                           (in.menu_down ? 1 : 0) - (in.menu_up ? 1 : 0));
            sound_play(SFX_MENU_MOVE);
        }
        if (in.select_pressed && c->cursor >= 0) game_flip(g, c->cursor);

        // Pointer: a tap (touch) or a click (mouse) turns up the card under it.
        if (in.touch_tap || in.left_pressed) {
            int x = in.touch_tap ? (int)in.tap_x : in.mouse_x;
            int y = in.touch_tap ? (int)in.tap_y : in.mouse_y;
            int idx = render_card_at(g, x, y);
            c->cursor = -1;             // pointer takes over from the keyboard
            if (idx >= 0) {
                game_flip(g, idx);
            } else if (in.touch_tap && y < render_board_top(g)) {
                // On touch, a tap above the cards -- the title bar and the
                // pairs/moves line -- opens the menu. A child rarely finds the
                // two-finger tap on their own; "touch the top" is a menu button
                // they can see. Desktop has Escape, so a stray click up there
                // does nothing.
                c->state = STATE_MENU;
                c->selected = 0;
                sound_play(SFX_MENU_SELECT);
                break;
            } else if (game_resolving(g)) {
                game_flip(g, -1);       // tap anywhere else ends the pause
            }
        }

        play_event_sounds(g->events);
        if (game_is_over(g)) c->state = STATE_WON;
        break;
    }

    case STATE_WON:
        if (in.escape_pressed || (in.any_pressed && !in.fullscreen_toggle)) {
            c->state = STATE_MENU;
            c->selected = 0;
        }
        break;
    }

    // Render for the current state.
    if (c->state == STATE_MENU) {
        render_menu("OPENPAIRS", labels, menu_count, c->selected, gap_before);
    } else if (c->state == STATE_OPTIONS) {
        const char* opt_labels[OPT_ITEMS];
        int opt_count = build_options(c->level, opt_labels);
        render_menu("OPTIONS", opt_labels, opt_count, c->selected, OPT_BACK);
    } else if (c->state == STATE_WON) {
        render_win(c->game, c->cursor);
    } else {
        render_frame(c->game, c->cursor);
    }
}

#if defined(PLATFORM_IOS)

// iOS: UIKit provides main() and the run loop, so the main() below is compiled
// out. The app shell (ios/ios_main.mm) sets up the Metal layer, calls
// op_app_init() once, then op_app_frame() from a CADisplayLink each frame.
static AppCtx ios_ctx;

void op_app_init(void) {
    srand((unsigned int)time(NULL));
    render_init();   // no-op on iOS (UIKit owns the window)
    sound_init();
    app_ctx_init(&ios_ctx);
}

void op_app_frame(void) { frame_step(&ios_ctx); }

#else

int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    // CLI: --record [path] starts recording immediately (auto-named if no path).
    bool cli_record = false;
    const char* cli_record_path = NULL;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--record") == 0) {
            cli_record = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') cli_record_path = argv[++i];
        }

    render_init();
    sound_init();
    if (cli_record) recorder_start(cli_record_path);

    // Static so the pointer handed to emscripten stays valid after main()'s
    // stack is unwound on the web build.
    static AppCtx ctx;
    app_ctx_init(&ctx);

#ifdef PLATFORM_WEB
    emscripten_set_main_loop_arg(frame_step, &ctx, 0, 1);
#else
    while (!render_window_should_close() && !ctx.quit) {
        frame_step(&ctx);
    }
    recorder_stop();
    if (ctx.game) game_destroy(ctx.game);
    sound_shutdown();
    render_cleanup();
#endif
    return 0;
}

#endif // PLATFORM_IOS
