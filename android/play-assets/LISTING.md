# openpairs — Google Play store listing

Copy/paste these into the Play Console (**Grow → Store presence → Main store
listing**, plus **Store settings** for category). The images in this folder are
the shipped assets. `scripts/play_release.py` pushes the fields below and the
images from this folder, so edit them here rather than in the console.

The screenshots regenerate with `scripts/gen_store_screenshots.mjs`, and the
icon and feature graphic with `scripts/gen_icons.py`.

## Assets (this folder)

| File | Play field | Spec |
|------|-----------|------|
| `icon-512.png` | App icon | 512×512 PNG (32-bit) |
| `feature-graphic-1024x500.png` | Feature graphic | 1024×500 PNG/JPG |
| `screenshots/phone/` | Phone screenshots | 4× 1080×1920 PNG |
| `screenshots/phone-landscape/` | Phone screenshots (sideways) | 4× 1920×1080 PNG |
| `screenshots/tablet/` | 7-inch and 10-inch tablet screenshots | 4× 1600×2560 PNG |
| `screenshots/tablet-landscape/` | Tablet screenshots (sideways) | 4× 2560×1600 PNG |

The game plays in both orientations, so each slot has an upright and a sideways
set. Upload one orientation per slot, not a mixture.

## App name (≤30 chars)

```
openpairs
```

## Short description (≤80 chars)

```
Turn cards over and find the matching pairs. Free, no ads, no tracking.
```

## Full description (≤4000 chars)

```
A matching pairs game for young children. Cards start face down. Turn two over: if they match, they stay up; if not, they turn back. Find every pair to finish the board.

No ads. No tracking. No accounts. No in-app purchases. No timers and no way to lose. openpairs requests zero permissions and never touches the network. It's just the game.

MADE FOR SMALL PLAYERS
• Nothing to read: every card is a simple, bright shape
• Pairs match by shape, not colour, so it works for colour-blind players too
• Big cards with plenty of space between them
• No timer, no score to chase, no losing — the board simply gets finished

FIVE SIZES
• Very Easy starts at three pairs, and Easy, Medium, Hard and Extra Hard add more
• The board fits your screen: a tablet shows the biggest boards, and a phone quietly keeps the cards large enough for small fingers
• Play upright or sideways, on a phone or a tablet

BUILT RIGHT
• Fully offline — perfect for flights, waiting rooms, anywhere
• Tiny download, easy on your battery

FREE AND OPEN SOURCE
openpairs is open source. Read the code, report a bug, or build it yourself: https://github.com/dannyheskett/openpairs
```

## Categorization (Store settings)

- **App or game:** Game
- **Category:** Educational
- **Tags:** educational, puzzle, brain games, kids
- **Email:** dan@danheskett.com
- **Website:** https://danheskett.com
- **Content rating:** Everyone (no objectionable content; IARC questionnaire —
  answer "no" to all violence/adult/gambling items)
- **Target audience:** ages 5 and under, 6-8, 9-12 (a children's app; the
  declaration and the Families policy apply)

## Data safety (Policy → App content)

- Data collected: **None**
- Data shared: **None**
- App has no `INTERNET` permission (verify in the manifest) → "no data
  transmitted off the device" is truthful.
- Privacy policy URL: **https://danheskett.com/app/privacy-policy/**

## Screenshots

Pushed via the Play API from the folders above. Captured from the web build --
the same C the Android app runs -- in a headless browser at each target size:

    npm i playwright-core
    make web
    node scripts/gen_store_screenshots.mjs --src build/web

That one command also refreshes the App Store sets under
`ios/app-store-assets/screenshots/`, so the two listings cannot drift apart.
Regenerate whenever the board, the chrome or the font changes.
