// Unit tests for openpairs' rules and the fixed-timestep clock — no raylib, no
// window. game.c and tick.c are included directly so their file-static helpers
// are visible.
//
// Built and run by `make test`. A non-zero exit means a failure.
#include "../src/game.c"
#include "../src/tick.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            failures++;                                                      \
        }                                                                    \
    } while (0)

// Run the mismatch pause to completion.
static void settle(Game* g) {
    for (int i = 0; i < HIDE_STEPS + 2; i++) { game_step_begin(g); game_update(g); }
}

// --- The deal ---------------------------------------------------------------
static void test_deal(void) {
    Game* g = game_create(10, 12345);
    CHECK(g->pair_count == 10);
    CHECK(g->card_count == 20);

    int seen[MAX_FACES] = {0};
    for (int i = 0; i < g->card_count; i++) {
        CHECK(g->cards[i].state == CARD_DOWN);
        CHECK(g->cards[i].face >= 0 && g->cards[i].face < 10);
        seen[g->cards[i].face]++;
    }
    for (int f = 0; f < 10; f++) CHECK(seen[f] == 2);   // every face exactly twice
    game_destroy(g);
}

static void test_deal_is_reproducible(void) {
    Game* a = game_create(8, 999);
    Game* b = game_create(8, 999);
    Game* c = game_create(8, 1000);
    bool same = true, differs = false;
    for (int i = 0; i < a->card_count; i++) {
        if (a->cards[i].face != b->cards[i].face) same = false;
        if (a->cards[i].face != c->cards[i].face) differs = true;
    }
    CHECK(same);      // one seed, one board
    CHECK(differs);   // a different seed deals a different board
    game_destroy(a); game_destroy(b); game_destroy(c);
}

static void test_deal_clamps(void) {
    Game* g = game_create(0, 1);
    CHECK(g->pair_count == 1);
    game_destroy(g);
    g = game_create(MAX_PAIRS + 50, 1);
    CHECK(g->pair_count == MAX_PAIRS);
    game_destroy(g);
}

// Find two indices holding the same face, and one holding a different one.
static void find_pair(const Game* g, int* a, int* b, int* other) {
    *a = *b = *other = -1;
    for (int i = 0; i < g->card_count && *b < 0; i++)
        for (int j = i + 1; j < g->card_count; j++)
            if (g->cards[i].face == g->cards[j].face) { *a = i; *b = j; break; }
    for (int i = 0; i < g->card_count; i++)
        if (i != *a && g->cards[i].face != g->cards[*a].face) { *other = i; break; }
}

// --- A matching turn --------------------------------------------------------
static void test_match_stays_up(void) {
    Game* g = game_create(6, 7);
    int a, b, other;
    find_pair(g, &a, &b, &other);

    game_step_begin(g);
    CHECK(game_flip(g, a));
    CHECK(g->cards[a].state == CARD_UP);
    CHECK(g->events & EV_FLIP);
    CHECK(g->moves == 0);            // a turn is two cards

    game_step_begin(g);
    CHECK(game_flip(g, b));
    CHECK(g->events & EV_MATCH);
    CHECK(g->cards[a].state == CARD_MATCHED);
    CHECK(g->cards[b].state == CARD_MATCHED);
    CHECK(g->pairs_found == 1);
    CHECK(g->moves == 1);
    CHECK(!game_resolving(g));       // matched pairs resolve immediately

    settle(g);                        // and stay up
    CHECK(g->cards[a].state == CARD_MATCHED);
    game_destroy(g);
}

