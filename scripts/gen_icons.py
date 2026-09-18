#!/usr/bin/env python3
"""Generate every launcher / store icon from one vector description.

The icons are the game itself in miniature: two cards, one face down showing
the crosshatched back and one turned up on a red circle, drawn the way
render.c draws them, on the green felt. Keeping them generated rather than
hand-drawn means the palette can never drift from src/render.c, and every size
is produced from the same geometry.

Outputs (run from the repo root, needs Pillow):
    android/res/mipmap-*/ic_launcher.png            legacy square launcher icon
    android/res/mipmap-*/ic_launcher_foreground.png adaptive-icon foreground
    android/play-assets/icon-512.png                Play store listing icon
    android/play-assets/feature-graphic-1024x500.png Play store feature graphic
    ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png  iOS app icon
    ios/app-store-assets/icon-1024.png              App Store listing icon

    scripts/gen_icons.py
"""
import os

from PIL import Image, ImageDraw

# Palette, copied from the constants at the top of src/render.c.
TABLE = (12, 92, 52, 255)          # FELT: openklondike's green table
TABLE_DARK = (10, 76, 44, 255)     # FELT_DARK
CARD_BACK = (36, 72, 156, 255)
CARD_BACK2 = (80, 130, 220, 255)
CARD_FACE = (248, 248, 242, 255)
CARD_EDGE = (40, 40, 40, 255)
FACE_RED = (214, 64, 64, 255)      # FACE_COLOR[0], the circle face

SS = 4  # supersample factor; every shape is drawn large and downscaled


def circle(d, cx, cy, r, **kw):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], **kw)


def card(w, h, face_up):
    """One card, as an RGBA image: the crosshatched back, or the red circle face."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = int(w * 0.12)   # CARD_ROUND
    edge = max(1, int(w * 0.035))
    body = CARD_FACE if face_up else CARD_BACK
    d.rounded_rectangle([0, 0, w - 1, h - 1], r, fill=body, outline=CARD_EDGE, width=edge)
    if face_up:
        circle(d, w / 2, h / 2, w * 0.30, fill=FACE_RED)
    else:
        # The card back from render.c: a white inset frame with a diagonal
        # crosshatch inside it.
        m = max(3, int(h * 0.09))
        hatch = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        hd = ImageDraw.Draw(hatch)
        step = max(4, int(h * 0.10))
        lw = max(1, int(round(h * 0.013)))
        for k in range(-h, 2 * w, step):
            hd.line([k, 0, k + h, h], fill=CARD_BACK2, width=lw)
            hd.line([k, h, k + h, 0], fill=CARD_BACK2, width=lw)
        clip = Image.new("L", (w, h), 0)
        ImageDraw.Draw(clip).rectangle([m + 2, m + 2, w - m - 2, h - m - 2], fill=255)
        img.paste(hatch, (0, 0), Image.composite(hatch.split()[3], Image.new("L", (w, h), 0), clip))
        d.rounded_rectangle([m, m, w - m, h - m], int(r * 0.6), outline=CARD_FACE,
                            width=max(2, int(w * 0.012)))
    return img


def compose(size, background, content_scale):
    """The icon at `size` px. `background` is None for the adaptive foreground.

    `content_scale` is the fraction of the canvas the card pair spans, so the
    adaptive foreground can stay inside its 66% safe zone while the legacy
    square icon fills more of its tile.
    """
    n = size * SS
    img = Image.new("RGBA", (n, n), background if background else (0, 0, 0, 0))

    span = int(n * content_scale)
    cw = int(span * 0.62)
    ch = cw                                   # cards are square, as in render.c
    cx, cy = n // 2, n // 2

    # The face-down card sits behind and to the left, tilted the other way, so
    # the pair reads as two cards rather than one at every size.
    back = card(cw, ch, False).rotate(12, resample=Image.BICUBIC, expand=True)
    img.alpha_composite(back, (cx - back.width // 2 - int(cw * 0.28),
                               cy - back.height // 2 - int(ch * 0.06)))
    face = card(cw, ch, True).rotate(-7, resample=Image.BICUBIC, expand=True)
    img.alpha_composite(face, (cx - face.width // 2 + int(cw * 0.26),
                               cy - face.height // 2 + int(ch * 0.08)))
    return img.resize((size, size), Image.LANCZOS)


def feature_graphic(w, h):
    """Play's 1024x500 feature graphic: the icon art on a table gradient."""
    img = Image.new("RGBA", (w * 2, h * 2), TABLE)
    d = ImageDraw.Draw(img)
    for y in range(h * 2):  # subtle vertical shade, dark at the bottom
        t = y / (h * 2)
        d.line([0, y, w * 2, y],
               fill=tuple(int(TABLE[i] + (TABLE_DARK[i] - TABLE[i]) * t) for i in range(3)))
    art = compose(h * 2, None, 0.66)
    img.alpha_composite(art, ((w * 2 - art.width) // 2, 0))
    return img.resize((w, h), Image.LANCZOS)


def save(img, path, opaque=False):
    """Write `img`, flattening away the alpha channel when `opaque` is set.

    Apple rejects an app icon that has an alpha channel outright -- it masks the
    corners itself -- so the iOS icons must be flat RGB. The Android adaptive
    foreground is the opposite case and must keep its transparency.
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if opaque:
        flat = Image.new("RGB", img.size, TABLE[:3])
        flat.paste(img, mask=img.split()[3])
        img = flat
    img.save(path)
    print("gen_icons: wrote %s (%dx%d %s)" % (path, img.width, img.height, img.mode))


def main():
    # Legacy square launcher icon and the adaptive foreground, per density. The
    # adaptive foreground canvas is 108dp to the legacy 48dp, and its content
    # must stay inside the central 72dp, hence the smaller content scale.
    for suffix, legacy in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96),
                           ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = "android/res/mipmap-%s" % suffix
        save(compose(legacy, TABLE, 0.82), "%s/ic_launcher.png" % d)
        save(compose(legacy * 108 // 48, None, 0.55),
             "%s/ic_launcher_foreground.png" % d)

    save(compose(512, TABLE, 0.76), "android/play-assets/icon-512.png")
    save(feature_graphic(1024, 500), "android/play-assets/feature-graphic-1024x500.png",
         opaque=True)

    ios_icon = compose(1024, TABLE, 0.76)
    save(ios_icon, "ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png", opaque=True)
    save(ios_icon, "ios/app-store-assets/icon-1024.png", opaque=True)


if __name__ == "__main__":
    main()
