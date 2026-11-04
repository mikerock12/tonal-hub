#!/usr/bin/env python3
"""Pre-render the Christmas garland, bulbs and star used by Windows and Android.

Everything expensive (twisted wire, sockets, blurred shadows, glass shading) is
baked here, so the apps only change the opacity of small lit-bulb sprites while
music plays. Output is deterministic and shared: resources/seasonal is copied to
android/app/src/main/assets/seasonal, and tests/themes/check_themes.py compares
both trees.

Coordinates are normalised to the album cover: (0,0)-(1,1) is the cover, and the
layers extend MARGIN beyond it on every side so a strand can wrap around an edge.
The BACK layer is drawn under the cover and FRONT over it.

Needs numpy and Pillow. Run: python packaging/generate-christmas-assets.py
"""
import json
import math
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "resources" / "seasonal"
ANDROID = ROOT / "android" / "app" / "src" / "main" / "assets" / "seasonal"

MARGIN = 0.06
SIZE = 1024            # final layer side, including margins
SS = 2                 # supersampling while drawing
SPAN = 1 + 2 * MARGIN
UNIT = SIZE * SS / SPAN  # supersampled pixels per cover width
BULB_LEN = 0.058       # bulb glass length, in cover widths
SPRITE = 192           # lit sprite side
SPRITE_SPAN = 3.5      # sprite side / bulb length
SPACING = 0.056        # arc length between bulbs
COLORS = {
    "red": (255, 52, 58),
    "gold": (255, 190, 64),
    "green": (46, 226, 104),
    "blue": (58, 142, 255),
}
ORDER = ["red", "gold", "green", "blue"]

# Strand runs in cover units; consecutive runs share their apex in the margin,
# which is where the wire changes sides of the cover.
FRONT_RUNS = [
    [(-.046, .118), (.02, .082), (.15, .136), (.29, .186), (.43, .124), (.56, .086),
     (.70, .142), (.84, .188), (.955, .112), (1.046, .062)],
    [(1.046, .318), (.985, .338), (.90, .392), (.838, .462), (.882, .532), (.965, .574), (1.046, .588)],
    [(-.046, .372), (.016, .388), (.10, .436), (.165, .532), (.152, .642), (.082, .722), (.012, .762), (-.046, .778)],
    [(.128, 1.046), (.158, .986), (.238, .926), (.358, .906), (.468, .944), (.528, .994), (.558, 1.046)],
]
BACK_RUNS = [
    [(.05, .05), (-.02, .082), (-.046, .118)],
    [(1.046, .062), (1.02, .118), (.965, .17), (.96, .25), (1.015, .286), (1.046, .318)],
    [(1.046, .588), (1.0, .63), (.9, .64), (.5, .6), (.1, .44), (-.012, .386), (-.046, .372)],
    [(-.046, .778), (-.012, .83), (.04, .92), (.09, .99), (.128, 1.046)],
    [(.558, 1.046), (.585, 1.0), (.62, .955)],
]


def catmull(points, step, before=None, after=None):
    """before/after are the neighbouring run's points, so the wire stays smooth at an apex."""
    pts = [np.array(p, float) for p in points]
    first = np.array(before, float) if before is not None else 2 * pts[0] - pts[1]
    last = np.array(after, float) if after is not None else 2 * pts[-1] - pts[-2]
    pts = [first] + pts + [last]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1:i + 3]
        n = max(2, int(np.linalg.norm(p2 - p1) / step))
        for k in range(n):
            t = k / n
            out.append(.5 * ((2 * p1) + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                             + (3 * p1 - p0 - 3 * p2 + p3) * t ** 3))
    out.append(pts[-2])
    return np.array(out)


def to_px(p):
    return (np.asarray(p) + MARGIN) * UNIT


def arclen(path):
    return np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(path, axis=0), axis=1))])


def sample_at(path, lengths, s):
    i = min(len(path) - 2, int(np.searchsorted(lengths, s)))
    tangent = path[i + 1] - path[max(0, i - 1)]
    tangent /= max(1e-9, np.linalg.norm(tangent))
    return path[i], tangent


