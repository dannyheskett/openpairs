#include "game.h"

#include <stdlib.h>
#include <string.h>

// How long a mismatched pair stays face up before turning back, in fixed 60 Hz
// steps (see tick.h). Long enough for a child to take the second card in,
// short enough not to feel like a punishment; a tap cuts it short.
#define HIDE_STEPS 54   // 0.9 s

const int LEVEL_PAIRS[LEVEL_COUNT] = { 3, 6, 10, 15, 21 };

const char* const LEVEL_NAME[LEVEL_COUNT] = {
    "Very Easy", "Easy", "Medium", "Hard", "Extra Hard",
};

// xorshift64*, so a seed reproduces a board on every platform. rand() would
// not: its sequence is implementation-defined, and the tests pin exact deals.
static uint64_t next_rand(uint64_t* s) {
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static int rand_below(uint64_t* s, int n) {
    return (int)(next_rand(s) % (uint64_t)n);
}

Game* game_create(int pairs, uint64_t seed) {
    if (pairs < 1) pairs = 1;
    if (pairs > MAX_PAIRS) pairs = MAX_PAIRS;

    Game* g = calloc(1, sizeof *g);
    if (!g) return NULL;

    g->rng = seed ? seed : 1;   // a zero state would stick at zero forever
    g->pair_count = pairs;
    g->card_count = pairs * 2;
    g->first = g->second = -1;

    // Two of each face, then a Fisher-Yates shuffle.
    for (int i = 0; i < g->card_count; i++) {
        g->cards[i].face = i / 2;
        g->cards[i].state = CARD_DOWN;
    }
    for (int i = g->card_count - 1; i > 0; i--) {
        int j = rand_below(&g->rng, i + 1);
        Card t = g->cards[i];
        g->cards[i] = g->cards[j];
        g->cards[j] = t;
    }
    return g;
}

void game_destroy(Game* g) { free(g); }

void game_step_begin(Game* g) { g->events = 0; }

bool game_resolving(const Game* g) { return g->second >= 0; }
bool game_is_over(const Game* g)   { return g->won; }

// Turn the two cards of a resolved mismatch back over.
static void hide_pair(Game* g) {
    if (g->first >= 0 && g->cards[g->first].state == CARD_UP)
        g->cards[g->first].state = CARD_DOWN;
    if (g->second >= 0 && g->cards[g->second].state == CARD_UP)
        g->cards[g->second].state = CARD_DOWN;
    g->first = g->second = -1;
    g->hide_timer = 0;
}

void game_update(Game* g) {
    if (g->hide_timer > 0 && --g->hide_timer == 0) hide_pair(g);
}

bool game_flip(Game* g, int index) {
    if (g->won) return false;

    // A tap while a mismatch is on display ends the pause rather than being
    // swallowed. The tapped card is not turned up by that same tap: the board
    // the player was looking at changes underneath them, and turning up
    // whatever happened to be under their finger is not what they asked for.
    if (game_resolving(g)) {
        if (g->hide_timer > 0) hide_pair(g);
        return false;
    }

    if (index < 0 || index >= g->card_count) return false;
    if (g->cards[index].state != CARD_DOWN) return false;

    g->cards[index].state = CARD_UP;
    g->events |= EV_FLIP;

    if (g->first < 0) {
        g->first = index;
        return true;
    }

    g->second = index;
    g->moves++;

    if (g->cards[g->first].face == g->cards[g->second].face) {
        g->cards[g->first].state = CARD_MATCHED;
        g->cards[g->second].state = CARD_MATCHED;
        g->first = g->second = -1;
        g->pairs_found++;
        g->events |= EV_MATCH;
        if (g->pairs_found == g->pair_count) {
            g->won = true;
            g->events |= EV_WIN;
        }
    } else {
        g->hide_timer = HIDE_STEPS;
        g->events |= EV_MISS;
    }
    return true;
}
