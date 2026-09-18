# openpairs

A matching-pairs game (concentration) for young children, written in C. Cards
start face down; turn two over to find a pair. No timer, no score to chase, and
no way to lose. It runs natively on Windows, macOS, Linux, Android, and iOS, and
in the browser via WebAssembly.

Rendering, input, and audio go through raylib 6.0 on every platform except iOS,
which uses a native Metal backend with no raylib (see
[Architecture](#architecture)). The game logic (`src/game.c`) and the board
layout (`src/layout.c`) are platform-independent and shared unchanged.

## Platforms

| Platform | Build | Orientation | Input |
|----------|-------|-------------|-------|
| Linux / Windows / macOS | native (raylib) | any window shape | mouse + keyboard |
| Web (WASM) | Emscripten (raylib) | any window shape | mouse + keyboard + touch |
| Android | NativeActivity (raylib) | portrait or landscape | touch |
| iOS | native Metal (no raylib) | portrait or landscape, iPhone + iPad | touch |

Unlike the other games in this family there is no second renderer to switch
between: a grid of cards reflows to any window, so one adaptive layout serves a
desktop window, both phone orientations, and an iPad. Every metric is derived
from the live view size each frame, so rotating simply re-fits the same board.

## Rules

- Every face is dealt exactly twice. Turn one card up, then a second.
- A matching pair stays face up. A mismatch turns back after a short pause, or
  at once if you tap again.
- The board is finished when every pair is found. There is no timer and no
  losing.
- Pairs match on **shape**, never on colour alone, so the game works for
  colour-blind players and for children who do not yet read.

## Levels and the screen cap

**Options** offers five levels: Very Easy (3 pairs), Easy (6), Medium (10),
Hard (15) and Extra Hard (21).

The screen has the final say. `src/layout.c` never lays out cards below a
readable floor, so the level's ask is capped by what actually fits: a small
phone tops out where an iPad plays the full board. The Options row shows the
real number for the device in front of you ("Extra Hard: 18 pairs"), and the
count is fixed when a game starts, so rotating re-fits the same board instead of
reshuffling it. The floor scales with the screen's short edge rather than being
a flat pixel count, because a pixel is not a fixed size across densities.

## Controls

**Mouse and keyboard**, on desktop and desktop browsers:

| Input | Action |
|-------|--------|
| Click a card | Turn it up |
| Arrow keys (or W A S D) | Move the highlight |
| Enter / Space | Turn up the highlighted card |
| Escape | Back to the menu, game stays resumable |
| Alt+Enter | Toggle fullscreen |
| Click a menu row | Choose it |
| Up / Down + Enter | Menu navigation; Left / Right cycle a value in Options |

**Touch**, on iOS, Android, and mobile browsers:

| Gesture | Action |
|---------|--------|
| Tap a card | Turn it up |
| Tap above the cards (title / status line) | Back to the menu, game stays resumable |
| Two-finger tap | The same, anywhere on the board |
| Tap a menu row | Choose it; in Options, tapping cycles the value |
| Swipe up / down | Move the menu selection; left / right cycles a value |

## Menu and window

These behave identically in every game in this family (openblocks, openrackem,
openklondike, opencheckers, openpairs, opensweeper). The code for them
(`src/menu.c`, `src/window.c`, `src/present.c`, and the gfx, safe-area,
timing, audio and recorder layers) is the same file in every repo.

- **Menu**: Resume Game (when a game is in progress), New Game, Options (when
  the game has settings), Sound, Record (desktop only), Exit (desktop only, set
  apart by a blank line). Options holds the settings and Back.
- **Menu input**: Up / Down (or W / S) move, Enter / Space choose, Left / Right
  (or A / D) cycle an Options value, Escape backs out. A mouse click or a tap on
  a row chooses it. Swipes move the selection and cycle values.
- **Menu size**: derived from the long edge of the view, so it is the same size
  upright and sideways and grows with the window; it shrinks only when its rows
  would not otherwise fit.
- **Back to the menu**: Escape, Android Back, or a two-finger tap. Losing focus
  (app backgrounded, tab hidden, window deactivated) also returns to the menu;
  the game stays resumable.
- **Window**: desktop opens at 960×720, resizes freely down to 640×480, and
  Alt+Enter toggles borderless fullscreen and back to the previous window.
  Web fills the browser viewport. Android and iOS are fullscreen.

## Building

raylib is built once from source into a gitignored install directory (per
platform) before the game is built. Each `scripts/build_raylib_*.sh` clones
raylib (pinned via `RAYLIB_TAG`, default `6.0`) and installs its headers and
`libraylib.a`. CI runs these scripts before each build.

### Desktop

```bash
./scripts/build_raylib_linux.sh      # once, on a fresh clone
make                                 # -> build/openpairs   (dev, -O2)
make run
make release                         # -> build/openpairs-release (-O3)
```

Windows (mingw-w64 cross-compile) and macOS (universal arm64 + x86_64):

```bash
./scripts/build_raylib_windows.sh && make windows   # -> build/openpairs-x64.exe, -x86.exe
./scripts/build_raylib_mac.sh     && make mac       # -> build/openpairs-mac
```

### Android (needs the Android SDK + NDK)

```bash
./scripts/build_raylib_android.sh
make android        # -> build/openpairs.apk   (debug-signed, sideloadable)
make android-play   # -> build/openpairs.aab   (Play App Bundle; PLAY_* signing vars)
```

A `NativeActivity` with no Gradle; a small `OpenpairsActivity` Java class
(compiled with `javac` + `d8`) handles immersive full screen and hands the
window insets to the layout. arm64-v8a, `targetSdk` 36, 16 KB-page aligned.

### iOS (needs macOS + Xcode; no raylib)

```bash
make ios-sim   # -> build/ios-sim/Openpairs.app   (Simulator, arm64)
make ios       # -> build/openpairs.ipa           (device arm64, unsigned)
```

iPhone and iPad, both orientations, iOS 15+. The `.ipa` is unsigned unless
`IOS_SIGN_IDENTITY` / `IOS_PROFILE` / `IOS_TEAM_ID` are set; AWS Device Farm
re-signs an unsigned one on upload.

### Web (needs Emscripten)

```bash
./scripts/build_raylib_web.sh
make web        # -> build/web/openpairs.{html,js,wasm}
make web-serve  # http://localhost:8080/openpairs.html
```

## Tests

Unit tests with no raylib or window required:

```bash
make test
```

- `test_game` — the deal (every face exactly twice, reproducible from a seed),
  match/mismatch/pause behaviour, winning, and the fixed 60 Hz clock.
- `test_layout` — the level cap on real device shapes, that no board is ever
  laid out below the readable floor in either orientation, that chrome does not
  resize when the device is merely turned, grid shape, the centred short last
  row, and hit testing.
- `test_input` — the touch tap recognizer: release-decided taps, drag rejection,
  two-finger tap, swipes, and slop scaling.

## Continuous integration and releases

Every pull request to `main` builds all platforms via GitHub Actions
([`ci.yml`](.github/workflows/ci.yml)) and runs `make test`. Pushing to `main`
cuts the next `release-N` via [`release.yml`](.github/workflows/release.yml),
which attaches per-platform archives, the Android APK, the iOS `.ipa` and the
WASM bundle to the GitHub Release; when the store secrets are set it also
uploads the AAB to the Play internal track, uploads the `.ipa` to TestFlight and
submits it to App Review. Setup:
[`android/play-assets/KEYSTORE.md`](android/play-assets/KEYSTORE.md) and
[`ios/app-store-assets/TESTFLIGHT.md`](ios/app-store-assets/TESTFLIGHT.md).

## Recording (desktop only)

Toggle **Record: On/Off** from the menu to capture the session to an H.264 MP4
(`openpairs-YYYYMMDD-HHMMSS.mp4`), one video frame per rendered frame, no
external tools. The board is re-rendered at a fixed 640×480 and supersampled
for capture. Mobile and web compile it out.

```bash
./build/openpairs --record            # auto-named file
./build/openpairs --record clip.mp4   # explicit path
```

## Architecture

- `src/game.c` — rules only: the deal, the turn, the mismatch pause, winning.
  No drawing, no input, no platform. Covered by `make test`.
- `src/layout.c` — the board solver: how many pairs fit, the grid, card size and
  hit testing, as pure functions of the view size. This is what makes rotation
  and iPad support fall out rather than being special-cased.
- `src/render.c` — one adaptive renderer: chrome, the card grid, the 21 drawn
  faces, the flip animation, menus. Drawing goes through a small primitive layer
  (`src/gfx.h`): `src/gfx_raylib.c` wraps raylib, `ios/gfx_metal.mm` is a native
  Metal implementation. `src/op_types.h` supplies raylib-compatible types so the
  shared code compiles without raylib on iOS.
- Audio is a similar seam (`src/audio.h`): `src/audio_raylib.c` vs
  `ios/audio_ios.mm` (AVAudioEngine). Effects are synthesized at startup; sound
  is off by default.
- iOS backend: `ios/plat_ios.mm` (touch / screen / timing) and `ios/ios_main.mm`
  (UIKit app + `CAMetalLayer` view + `CADisplayLink` loop).
- `src/safe_area.c` carries all four window insets, not just the top: sideways,
  the camera cutout and the gesture bar move to a side edge.
- All text is the bundled Nunito SemiBold (SIL OFL, see `NOTICE`), embedded so
  there is no runtime asset file. There are no asset files at all: card faces
  are drawn, sounds are synthesized, icons and store screenshots are generated
  (`scripts/gen_icons.py`, `scripts/gen_store_screenshots.mjs`).

## Dependencies

- A C99 compiler (GCC or Clang); a C++ / Objective-C++ compiler for the iOS
  backend.
- [raylib](https://github.com/raysan5/raylib) 6.0 (static) on all platforms
  except iOS, built by the `scripts/build_raylib_*.sh` helpers.
- The MP4 recorder uses two vendored public-domain (CC0) single-header
  libraries: [minih264](third_party/minih264) and [minimp4](third_party/minimp4).

## Project structure

```
openpairs/
├── src/            # shared C sources + gfx/audio raylib backends
├── ios/            # native Metal / UIKit backend (Objective-C++) + App Store assets
├── android/        # NativeActivity manifest, resources, Java activity + Play assets
├── web/            # Emscripten HTML shell
├── scripts/        # raylib build scripts, asset/font generators, store tooling
├── third_party/    # vendored single-header libs + Nunito
├── tests/          # game, layout and input unit tests
├── Makefile
├── LICENSE         # MIT (this project's own code)
└── NOTICE          # third-party attributions
```

## License

openpairs' own code is released under the [MIT License](LICENSE). The vendored
`minih264` and `minimp4` libraries are public domain (CC0), and the Nunito font
is under the SIL Open Font License; see [NOTICE](NOTICE) for attributions.
