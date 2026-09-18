#ifndef OPENPAIRS_GAME_H
#define OPENPAIRS_GAME_H

#include <stdbool.h>
#include <stdint.h>

// --------------------------------------------------------------------------
// Matching pairs (concentration). A grid of face-down cards, every face dealt
// exactly twice. Turn one card up, then a second: a matching pair stays up, a
// mismatch turns back after a short pause. The game ends when every pair is
// found; there is no timer and no way to lose.
//
// Pure logic: no drawing, no input, no raylib. The board is a flat array of
// cards in deal order; the renderer arranges them into rows and columns.
// --------------------------------------------------------------------------

// The five levels ask for this many pairs. The screen can cap the count (see
// layout.h, pairs_that_fit): the game is created with the capped number, so a
// small phone plays a smaller board than an iPad at the same level.
typedef enum {
    LEVEL_VERY_EASY = 0,
    LEVEL_EASY,
    LEVEL_MEDIUM,
    LEVEL_HARD,
    LEVEL_EXTRA_HARD,
    LEVEL_COUNT,
} Level;

extern const int LEVEL_PAIRS[LEVEL_COUNT];
extern const char* const LEVEL_NAME[LEVEL_COUNT];

// Distinct card faces available. Faces are drawn shapes (render.c), and a pair
// matches on FACE, never on colour alone, so the game works for colour-blind
// players and for children who do not yet read.
#define MAX_FACES 21
#define MAX_PAIRS MAX_FACES
#define MAX_CARDS (MAX_PAIRS * 2)

typedef enum {
    CARD_DOWN = 0,  // face down
    CARD_UP,        // face up, being looked at this turn
    CARD_MATCHED,   // face up for good
} CardState;

typedef struct {
    int       face;   // 0..MAX_FACES-1; the pair key
    CardState state;
} Card;

// Per-frame event flags consumed by main.c to drive sound.
enum {
    EV_FLIP  = 1 << 0,
    EV_MATCH = 1 << 1,
    EV_MISS  = 1 << 2,
    EV_WIN   = 1 << 3,
};

typedef struct {
    Card cards[MAX_CARDS];
    int  card_count;
    int  pair_count;
    int  pairs_found;
    int  moves;          // completed two-card turns

    // The turn in progress: indices of the cards face up, -1 when unused.
    int  first, second;
    int  hide_timer;     // fixed 60 Hz steps left before a mismatch turns back

    bool won;
    unsigned events;     // EV_* set this step, cleared by game_step_begin
    uint64_t rng;        // deal shuffle; seeded by the caller
} Game;

// Lifecycle ----------------------------------------------------------------
// Deal `pairs` pairs (clamped to 1..MAX_PAIRS) shuffled from `seed`. The same
// seed always deals the same board, which is what the tests rely on.
Game* game_create(int pairs, uint64_t seed);
void  game_destroy(Game* g);
void  game_step_begin(Game* g);   // clear per-step events

// Simulation ---------------------------------------------------------------
// Advance one fixed 60 Hz step: runs the pause that holds a mismatched pair
// face up before turning it back.
void game_update(Game* g);

// Interaction --------------------------------------------------------------
// Turn up the card at `index`. Ignored for an invalid index, a card that is
// already up or matched, or a third card while a pair is resolving -- except
// that tapping during the mismatch pause ends it immediately, which is what an
// impatient player expects. Returns true if a card was turned up.
bool game_flip(Game* g, int index);

// True while two cards are up and being compared (the brief look-at window).
bool game_resolving(const Game* g);
bool game_is_over(const Game* g);

#endif