def draw_wire(layer, path):
    """Two twisted strands, shaded so the twist reads even at phone size."""
    px = to_px(path)
    lengths = arclen(px)
    total = lengths[-1]
    width = 1.55 * SS
    radius = 1.25 * SS
    twist = 2 * math.pi / (15 * SS)
    step = max(1, SS // 2)
    draw = ImageDraw.Draw(layer)
    samples = np.arange(0, total, step)
    idx = np.clip(np.searchsorted(lengths, samples), 1, len(px) - 1)
    frac = (samples - lengths[idx - 1]) / np.maximum(1e-6, lengths[idx] - lengths[idx - 1])
    pos = px[idx - 1] + (px[idx] - px[idx - 1]) * frac[:, None]
    tang = np.gradient(pos, axis=0)
    tang /= np.maximum(1e-6, np.linalg.norm(tang, axis=1))[:, None]
    normal = np.stack([-tang[:, 1], tang[:, 0]], axis=1)
    for strand in (1, 0):
        phase = samples * twist + strand * math.pi
        offset = np.sin(phase)
        depth = np.cos(phase)
        for j in range(len(samples)):
            # Draw the far half of each strand first; the near half overlaps it.
            if (depth[j] > 0) != bool(strand):
                continue
            x, y = pos[j] + normal[j] * offset[j] * radius
            light = .28 + .72 * (.5 + .5 * depth[j])
            color = (int(6 + 16 * light), int(22 + 44 * light), int(14 + 22 * light), 255)
            draw.ellipse((x - width, y - width, x + width, y + width), fill=color)
    for strand in (0, 1):
        phase = samples * twist + strand * math.pi
        offset = np.sin(phase)
        depth = np.cos(phase)
        for j in range(0, len(samples), 2):
            if depth[j] < .35:
                continue
            x, y = pos[j] + normal[j] * offset[j] * radius - np.array([.5, .7]) * SS
            r = width * .34
            shine = int(70 * depth[j])
            draw.ellipse((x - r, y - r, x + r, y + r), fill=(120, 190, 136, shine))


def glass(size, supersample=4):
    """Teardrop bulb masks: returns (mask, edge distance 0..1) on a square grid."""
    n = size * supersample
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    x = (x + .5) / n - .5
    y = (y + .5) / n - .5
    length = 1 / SPRITE_SPAN
    v = (y + length / 2) / length  # 0 tip, 1 base
    # Cone with a soft tip, widest near the shoulder, necking into the socket: a C9 lamp.
    vv = np.clip(v, 1e-4, 1 - 1e-4)
    half = .262 * length * np.sin(np.pi * vv ** 1.42) ** .86
    half = np.where((v > 0) & (v < 1), half + .05 * length * np.clip((v - .86) / .14, 0, 1), 0)
    inside = np.abs(x) < half
    edge = np.clip(1 - np.abs(x) / np.maximum(half, 1e-6), 0, 1) * inside
    return inside.astype(np.float32), edge, x, y, v, length


def downsample(a, supersample=4):
    n = a.shape[0] // supersample
    return a.reshape(n, supersample, n, supersample, -1).mean(axis=(1, 3))


def bulb_sprite(color, lit):
    inside, edge, x, y, v, length = glass(SPRITE)
    c = np.array(color, np.float32) / 255
    r2 = (x / (length * .33)) ** 2 + ((y - length * .06) / (length * .5)) ** 2
    if lit:
        core = np.exp(-r2 * 1.5)
        hot = np.exp(-r2 * 7)
        body = c[None, None] * (.72 + .55 * core[..., None])
        rgb = body * (1 - hot[..., None] * .85) + np.array([1, .97, .86]) * hot[..., None] * .85
        rim = np.clip(1 - edge / .16, 0, 1)
        rgb = rgb * (1 - rim[..., None] * .22) + c * rim[..., None] * .22
    else:
        depth = .36 + .44 * edge ** .5
        rgb = c[None, None] * depth[..., None] * .64 + .04
    spec = np.exp(-(((x + length * .075) / (length * .028)) ** 2 + ((y + length * .1) / (length * .12)) ** 2))
    spec2 = np.exp(-(((x - length * .08) / (length * .018)) ** 2 + ((y - length * .06) / (length * .09)) ** 2))
    rgb = rgb + (spec * (.75 if lit else .62) + spec2 * .24)[..., None]
    rgb = np.clip(rgb, 0, 1)
    alpha = inside
    if lit:
        r = np.sqrt(x * x + (y * .92) ** 2)
        halo = .60 * np.exp(-(r / (length * .50)) ** 2) + .24 * np.exp(-(r / (length * 1.05)) ** 2)
        fade = np.clip((.5 - r) / .1, 0, 1)
        halo = halo * fade
        out_rgb = rgb * inside[..., None] + (c * .92 + .08)[None, None] * (1 - inside[..., None])
        alpha = inside + (1 - inside) * halo
        return premultiplied_down(out_rgb, alpha)
    return premultiplied_down(rgb, alpha)


def premultiplied_down(rgb, alpha):
    pm = np.dstack([rgb * alpha[..., None], alpha])
    small = downsample(pm)
    a = np.maximum(small[..., 3:], 1e-4)
    return Image.fromarray(np.clip(np.dstack([small[..., :3] / a, small[..., 3:]]) * 255, 0, 255).astype(np.uint8), "RGBA")


def socket_sprite():
    w, h = 64, 96
    n = 4
    y, x = np.mgrid[0:h * n, 0:w * n].astype(np.float32)
    x = (x + .5) / (w * n) - .5
    y = (y + .5) / (h * n)
    half = .30 - .08 * y
    inside = (np.abs(x) < half) & (y > .04) & (y < .98)
    shade = .34 + .66 * np.clip(1 - ((x + .1) / half) ** 2, 0, 1)
    ribs = .9 + .1 * np.sin(y * 34)
    base = np.array([.07, .26, .15])
    rgb = base[None, None] * (shade * ribs)[..., None] * 1.9
    a = inside.astype(np.float32)
    pm = np.dstack([rgb * a[..., None], a]).reshape(h, n, w, n, 4).mean(axis=(1, 3))
    alpha = np.maximum(pm[..., 3:], 1e-4)
    return Image.fromarray(np.clip(np.dstack([pm[..., :3] / alpha, pm[..., 3:]]) * 255, 0, 255).astype(np.uint8), "RGBA")


def paste_rotated(layer, sprite, center, angle, length_px):
    """angle 0 means the bulb tip points up; center is the glass centre."""
    scale = length_px * SPRITE_SPAN / sprite.width
    side = int(round(sprite.width * scale))
    resized = sprite.resize((side, side), Image.LANCZOS).rotate(-angle, Image.BICUBIC, expand=True)
    layer.alpha_composite(resized, (int(center[0] - resized.width / 2), int(center[1] - resized.height / 2)))


def paste_socket(layer, socket, center, angle, length_px):
    """Socket sits between the wire and the base of the glass."""
    h = length_px * .50
    w = h * socket.width / socket.height * 1.1
    resized = socket.resize((max(1, int(w)), max(1, int(h))), Image.LANCZOS).rotate(-angle, Image.BICUBIC, expand=True)
    layer.alpha_composite(resized, (int(center[0] - resized.width / 2), int(center[1] - resized.height / 2)))


def place_bulbs(path, first, spacing, layer):
    lengths = arclen(path)
    bulbs = []
    s = spacing * .5
    index = first
    while s < lengths[-1] - spacing * .3:
        point, tangent = sample_at(path, lengths, s)
        normal = np.array([-tangent[1], tangent[0]])
        if normal[1] < 0:
            normal = -normal  # always the gravity side
        sign = 1 if index % 3 != 2 else -1  # two hang, one stands, like real strings
        direction = normal * sign
        direction = direction / np.linalg.norm(direction)
        wobble = math.sin(index * 2.4) * 11
        angle = math.degrees(math.atan2(direction[0], -direction[1])) + wobble
        tilt = math.radians(angle)
        tip = np.array([math.sin(tilt), -math.cos(tilt)])
        centre = point + tip * (BULB_LEN * .78)
        socket = point + tip * (BULB_LEN * .16)
        bulbs.append({"x": round(float(centre[0]), 4), "y": round(float(centre[1]), 4),
                      "angle": round(angle, 1), "color": ORDER[index % 4], "layer": layer,
                      "sx": float(socket[0]), "sy": float(socket[1])})
        index += 1
        s += spacing * (1 + .1 * math.sin(index * 1.7))
    return bulbs, index


def inside_cover(p):
    return 0 <= p[0] <= 1 and 0 <= p[1] <= 1


def shadow_of(drawn, dx, dy, blur, opacity):
    alpha = drawn.getchannel("A").point(lambda v: int(v * opacity))
    shadow = Image.new("RGBA", drawn.size, (0, 0, 0, 0))
    shadow.putalpha(alpha)
    shifted = Image.new("RGBA", drawn.size, (0, 0, 0, 0))
    shifted.paste(shadow, (int(dx), int(dy)))
    return shifted.filter(ImageFilter.GaussianBlur(blur))


def cover_clip(size):
    mask = Image.new("L", (size, size), 0)
    lo = MARGIN / SPAN * size
    hi = (1 + MARGIN) / SPAN * size
    ImageDraw.Draw(mask).rectangle((lo, lo, hi, hi), fill=255)
    return mask


def clipped(image, mask, inverse=False):
    alpha = np.asarray(image.getchannel("A"), np.float32) * (np.asarray(mask, np.float32) / 255)
    if inverse:
        alpha = np.asarray(image.getchannel("A"), np.float32) * (1 - np.asarray(mask, np.float32) / 255)
    out = image.copy()
    out.putalpha(Image.fromarray(alpha.astype(np.uint8)))
    return out


def build_layers(off, socket):
    side = SIZE * SS
    step = .002
    # Runs alternate back, front, back... sharing an apex; each borrows its neighbour's
    # next point so both halves leave the apex along the same tangent.
    chain = [BACK_RUNS[0]]
    for front_run, back_run in zip(FRONT_RUNS, BACK_RUNS[1:]):
        chain += [front_run, back_run]
    paths = [catmull(run, step, chain[i - 1][-2] if i else None, chain[i + 1][1] if i + 1 < len(chain) else None)
             for i, run in enumerate(chain)]
    back_paths, front_paths = paths[0::2], paths[1::2]
    bulbs = []
    index = 0
    for path in front_paths:
        placed, index = place_bulbs(path, index, SPACING, "front")
        bulbs += placed
    for path in back_paths:
        placed, index = place_bulbs(path, index, SPACING * 1.05, "back")
        # A back bulb is only useful where it can be seen around the cover edge.
        bulbs += [b for b in placed if not (.035 < b["x"] < .965 and .035 < b["y"] < .965)]

    length_px = BULB_LEN * UNIT
    clip = cover_clip(side)

    def strand_layer(paths, layer_name):
        wire = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        for path in paths:
            draw_wire(wire, path)
        lamps = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        for bulb in bulbs:
            if bulb["layer"] != layer_name:
                continue
            paste_rotated(lamps, off[bulb["color"]], to_px((bulb["x"], bulb["y"])), bulb["angle"], length_px)
            paste_socket(lamps, socket, to_px((bulb["sx"], bulb["sy"])), bulb["angle"], length_px)
        return wire, lamps

    fwire, flamps = strand_layer(front_paths, "front")
    bwire, blamps = strand_layer(back_paths, "back")

    # Light comes from the upper left. Things floating over the cover throw a tight
    # shadow on it; the card sits further back, so the same object there is softer.
    u = SS
    front = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    both = fwire.copy()
    both.alpha_composite(flamps)
    on_cover = shadow_of(fwire, 5 * u, 7 * u, 2.2 * u, .62)
    on_cover.alpha_composite(shadow_of(flamps, 9 * u, 13 * u, 5.5 * u, .56))
    on_card = shadow_of(both, 11 * u, 16 * u, 7 * u, .42)
    front.alpha_composite(clipped(on_cover, clip))
    front.alpha_composite(clipped(on_card, clip, inverse=True))
    front.alpha_composite(fwire)
    front.alpha_composite(flamps)

    back = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    bboth = bwire.copy()
    bboth.alpha_composite(blamps)
    back.alpha_composite(shadow_of(bboth, 9 * u, 13 * u, 6 * u, .5))
    back.alpha_composite(bwire)
    back.alpha_composite(blamps)
    # The cover itself shades whatever passes beneath its edge.
    occlusion = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    block = Image.new("L", (side, side), 0)
    lo = MARGIN / SPAN * side
    hi = (1 + MARGIN) / SPAN * side
    ImageDraw.Draw(block).rectangle((lo + 8 * u, lo + 10 * u, hi + 8 * u, hi + 10 * u), fill=255)
    block = block.filter(ImageFilter.GaussianBlur(9 * u)).point(lambda v: int(v * .58))
    occlusion.putalpha(block)
    back.alpha_composite(occlusion)

    return front.resize((SIZE, SIZE), Image.LANCZOS), back.resize((SIZE, SIZE), Image.LANCZOS), bulbs


def lit_preview(front, bulbs, on):
    """One static image for ordinary covers: no clock, no per-frame work."""
    layer = front.copy()
    unit = SIZE / SPAN
    for i, bulb in enumerate(bulbs):
        if bulb["layer"] != "front":
            continue
        sprite = on[bulb["color"]]
        scale = BULB_LEN * unit * SPRITE_SPAN / sprite.width
        side = int(round(sprite.width * scale))
        resized = sprite.resize((side, side), Image.LANCZOS).rotate(-bulb["angle"], Image.BICUBIC, expand=True)
        cx = (bulb["x"] + MARGIN) * unit
        cy = (bulb["y"] + MARGIN) * unit
        layer.alpha_composite(resized, (int(cx - resized.width / 2), int(cy - resized.height / 2)))
    lo = int(MARGIN / SPAN * SIZE)
    hi = int((1 + MARGIN) / SPAN * SIZE)
    return layer.crop((lo, lo, hi, hi)).resize((512, 512), Image.LANCZOS)


def wire_tile():
    """Seamless horizontal tile of the seek bar wire."""
    period = 96
    h = 32
    n = 4
    canvas = Image.new("RGBA", (period * 3 * n, h * n), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    width = 2.3 * n
    radius = 3.1 * n
    for pass_near in (False, True):
        for strand in (1, 0):
            for x in np.arange(0, period * 3 * n, 1.0):
                phase = x / (24 * n) * 2 * math.pi + strand * math.pi
                offset = math.sin(phase)
                depth = math.cos(phase)
                if (depth > 0) != pass_near:
                    continue
                y = h * n / 2 + offset * radius
                light = .28 + .72 * (.5 + .5 * depth)
                color = (int(16 + 44 * light), int(50 + 104 * light), int(30 + 52 * light), 255)
                draw.ellipse((x - width, y - width, x + width, y + width), fill=color)
                if depth > .55:
                    r = width * .3
                    draw.ellipse((x - r, y - width * .55 - r, x + r, y - width * .55 + r), fill=(190, 236, 188, 150))
    return canvas.crop((period * n, 0, period * 2 * n, h * n)).resize((period, h), Image.LANCZOS)


def star_image(size=256):
    n = size * 3
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    x = (x + .5) / n - .5
    y = (y + .5) / n - .5
    angle = np.arctan2(x, -y)
    r = np.sqrt(x * x + y * y)
    sector = (angle % (2 * np.pi / 5)) - np.pi / 5
    outer, inner = .30, .128
    # Distance from each ray's edge, used both as coverage and bevel height.
    cos_limit = np.cos(np.pi / 5)
    tip = np.array([0, outer])
    valley = np.array([inner * math.sin(math.pi / 5), inner * cos_limit])
    px = r * np.sin(np.abs(sector))
    py = r * np.cos(sector)
    ex, ey = valley - tip
    nx, ny = -ey, ex
    norm = math.hypot(nx, ny)
    if nx * (0 - tip[0]) + ny * (0 - tip[1]) < 0:
        nx, ny = -nx, -ny
    dist = ((px - tip[0]) * nx + (py - tip[1]) * ny) / norm
    inside = dist > 0
    height = np.clip(dist / .09, 0, 1)
    side = np.sign(sector)
    # Facets: each ray has one lit and one shaded half, turned towards the light.
    facet_dir = angle - side * .55
    light = .55 + .45 * np.cos(facet_dir + .9)
    gold_hi = np.array([1, .95, .62])
    gold_lo = np.array([.86, .52, .10])
    rgb = gold_lo[None, None] * (1 - light[..., None]) + gold_hi[None, None] * light[..., None]
    center = np.exp(-(r / .065) ** 2)
    rgb = rgb * (1 - center[..., None]) + np.array([1, 1, .93]) * center[..., None]
    rgb = rgb + (np.clip(1 - dist / .012, 0, 1) * inside * .42)[..., None]
    rng = np.random.default_rng(1225)
    sparks = np.zeros((n, n), np.float32)
    for _ in range(170):
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(0, outer)
        cx, cy = int((math.sin(a) * d + .5) * n), int((.5 - math.cos(a) * d) * n)
        s = rng.integers(1, 3)
        sparks[max(0, cy - s):cy + s, max(0, cx - s):cx + s] = rng.uniform(.5, 1)
    sparks = sparks * inside
    rgb = np.clip(rgb + sparks[..., None] * .8, 0, 1)
    alpha = inside.astype(np.float32)
    pm = np.dstack([rgb * alpha[..., None], alpha]).reshape(size, 3, size, 3, 4).mean(axis=(1, 3))
    a = np.maximum(pm[..., 3:], 1e-4)
    return Image.fromarray(np.clip(np.dstack([pm[..., :3] / a, pm[..., 3:]]) * 255, 0, 255).astype(np.uint8), "RGBA")


def star_glow(size=256):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    x = (x + .5) / size - .5
    y = (y + .5) / size - .5
    r = np.sqrt(x * x + y * y)
    a = np.arctan2(y, x)
    halo = .86 * np.exp(-(r / .14) ** 2) + .42 * np.exp(-(r / .26) ** 2)
    rays = np.zeros_like(r)
    for k, reach, thick in ((0, .48, .012), (1, .36, .010), (2, .48, .012), (3, .36, .010)):
        for sign in (0, math.pi):
            angle = a - (k * math.pi / 4 + sign)
            angle = (angle + math.pi) % (2 * math.pi) - math.pi
            along = r * np.cos(angle)
            across = r * np.sin(angle)
            rays = np.maximum(rays, np.exp(-(across / thick) ** 2) * np.clip(1 - along / reach, 0, 1) ** 1.5 * (along > 0))
    alpha = np.clip(halo + rays * .95, 0, 1) * np.clip((.5 - r) / .06, 0, 1)
    rgb = np.array([1, .86, .50])[None, None] * (1 - rays[..., None] * .6) + np.array([1, 1, .92])[None, None] * rays[..., None] * .6
    return Image.fromarray(np.clip(np.dstack([rgb, alpha]) * 255, 0, 255).astype(np.uint8), "RGBA")


def save_webp(image, path, quality=86, lossless=False):
    image.save(path, "WEBP", quality=quality, alpha_quality=95, method=6, exact=False, lossless=lossless)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    on = {name: bulb_sprite(rgb, True) for name, rgb in COLORS.items()}
    off = {name: bulb_sprite(rgb, False) for name, rgb in COLORS.items()}
    socket = socket_sprite()
    front, back, bulbs = build_layers(off, socket)
    save_webp(front, OUT / "garland_front.webp")
    save_webp(back, OUT / "garland_back.webp")
    save_webp(lit_preview(front, bulbs, on), OUT / "garland_lit.webp")
    for name, sprite in on.items():
        save_webp(sprite, OUT / f"bulb_{name}.webp", 90)
        save_webp(off[name], OUT / f"bulb_{name}_off.webp", 90)
    save_webp(socket, OUT / "socket.webp", 90)
    save_webp(wire_tile(), OUT / "wire.webp", lossless=True)
    save_webp(star_image(), OUT / "star.webp", 92)
    save_webp(star_glow(), OUT / "star_glow.webp", 92)
    meta = {
        "margin": MARGIN,
        "bulbLength": BULB_LEN,
        "spriteSpan": SPRITE_SPAN,
        "bulbs": [{k: b[k] for k in ("x", "y", "angle", "color", "layer")} for b in bulbs],
    }
    (OUT / "garland.json").write_text(json.dumps(meta, separators=(",", ":")) + "\n", encoding="utf-8")
    data = ROOT / "qml" / "companion" / "ChristmasGarlandData.js"
    bulbs_js = json.dumps(meta["bulbs"], separators=(",", ":"))
    data.write_text(".pragma library\n// Generated by packaging/generate-christmas-assets.py; do not edit.\n"
                    f"var margin={MARGIN};\nvar bulbLength={BULB_LEN};\nvar spriteSpan={SPRITE_SPAN};\nvar bulbs={bulbs_js};\n",
                    encoding="utf-8")
    ANDROID.mkdir(parents=True, exist_ok=True)
    for file in OUT.iterdir():
        shutil.copy2(file, ANDROID / file.name)
    print(f"{len(bulbs)} bulbs ({sum(b['layer'] == 'front' for b in bulbs)} front)")


if __name__ == "__main__":
    main()
