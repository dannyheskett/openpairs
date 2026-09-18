// Capture the Google Play and App Store screenshots from the web build.
//
// openpairs has ONE adaptive layout, so the web build renders what a phone, a
// tablet and an iPad render -- a headless browser at the right viewport size
// produces pixel-equivalent frames without a device or a farm. Both stores want
// phone and tablet sets, and this game ships in both orientations, so each slot
// gets its own folder and you upload whichever orientation you want it to show.
//
// Regenerate whenever the portrait UI changes. The committed PNGs under
// android/play-assets/screenshots/ and ios/app-store-assets/screenshots/ are what
// scripts/play_release.py and scripts/asc_release.py push to the stores.
//
// Usage:
//   npm i playwright-core                    # not a repo dependency; dev-only
//   make web   (or: gh release download release-N -p '*-web-wasm.zip' && unzip it)
//   node scripts/gen_store_screenshots.mjs --src build/web
//
// Options:
//   --src <dir>      web bundle directory (contains openpairs.html) [required]
//   --out <dir>      repo root to write screenshots under         [default: cwd]
//   --chrome <path>  Chromium binary  [default: $CHROME, else the Playwright cache]
//   --only <name>    capture a single target (see TARGETS below)
//
// Why a browser and not the real app: the maintainer has no Android device and no
// Mac, and Device Farm returns video, not clean full-resolution stills.

import { chromium } from 'playwright-core';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { existsSync, readdirSync } from 'node:fs';
import { extname, join, resolve } from 'node:path';
import { homedir } from 'node:os';

// ---------------------------------------------------------------------------
// Targets. Play's tablet slot takes the same 9:16 frames at 2x, which is why one
// capture pass fills both the 7-inch and 10-inch slots (see play-assets/LISTING.md).
// The App Store's 6.9" slot is the only iPhone size that covers every device.
// ---------------------------------------------------------------------------
const TARGETS = [
  { name: 'play-phone',            w: 1080, h: 1920, out: 'android/play-assets/screenshots/phone' },
  { name: 'play-phone-landscape',  w: 1920, h: 1080, out: 'android/play-assets/screenshots/phone-landscape' },
  { name: 'play-tablet',           w: 1600, h: 2560, out: 'android/play-assets/screenshots/tablet' },
  { name: 'play-tablet-landscape', w: 2560, h: 1600, out: 'android/play-assets/screenshots/tablet-landscape' },
  { name: 'ios-6.9',               w: 1290, h: 2796, out: 'ios/app-store-assets/screenshots/iphone-6.9' },
  { name: 'ios-6.9-landscape',     w: 2796, h: 1290, out: 'ios/app-store-assets/screenshots/iphone-6.9-landscape' },
  { name: 'ipad-13',               w: 2064, h: 2752, out: 'ios/app-store-assets/screenshots/ipad-13' },
  { name: 'ipad-13-landscape',     w: 2752, h: 2064, out: 'ios/app-store-assets/screenshots/ipad-13-landscape' },
];

// The board the shots are taken on. Easy (6 pairs) fills a phone nicely and
// still reads at a glance in a store listing; a 21-pair Extra Hard board is a
// wall of tiny cards in a thumbnail.
const SHOT_LEVEL_TAPS = 0;   // Options rows to cycle from the default (Easy)
const MAX_TURNS = 60;        // safety stop, not the normal exit

// Colours from src/render.c, as read back from a screenshot.
const CARD_BACK = [36, 72, 156];
const CARD_BACK2 = [80, 130, 220];   // the plaid drawn on the back
const CARD_FACE = [248, 248, 242];
const CARD_MATCH = [222, 240, 222];

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
               '.data': 'application/octet-stream', '.png': 'image/png' };

function arg(flag, fallback) {
  const i = process.argv.indexOf(flag);
  return i > -1 && process.argv[i + 1] ? process.argv[i + 1] : fallback;
}

