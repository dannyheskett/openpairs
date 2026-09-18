# openpairs — App Store listing

Copy/paste into App Store Connect. Mirrors `android/play-assets/LISTING.md`, with
the differences Apple requires (subtitle, keywords, promotional text).
`scripts/asc_release.py listing` pushes the fields below and the screenshots,
so edit them here rather than in the console.

One rule that differs from Play:

- **Never mention Android, Google Play, or another platform** in the description.
  Apple rejects listings that reference competing stores.

## New App form (My Apps -> + -> New App)

| Field | Value |
| --- | --- |
| Platform | iOS |
| Name | `openpairs` (must be unique App Store-wide; see fallbacks below) |
| Primary Language | English (U.S.) |
| Bundle ID | `com.danheskett.openpairs` |
| SKU | `openpairs` |
| User Access | Full Access |

If `openpairs` is taken, in order of preference: `openpairs Match`,
`openpairs Cards`, `openpairs Game`. The name is public, capped at 30
characters, and can be changed with any later version — the SKU and bundle ID
cannot.

## Subtitle (<=30 chars)

```
Find the matching pairs
```

## Promotional text (<=170 chars)

Editable anytime without submitting a new build — use it for release notes or
seasonal copy.

```
No ads, no tracking, no timers. A matching pairs game for small children, free and open source.
```

## Keywords (<=100 chars, comma-separated, no spaces after commas)

Do not repeat the app name — it is already indexed.

```
pairs,match,matching game,concentration,kids,toddler,preschool,shapes,brain,offline
```

## Description (<=4000 chars)

```
A matching pairs game for young children. Cards start face down. Turn two over: if they match, they stay up; if not, they turn back. Find every pair to finish the board.

No ads. No tracking. No accounts. No in-app purchases. No timers and no way to lose. openpairs never touches the network. It's just the game. You can even play it in airplane-mode.

MADE FOR SMALL PLAYERS
• Nothing to read: every card is a simple, bright shape
• Pairs match by shape, not colour, so it works for colour-blind players too
• Big cards with plenty of space between them
• No timer, no score to chase, no losing — the board simply gets finished

FIVE SIZES
• Very Easy starts at three pairs, and Easy, Medium, Hard and Extra Hard add more
• The board fits your screen: an iPad shows the biggest boards, and an iPhone quietly keeps the cards large enough for small fingers
• Play upright or sideways, on iPhone or iPad

FREE AND OPEN SOURCE
openpairs is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/openpairs
```

## App information

- **Category (primary):** Games -> Family
- **Category (secondary):** Games -> Puzzle
- **Content Rights:** does not contain third-party content
- **Age Rating:** answer "None" to every question -> **4+**
- **Copyright:** `2026 Daniel Heskett`
- **Support URL:** https://danheskett.com
- **Marketing URL:** https://danheskett.com/projects/openpairs/
- **Privacy Policy URL:** https://danheskett.com/app/privacy-policy/

## Screenshots

Two device families are required because the app declares iPhone and iPad
(`UIDeviceFamily [1,2]` in `ios/Info.plist`): the 6.9" iPhone set (1290x2796)
and the 13" iPad set (2064x2752). Each has an upright and a sideways folder;
upload one orientation per slot, not a mixture.

Captured from the web build -- the same C the app runs -- in a headless browser:

    npm i playwright-core
    make web
    node scripts/gen_store_screenshots.mjs --src build/web

## App Privacy (App Store Connect -> App Privacy)

Answer **"No, we do not collect data from this app."** — accurate and verified:
no network code, no analytics SDK, no permissions requested. This yields a
"Data Not Collected" privacy label. It has no API, so it is set once by hand,
and it must be **published** (the button at the top right) before a version can
be submitted.

## Kids Category

The listing does **not** opt into Apple's Kids Category. That category adds
review requirements (no outbound links, a parental gate) and the app has no
links or purchases to gate anyway; Games -> Family with a 4+ rating reaches the
same audience without them.

## Pricing

Free. No in-app purchases.

## Export compliance

openpairs uses no encryption of any kind. `ITSAppUsesNonExemptEncryption = false`
in `ios/Info.plist` stops App Store Connect asking on every upload.