// --- A mismatched turn ------------------------------------------------------
static void test_miss_turns_back(void) {
    Game* g = game_create(6, 7);
    int a, b, other;
    find_pair(g, &a, &b, &other);

    game_step_begin(g); game_flip(g, a);
    game_step_begin(g); game_flip(g, other);
    CHECK(g->events & EV_MISS);
    CHECK(game_resolving(g));
    CHECK(g->cards[a].state == CARD_UP);
    CHECK(g->pairs_found == 0);
    CHECK(g->moves == 1);

    // Still up while the pause runs.
    for (int i = 0; i < HIDE_STEPS - 1; i++) { game_step_begin(g); game_update(g); }
    CHECK(g->cards[a].state == CARD_UP);

    game_step_begin(g); game_update(g);
    CHECK(g->cards[a].state == CARD_DOWN);
    CHECK(g->cards[other].state == CARD_DOWN);
    CHECK(!game_resolving(g));
    game_destroy(g);
}

static void test_tap_ends_the_pause(void) {
    Game* g = game_create(6, 7);
    int a, b, other;
    find_pair(g, &a, &b, &other);
    game_step_begin(g); game_flip(g, a);
    game_step_begin(g); game_flip(g, other);

    // A tap during the pause turns the pair back at once, and does NOT turn up
    // whatever was tapped: the board the player was looking at just changed.
    game_step_begin(g);
    CHECK(!game_flip(g, b));
    CHECK(g->cards[a].state == CARD_DOWN);
    CHECK(g->cards[b].state == CARD_DOWN);
    CHECK(!game_resolving(g));
    game_destroy(g);
}

// --- Flips that must be ignored ---------------------------------------------
static void test_bad_flips_ignored(void) {
    Game* g = game_create(4, 3);
    game_step_begin(g);
    CHECK(!game_flip(g, -1));
    CHECK(!game_flip(g, g->card_count));
    CHECK(game_flip(g, 2));
    CHECK(!game_flip(g, 2));         // already face up
    game_destroy(g);
}

// --- Winning ----------------------------------------------------------------
static void test_win(void) {
    Game* g = game_create(3, 5150);
    int matched = 0;
    for (int f = 0; f < g->pair_count; f++) {
        int a = -1, b = -1;
        for (int i = 0; i < g->card_count; i++) {
            if (g->cards[i].face != f) continue;
            if (a < 0) a = i; else b = i;
        }
        game_step_begin(g); game_flip(g, a);
        game_step_begin(g); game_flip(g, b);
        matched++;
        CHECK(g->pairs_found == matched);
    }
    CHECK(game_is_over(g));
    CHECK(g->events & EV_WIN);
    CHECK(g->moves == g->pair_count);
    game_step_begin(g);
    CHECK(!game_flip(g, 0));         // the board is finished
    game_destroy(g);
}

// --- Fixed-timestep accumulator (tick.c) ------------------------------------
// The mismatch pause is counted in these steps, so it must hold 60 steps/s on
// any display refresh.
static void test_sim_clock(void) {
    SimClock c = {0};
    CHECK(sim_clock_advance(&c, SIM_DT) == 1);
    CHECK(c.accum == 0.0);

    c.accum = 0.0;
    CHECK(sim_clock_advance(&c, SIM_DT / 2) == 0);
    CHECK(sim_clock_advance(&c, SIM_DT / 2) == 1);

    c.accum = 0.0;
    int total = 0;
    for (int i = 0; i < 120; i++) total += sim_clock_advance(&c, SIM_DT / 2);
    CHECK(total == 60);              // a 120 Hz display still runs 60 steps/s

    c.accum = 0.0;
    CHECK(sim_clock_advance(&c, 100.0) == SIM_MAX_STEPS);   // spiral guard
    CHECK(c.accum == 0.0);

    c.accum = 0.0;
    CHECK(sim_clock_advance(&c, -1.0) == 0);                // stalled clock
    c.accum = 1.0;
    sim_clock_reset(&c);
    CHECK(c.accum == 0.0);
}

int main(void) {
    printf("test_game: deal, match, miss, pause, win, fixed clock\n");
    test_deal();
    test_deal_is_reproducible();
    test_deal_clamps();
    test_match_stays_up();
    test_miss_turns_back();
    test_tap_ends_the_pause();
    test_bad_flips_ignored();
    test_win();
    test_sim_clock();
    if (failures == 0) { printf("OK: all checks passed\n"); return 0; }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