function findChrome() {
  const explicit = arg('--chrome', process.env.CHROME);
  if (explicit) return explicit;
  const cache = join(homedir(), '.cache/ms-playwright');
  if (!existsSync(cache)) return null;
  // Highest chromium-<rev> wins; Playwright keeps several revisions side by side.
  const dirs = readdirSync(cache)
    .filter(d => /^chromium-\d+$/.test(d))
    .sort((a, b) => +b.split('-')[1] - +a.split('-')[1]);
  for (const d of dirs) {
    for (const exe of ['chrome-linux64/chrome', 'chrome-linux/chrome', 'chrome-mac/Chromium.app/Contents/MacOS/Chromium']) {
      const p = join(cache, d, exe);
      if (existsSync(p)) return p;
    }
  }
  return null;
}

async function serve(dir) {
  const server = createServer(async (req, res) => {
    const rel = decodeURIComponent(req.url.split('?')[0]).replace(/^\/+/, '') || 'openpairs.html';
    try {
      const body = await readFile(join(dir, rel));
      res.writeHead(200, { 'Content-Type': MIME[extname(rel)] || 'application/octet-stream' });
      res.end(body);
    } catch {
      res.writeHead(404).end('not found');
    }
  });
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  return { server, port: server.address().port };
}

// Store listings reject screenshots with an alpha channel; the browser writes
// RGBA. Re-encode as plain RGB PNG (colour type 2) with zlib, no dependencies.
// The same decoder reads the board back from screenshots while playing.
import { deflateSync, inflateSync } from 'node:zlib';
function crc32(buf) {
  let c, crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    c = (crc ^ buf[n]) & 0xff;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const td = Buffer.concat([Buffer.from(type), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
  return Buffer.concat([len, td, crc]);
}
// Decode an 8-bit RGB or RGBA PNG (as the browser writes) to RGB rows.
function decodePng(png) {
  let off = 8, w = 0, h = 0, ct = 0;
  const idat = [];
  while (off < png.length) {
    const len = png.readUInt32BE(off), type = png.toString('ascii', off + 4, off + 8);
    const data = png.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); ct = data[9]; }
    if (type === 'IDAT') idat.push(data);
    off += 12 + len;
  }
  if (ct !== 2 && ct !== 6) throw new Error(`unexpected PNG colour type ${ct}`);
  const bpp = ct === 6 ? 4 : 3, stride = w * bpp;
  const raw = inflateSync(Buffer.concat(idat));
  const rgb = Buffer.alloc(w * h * 3);
  const prev = Buffer.alloc(stride), cur = Buffer.alloc(stride);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)];
    raw.copy(cur, 0, y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let i = 0; i < stride; i++) {          // undo the PNG row filter
      const a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      let v = cur[i];
      if (f === 1) v += a; else if (f === 2) v += b; else if (f === 3) v += (a + b) >> 1;
      else if (f === 4) { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                          v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      cur[i] = v & 0xff;
    }
    for (let x = 0; x < w; x++) cur.copy(rgb, (y * w + x) * 3, x * bpp, x * bpp + 3);
    cur.copy(prev);
  }
  return { w, h, rgb };
}

function encodeRgbPng({ w, h, rgb }) {
  const out = Buffer.alloc(h * (1 + w * 3));
  for (let y = 0; y < h; y++) {
    out[y * (1 + w * 3)] = 0;
    rgb.copy(out, y * (1 + w * 3) + 1, y * w * 3, (y + 1) * w * 3);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr),
                        chunk('IDAT', deflateSync(out)), chunk('IEND', Buffer.alloc(0))]);
}

