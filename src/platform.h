#ifndef OPENPAIRS_PLATFORM_H
#define OPENPAIRS_PLATFORM_H

// The game's name: window title and recording file prefix.
#define GAME_NAME "openpairs"

// Recording size (recorder.c): the smallest desktop window. Both are multiples
// of 16 for the H.264 encoder.
#define REC_W 640
#define REC_H 480

// OP_TOUCH selects the touch frontend: tap-driven cards and menus, and no
// on-screen chrome that assumes a mouse. It is enabled on Android, iOS, and the
// WebAssembly build (which targets mobile browsers but also accepts a mouse and
// keyboard for desktop browsers). Desktop native builds leave it unset.
//
// raylib defines PLATFORM_ANDROID / PLATFORM_WEB for its own sources; our build
// passes the matching -D for the game translation units.
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB) || defined(PLATFORM_IOS)
#define OP_TOUCH 1
#endif

// Unlike the other games in this family there is no second renderer to select.
// A grid of cards reflows to any window, so one adaptive layout serves the
// desktop, both phone orientations, and an iPad: src/layout.c derives every
// metric from the live view size each frame, and rotating simply re-fits the
// same board. A window too small for a level caps it, as a small phone does.

#endif // OPENPAIRS_PLATFORM_H