async function capture(target, url, chrome) {
  const { w, h, out } = target;
  const browser = await chromium.launch({
    executablePath: chrome,
    // SwiftShader: the runner has no GPU, and raylib needs a real WebGL context.
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  });
  // hasTouch is the whole trick: Chromium's touch emulation makes
  // matchMedia('(pointer: coarse)') match, which is exactly what src/main.c reads
  // to choose the portrait renderer. No page-script shim is needed.
  const ctx = await browser.newContext({
    viewport: { width: w, height: h }, deviceScaleFactor: 1, hasTouch: true,
  });
  const page = await ctx.newPage();
  await page.goto(url, { waitUntil: 'load' });
  await page.waitForTimeout(w * h > 3000000 ? 9000 : 6000);   // WASM boot + font upload

  const wait = ms => page.waitForTimeout(ms);
  const { writeFile } = await import('node:fs/promises');
  const shot = async name => writeFile(join(out, `${name}.png`), encodeRgbPng(decodePng(await page.screenshot())));

  // The input layer samples the pointer once a frame and decides a tap on
  // release. A move+press inside a single frame records the origin at the
  // PREVIOUS position, so the tap reads as a drag and never fires -- every press
  // below therefore settles first. A press also has to be held for more than a
  // couple of frames; 60ms is silently dropped, 120ms is reliable. SwiftShader
  // renders a 4K tablet frame slowly, so the canvas is only read back once the
  // frame after the tap has certainly been drawn.
  // Timings scale with the viewport: SwiftShader draws a 2064x2752 iPad frame
  // several times slower than a phone frame, and a 120ms press that spans two
  // frames on a phone can land inside ONE there -- the recognizer never sees a
  // press and then a release, so the tap is silently dropped and the board
  // never finishes.
  const slow = w * h > 3000000;
  const T = { settle: slow ? 220 : 80, hold: slow ? 500 : 120, after: slow ? 900 : 450 };
  const settle = async (x, y) => { await page.mouse.move(x, y); await wait(T.settle); };
  async function tap(x, y) {
    await settle(x, y);
    await page.mouse.down(); await wait(T.hold); await page.mouse.up(); await wait(T.after);
  }

  // Read pixels back from a screenshot of the page. (Reading the WebGL canvas
  // from page script returned the previous frame under SwiftShader; a screenshot
  // always waits for the current one.) Returns RGB triples at the given points.
  const pixels = async pts => {
    const img = decodePng(await page.screenshot());
    return pts.map(([px, py]) => {
      const i = (Math.round(py) * img.w + Math.round(px)) * 3;
      return [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
    });
  };
  const near = (p, q, tol = 26) => p.every((v, i) => Math.abs(v - q[i]) <= tol);

  // The menu's highlighted row is the only yellow text on screen; its centre is
  // where New Game sits before anything is touched.
  async function selectedMenuRowY() {
    const img = decodePng(await page.screenshot());
    const hits = [];
    for (let y = 0; y < img.h; y += 2) {
      for (let x = Math.round(w * 0.3); x < w * 0.7; x += 4) {
        const i = (y * img.w + x) * 3;
        const r = img.rgb[i], g = img.rgb[i + 1], b = img.rgb[i + 2];
        if (r > 210 && g > 180 && b < 150 && b > 50) { hits.push(y); break; }
      }
    }
    if (!hits.length) throw new Error(`${target.name}: no highlighted menu row`);
    return (Math.min(...hits) + Math.max(...hits)) / 2;
  }

  // Find the card grid by scanning a screenshot for card BACKS, then
  // recovering the pitch from the runs. Reading the geometry back out of the
  // picture keeps this script honest: it never assumes the layout formula, so a
  // change in src/layout.c cannot silently produce screenshots of thin air.
  async function readGrid() {
    const img = decodePng(await page.screenshot());
    // Backs only, and on a freshly dealt board, because the face colour is a
    // near-white within a few counts of the title text: matching on it would
    // read the wordmark as a row of cards.
    const isCard = (x, y) => {
      const i = (Math.round(y) * img.w + Math.round(x)) * 3;
      const p = [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
      // Both back colours, generously: the lattice pattern would otherwise chop
      // one card into several runs, and the antialiased pixels between the two
      // sit outside a tight tolerance. The green felt behind is far enough away
      // (12,92,52) not to be caught by this.
      return near(p, CARD_BACK, 30) || near(p, CARD_BACK2, 30);
    };
    const runs = (fixed, along, horizontal) => {
      const out = [];
      let start = -1;
      for (let v = 0; v < along; v++) {
        const on = horizontal ? isCard(v, fixed) : isCard(fixed, v);
        if (on && start < 0) start = v;
        if ((!on || v === along - 1) && start >= 0) { out.push([start, v - 1]); start = -1; }
      }
      // The white frame on the back is a thin light line crossing each card;
      // merge runs split by a gap that narrow, so one card reads as one run.
      const merged = [];
      for (const r of out) {
        const last = merged[merged.length - 1];
        if (last && r[0] - last[1] <= 8) last[1] = r[1];
        else merged.push([...r]);
      }
      return merged.filter(([a, b]) => b - a > 12);
    };
    // Sweep for a column that actually holds cards before scanning it for rows:
    // the middle of the screen lands in a gap when the grid has an even number
    // of columns.
    let rowSpans = [];
    for (let x = Math.round(w * 0.06); x < w * 0.94 && !rowSpans.length; x += 4) {
      rowSpans = runs(x, h, false);
    }
    if (!rowSpans.length) throw new Error(`${target.name}: no board found`);
    const mid = Math.round((rowSpans[0][0] + rowSpans[0][1]) / 2);
    const colSpans = runs(mid, w, true);
    if (!colSpans.length) throw new Error(`${target.name}: no cards in the first row`);
    const card = colSpans[0][1] - colSpans[0][0] + 1;
    if (process.env.OP_DEBUG)
      console.log(`  grid: ${rowSpans.length} rows ${JSON.stringify(rowSpans.slice(0,6))} / ${colSpans.length} cols ${JSON.stringify(colSpans.slice(0,6))}`);
    return { rowSpans, colSpans, card };
  }

  // Card centres, row-major, matching the deal order the game hit-tests.
  async function cardCentres() {
    const g = await readGrid();
    const pts = [];
    for (const [ry0, ry1] of g.rowSpans) {
      const y = (ry0 + ry1) / 2;
      const img = decodePng(await page.screenshot());
      const isCard = (x) => {
        const i = (Math.round(y) * img.w + Math.round(x)) * 3;
        const p = [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
        return near(p, CARD_BACK, 30) || near(p, CARD_BACK2, 30);
      };
      const spans = [];
      let start = -1;
      for (let x = 0; x < w; x++) {
        const on = isCard(x);
        if (on && start < 0) start = x;
        if ((!on || x === w - 1) && start >= 0) { spans.push([start, x - 1]); start = -1; }
      }
      // Same frame-gap merge as readGrid(), then one centre per card.
      const merged = [];
      for (const r of spans) {
        const last = merged[merged.length - 1];
        if (last && r[0] - last[1] <= 8) last[1] = r[1];
        else merged.push([...r]);
      }
      for (const [a, b] of merged) if (b - a > 12) pts.push([(a + b) / 2, y]);
    }
    return pts;
  }

  // The face colour at a card centre, or null when it is face down.
  async function faceAt(pt) {
    const img = decodePng(await page.screenshot());
    const i = (Math.round(pt[1]) * img.w + Math.round(pt[0])) * 3;
    const p = [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
    if (near(p, CARD_BACK, 30) || near(p, CARD_BACK2, 30)) return null;
    // The shape is drawn in its own colour over the near-white face; a card
    // whose centre reads as the face colour (a ring, say) is told apart by the
    // pixels just off centre, so sample a small cross and keep the darkest.
    let best = p, bestSum = p[0] + p[1] + p[2];
    for (const [dx, dy] of [[-8, 0], [8, 0], [0, -8], [0, 8], [-14, 0], [14, 0]]) {
      const j = ((Math.round(pt[1]) + dy) * img.w + Math.round(pt[0]) + dx) * 3;
      const q = [img.rgb[j], img.rgb[j + 1], img.rgb[j + 2]];
      const sum = q[0] + q[1] + q[2];
      if (sum < bestSum) { best = q; bestSum = sum; }
    }
    return best.map(v => Math.round(v / 12)).join(',');   // quantised: JPEG-free PNG, but be safe
  }

  await mkdir(out, { recursive: true });

  // 1. Title menu, untouched. Sound reads "Off" because it genuinely is off by
  //    default on every platform (src/sound.c) -- not a capture artifact.
  await shot('01-menu');

  for (let i = 0; i < SHOT_LEVEL_TAPS; i++) { /* level cycling hook, unused at Easy */ }

  await tap(w / 2, await selectedMenuRowY());   // New Game
  await wait(1200);

  // 2. The board as it is dealt: every card face down.
  await shot('02-board');

  const centres = await cardCentres();
  if (process.env.OP_DEBUG) console.log(`  ${target.name}: ${centres.length} cards found`);
  if (centres.length < 4) throw new Error(`${target.name}: only ${centres.length} cards found`);

  // Play it honestly: turn cards up, remember what was where, and match a pair
  // as soon as both halves are known. The mid-game shot is taken with a pair
  // face up, the last with the board finished.
  const known = new Map();      // index -> face colour
  const matched = new Set();
  let midDone = false;

  for (let turn = 0; turn < MAX_TURNS && matched.size < centres.length; turn++) {
    // A pair we already know: turn both up, they stay.
    let a = -1, b = -1;
    const byFace = new Map();
    for (const [idx, face] of known) {
      if (matched.has(idx)) continue;
      if (byFace.has(face)) { a = byFace.get(face); b = idx; break; }
      byFace.set(face, idx);
    }
    if (a >= 0) {
      await tap(...centres[a]);
      await tap(...centres[b]);
      matched.add(a); matched.add(b);
      if (!midDone && matched.size >= 4) { await shot('03-play'); midDone = true; }
      continue;
    }
    // Otherwise learn: turn up the two lowest unknown cards.
    const unknown = centres.map((_, i) => i).filter(i => !matched.has(i) && !known.has(i));
    if (unknown.length === 0) break;
    await tap(...centres[unknown[0]]);
    known.set(unknown[0], await faceAt(centres[unknown[0]]));
    if (unknown.length > 1) {
      await tap(...centres[unknown[1]]);
      known.set(unknown[1], await faceAt(centres[unknown[1]]));
      if (known.get(unknown[0]) === known.get(unknown[1])) {
        matched.add(unknown[0]); matched.add(unknown[1]);
        if (!midDone && matched.size >= 4) { await shot('03-play'); midDone = true; }
      } else {
        await wait(slow ? 1800 : 1100);   // let the mismatch turn back
      }
    }
  }

  if (matched.size < centres.length) throw new Error(`${target.name}: board unfinished`);
  await wait(600);
  // 4. The finished board with the "all pairs found" panel.
  await shot('04-complete');

  await browser.close();
  console.log(`${target.name}: 4 frames -> ${out}`);
}

const src = arg('--src');
if (!src) { console.error('--src <web bundle dir> is required'); process.exit(2); }
const root = resolve(arg('--out', process.cwd()));
const chrome = findChrome();
if (!chrome) { console.error('no Chromium found; pass --chrome or set $CHROME'); process.exit(2); }

const { server, port } = await serve(resolve(src));
const url = `http://127.0.0.1:${port}/openpairs.html`;
const only = arg('--only');

// A run can stall if a tap lands during an animation, so a target is worth
// replaying rather than failing the batch.
for (const t of TARGETS) {
  if (only && t.name !== only) continue;
  const target = { ...t, out: join(root, t.out) };
  for (let attempt = 1; ; attempt++) {
    try { await capture(target, url, chrome); break; }
    catch (e) {
      if (attempt === 3) throw e;
      console.log(`${t.name}: attempt ${attempt} failed (${e.message}); retrying`);
    }
  }
}
server.close();
