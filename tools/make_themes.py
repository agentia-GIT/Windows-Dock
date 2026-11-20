#!/usr/bin/env python3
"""Génère les thèmes de RocketDock Modern : Themes/<Nom>/theme.ini + Themes/<Nom>/Icons/*.png

Usage :  python3 tools/make_themes.py [dossier_de_sortie]      (nécessite Pillow et numpy)
Chaque thème définit les couleurs du dock, de la fenêtre Paramètres et un jeu de 19 icônes dessinées.
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = sys.argv[1] if len(sys.argv) > 1 else "Themes"
W = 512            # taille de travail d'une tuile (réduite à 256 ensuite)
BOX = (36, 36, 476, 476)
KEYS = ["computer", "folder", "documents", "downloads", "pictures", "music", "video", "recycle", "recycle_full",
        "settings", "terminal", "browser", "mail", "network", "code", "games", "ai", "chat", "app"]


# ----------------------------------------------------------------------------------------------
# Glyphes : dessinés dans un repère 0..100, 255 = matière, 0 = découpe
# ----------------------------------------------------------------------------------------------
def glyph_mask(key, size=1024):
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    k = size / 100.0

    def R(x0, y0, x1, y1, r=0, f=255):
        if r:
            d.rounded_rectangle((x0 * k, y0 * k, x1 * k, y1 * k), radius=r * k, fill=f)
        else:
            d.rectangle((x0 * k, y0 * k, x1 * k, y1 * k), fill=f)

    def E(x0, y0, x1, y1, f=255):
        d.ellipse((x0 * k, y0 * k, x1 * k, y1 * k), fill=f)

    def P(pts, f=255):
        d.polygon([(x * k, y * k) for x, y in pts], fill=f)

    def L(pts, w, f=255):
        d.line([(x * k, y * k) for x, y in pts], fill=f, width=int(w * k), joint="curve")
        for x, y in (pts[0], pts[-1]):
            E(x - w / 2, y - w / 2, x + w / 2, y + w / 2, f)

    if key == "computer":
        R(10, 18, 90, 68, 6); R(17, 25, 83, 61, 2, 0)
        L([(25, 46), (38, 46), (44, 36), (53, 56), (59, 46), (75, 46)], 3.4)
        R(43, 68, 57, 78); R(28, 78, 72, 85, 3)
    elif key == "folder":
        P([(8, 24), (38, 24), (46, 33), (92, 33), (92, 82), (8, 82)])
        R(8, 44, 92, 46.5, 0, 0)
        R(8, 44, 92, 82, 4)
    elif key == "documents":
        P([(22, 8), (60, 8), (80, 28), (80, 92), (22, 92)])
        P([(60, 8), (60, 28), (80, 28)], 0)
        P([(63, 14), (63, 25), (74, 25)])
        for y in (46, 59, 72):
            R(32, y, 70, y + 4.5, 2, 0)
    elif key == "downloads":
        R(41, 10, 59, 50); P([(24, 48), (76, 48), (50, 76)])
        R(14, 82, 86, 90, 4)
    elif key == "pictures":
        R(8, 16, 92, 84, 6); R(15, 23, 85, 77, 2, 0)
        P([(20, 74), (42, 44), (56, 62), (66, 51), (82, 74)]); E(62, 28, 76, 42)
    elif key == "music":
        R(40, 20, 46.5, 72); R(74, 14, 80.5, 66)
        P([(40, 20), (80.5, 12), (80.5, 26), (40, 34)])
        E(22, 62, 46, 82); E(56, 56, 80, 76)
    elif key == "video":
        R(8, 22, 92, 78, 8); P([(41, 36), (41, 64), (65, 50)], 0)
    elif key in ("recycle", "recycle_full"):
        if key == "recycle_full":
            P([(30, 24), (42, 4), (54, 24)]); P([(48, 24), (62, 8), (74, 24)])
        R(18, 24, 82, 32, 3); R(38, 14, 62, 24, 2)
        P([(24, 36), (76, 36), (70, 90), (30, 90)])
        for x in (39, 55):
            R(x, 44, x + 6, 82, 2, 0)
    elif key == "settings":
        pts = []
        n = 8
        for i in range(n):
            a0 = 2 * math.pi * i / n
            for da, r in ((-0.20, 30), (-0.13, 44), (0.13, 44), (0.20, 30)):
                a = a0 + da * 2
                pts.append((50 + r * math.cos(a), 50 + r * math.sin(a)))
        P(pts); E(14, 14, 86, 86)
        E(36, 36, 64, 64, 0)
    elif key == "terminal":
        R(7, 16, 93, 84, 7); R(13, 30, 87, 78, 2, 0)
        E(15, 21.5, 20, 26.5, 0); E(24, 21.5, 29, 26.5, 0)
        L([(24, 42), (37, 53), (24, 64)], 5.5); R(43, 62, 64, 67.5, 2)
    elif key == "browser":
        E(8, 8, 92, 92); E(14, 14, 86, 86, 0)
        E(30, 14, 70, 86); E(35, 14, 65, 86, 0)
        R(14, 48, 86, 52.5); R(22, 30, 78, 34); R(22, 66, 78, 70)
    elif key == "mail":
        R(8, 20, 92, 80, 6); L([(14, 28), (50, 56), (86, 28)], 5, 0)
    elif key == "network":
        L([(50, 22), (22, 74)], 4.5); L([(50, 22), (78, 74)], 4.5); L([(22, 74), (78, 74)], 4.5)
        L([(50, 22), (50, 56)], 4.5)
        E(37, 9, 63, 35); E(9, 61, 35, 87); E(65, 61, 91, 87); E(41, 47, 59, 65)
    elif key == "code":
        L([(34, 28), (12, 50), (34, 72)], 7); L([(66, 28), (88, 50), (66, 72)], 7); L([(57, 22), (43, 78)], 7)
    elif key == "games":
        E(4, 36, 40, 86); E(60, 36, 96, 86); R(16, 30, 84, 74, 12)
        R(22, 47.5, 40, 53.5, 0, 0); R(28, 41.5, 34, 59.5, 0, 0)
        E(61, 42, 70, 51, 0); E(72, 52, 81, 61, 0)
    elif key == "ai":
        R(24, 24, 76, 76, 7); R(33, 33, 67, 67, 3, 0)
        for t in (34, 46, 58):
            R(12, t, 24, t + 4); R(76, t, 88, t + 4); R(t, 12, t + 4, 24); R(t, 76, t + 4, 88)
        P([(50, 36), (54, 46), (64, 50), (54, 54), (50, 64), (46, 54), (36, 50), (46, 46)])
    elif key == "chat":
        R(8, 14, 92, 70, 12); P([(26, 66), (26, 90), (50, 68)])
        for x in (30, 50, 70):
            E(x - 5.5, 36.5, x + 5.5, 47.5, 0)
    else:  # app
        for x, y in ((15, 15), (55, 15), (15, 55), (55, 55)):
            R(x, y, x + 30, y + 30, 7)
    return m


# ----------------------------------------------------------------------------------------------
# Outils de dessin
# ----------------------------------------------------------------------------------------------
def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def grad_image(c1, c2, size=W, angle="diag"):
    t = np.zeros((size, size), dtype=np.float32)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32) / (size - 1)
    if angle == "diag":
        t = (xx + yy) / 2
    elif angle == "vert":
        t = yy
    elif angle == "radial":
        t = np.clip(np.sqrt((xx - 0.5) ** 2 + (yy - 0.35) ** 2) * 1.5, 0, 1)
    a = np.array(c1, dtype=np.float32)[None, None, :]
    b = np.array(c2, dtype=np.float32)[None, None, :]
    arr = a + (b - a) * t[:, :, None]
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB").convert("RGBA")


def shape_mask(shape):
    m = Image.new("L", (W * 2, W * 2), 0)
    d = ImageDraw.Draw(m)
    x0, y0, x1, y1 = [v * 2 for v in BOX]
    if shape == "circle":
        d.ellipse((x0, y0, x1, y1), fill=255)
    elif shape == "square":
        d.rounded_rectangle((x0, y0, x1, y1), radius=40, fill=255)
    elif shape == "hex":
        cx, cy, r = W, W, (x1 - x0) / 2 * 1.04
        pts = [(cx + r * math.cos(math.radians(60 * i - 90)), cy + r * math.sin(math.radians(60 * i - 90))) for i in range(6)]
        d.polygon(pts, fill=255)
        m = m.filter(ImageFilter.GaussianBlur(3)).point(lambda v: 255 if v > 127 else 0)
    else:  # squircle
        d.rounded_rectangle((x0, y0, x1, y1), radius=int((x1 - x0) * 0.235), fill=255)
    return m.resize((W, W), Image.LANCZOS)


def colorize(mask, color, alpha=255):
    layer = Image.new("RGBA", (W, W), color + (alpha,))
    layer.putalpha(mask.point(lambda v: v * alpha // 255))
    return layer


def blur_glow(mask, radius, color, strength=1.0):
    g = mask.filter(ImageFilter.GaussianBlur(radius)).point(lambda v: min(255, int(v * strength)))
    return colorize(g, color, 255) if True else None


# ---- motifs de fond ---------------------------------------------------------------------------
def pat_scanlines(ov, rng, c):
    d = ImageDraw.Draw(ov)
    for y in range(40, 480, 7):
        d.line([(0, y), (W, y)], fill=(0, 0, 0, 70), width=3)
    for _ in range(90):
        x, y = rng.randrange(40, 470), rng.randrange(40, 470)
        d.rectangle((x, y, x + rng.randrange(3, 12), y + 2), fill=c + (60,))


def pat_circuit(ov, rng, c):
    d = ImageDraw.Draw(ov)
    for _ in range(11):
        x, y = rng.randrange(50, 460), rng.randrange(50, 460)
        pts = [(x, y)]
        for _ in range(3):
            if rng.random() < 0.5:
                x += rng.choice([-1, 1]) * rng.randrange(40, 110)
            else:
                y += rng.choice([-1, 1]) * rng.randrange(40, 110)
            pts.append((x, y))
        d.line(pts, fill=c + (85,), width=5, joint="curve")
        for px, py in (pts[0], pts[-1]):
            d.ellipse((px - 9, py - 9, px + 9, py + 9), outline=c + (140,), width=4)


def pat_neural(ov, rng, c):
    d = ImageDraw.Draw(ov)
    pts = [(rng.randrange(50, 462), rng.randrange(50, 462)) for _ in range(16)]
    for i, p in enumerate(pts):
        for q in pts[i + 1:]:
            if (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 < 150 ** 2:
                d.line([p, q], fill=c + (70,), width=3)
    for p in pts:
        d.ellipse((p[0] - 7, p[1] - 7, p[0] + 7, p[1] + 7), fill=c + (150,))


def pat_orbits(ov, rng, c):
    for ang in (0, 60, 120):
        o = Image.new("RGBA", (W, W), (0, 0, 0, 0))
        ImageDraw.Draw(o).ellipse((70, 170, 442, 342), outline=c + (120,), width=5)
        ov.alpha_composite(o.rotate(ang, resample=Image.BICUBIC))
    d = ImageDraw.Draw(ov)
    for _ in range(14):
        x, y = rng.randrange(60, 450), rng.randrange(60, 450)
        d.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(255, 255, 255, 150))


def pat_aurora(ov, rng, c):
    d = ImageDraw.Draw(ov)
    cols = [(120, 255, 200), (170, 120, 255), (255, 120, 210)]
    for i, col in enumerate(cols):
        pts = [(x, 150 + i * 70 + 38 * math.sin(x / 60.0 + i * 1.7) + 20 * math.sin(x / 23.0)) for x in range(0, W + 8, 8)]
        d.line(pts, fill=col + (90,), width=34, joint="curve")
    ov.paste(ov.filter(ImageFilter.GaussianBlur(10)))


def pat_grid(ov, rng, c):
    d = ImageDraw.Draw(ov)
    hz = 250
    for i in range(1, 9):
        y = hz + (i ** 1.8) * 6.2
        d.line([(0, y), (W, y)], fill=c + (130,), width=3)
    for i in range(-9, 10):
        d.line([(256, hz), (256 + i * 64, W)], fill=c + (130,), width=3)
    d.rectangle((0, 0, W, hz - 1), fill=(0, 0, 0, 0))


def pat_lava(ov, rng, c):
    d = ImageDraw.Draw(ov)
    for _ in range(9):
        x, y = rng.randrange(40, 480), rng.randrange(40, 480)
        pts = [(x, y)]
        for _ in range(5):
            x += rng.randrange(-60, 60)
            y += rng.randrange(-60, 60)
            pts.append((x, y))
        d.line(pts, fill=c + (120,), width=7, joint="curve")
    ov.paste(ov.filter(ImageFilter.GaussianBlur(2.5)))


def pat_dither(ov, rng, c):
    d = ImageDraw.Draw(ov)
    for y in range(40, 480, 16):
        for x in range(40 + (y // 16 % 2) * 8, 480, 16):
            d.rectangle((x, y, x + 7, y + 7), fill=(255, 255, 255, 26))


PATTERNS = {"scan": pat_scanlines, "circuit": pat_circuit, "neural": pat_neural, "orbit": pat_orbits,
            "aurora": pat_aurora, "grid": pat_grid, "lava": pat_lava, "dither": pat_dither, None: None}


# ----------------------------------------------------------------------------------------------
# Une tuile
# ----------------------------------------------------------------------------------------------
def make_tile(th, key, idx):
    st = th["tile"]
    rng = random.Random(f"{th['name']}-{key}")
    mask = shape_mask(st["shape"])
    c1, c2 = st["pal"][idx % len(st["pal"])]
    canvas = Image.new("RGBA", (W, W), (0, 0, 0, 0))

    # lueur externe (néon) puis ombre portée
    if st.get("outer_glow"):
        gc = st["outer_glow"][idx % len(st["outer_glow"])] if isinstance(st["outer_glow"], list) else st["outer_glow"]
        canvas.alpha_composite(blur_glow(mask, 16, gc, 1.5))
    else:
        sh = Image.new("L", (W, W), 0)
        sh.paste(mask, (0, 10))
        canvas.alpha_composite(colorize(sh.filter(ImageFilter.GaussianBlur(12)), (0, 0, 0), 120))

    # fond dégradé + motif + reflet
    body = grad_image(c1, c2, W, st.get("grad", "diag"))
    ov = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    pat = PATTERNS[st.get("pattern")]
    if pat:
        pat(ov, rng, st.get("pattern_color", (255, 255, 255)))
    body.alpha_composite(ov)
    if st.get("sheen", True):
        sheen = grad_image((255, 255, 255), (255, 255, 255), W)
        a = np.zeros((W, W), dtype=np.uint8)
        yy = np.linspace(0, 1, W)[:, None]
        a[:] = np.clip((1 - yy / 0.55) * st.get("sheen_alpha", 60), 0, 255).astype(np.uint8)
        sheen.putalpha(Image.fromarray(a, "L"))
        body.alpha_composite(sheen)
    tile = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    tile.paste(body, (0, 0), mask)
    canvas.alpha_composite(tile)

    # bordure interne
    bw = st.get("border_w", 6)
    if bw:
        inner = mask.filter(ImageFilter.MinFilter(2 * bw + 1))
        ring = ImageChops.subtract(mask, inner)
        b = st.get("border", (255, 255, 255, 70))
        bc = b[idx % len(b)] if isinstance(b, list) else b
        canvas.alpha_composite(colorize(ring, bc[:3], bc[3] if len(bc) > 3 else 255))

    # glyphe
    gsz = int((BOX[2] - BOX[0]) * st.get("glyph_scale", 0.60))
    gm = glyph_mask(key).resize((gsz, gsz), Image.LANCZOS)
    full = Image.new("L", (W, W), 0)
    off = ((W - gsz) // 2, (W - gsz) // 2 + st.get("glyph_dy", 0))
    full.paste(gm, off)
    gcol = st["glyph"][idx % len(st["glyph"])] if isinstance(st["glyph"], list) else st["glyph"]
    if st.get("glyph_glow"):
        canvas.alpha_composite(blur_glow(full, 10, st["glyph_glow"][idx % len(st["glyph_glow"])] if isinstance(st["glyph_glow"], list) else st["glyph_glow"], 1.3))
    elif st.get("glyph_shadow", True):
        sh = Image.new("L", (W, W), 0)
        sh.paste(full, (0, 7))
        canvas.alpha_composite(colorize(sh.filter(ImageFilter.GaussianBlur(6)), (0, 0, 0), 110))
    if isinstance(gcol[0], tuple):   # dégradé vertical
        gl = grad_image(gcol[0], gcol[1], W, "vert")
    else:
        gl = Image.new("RGBA", (W, W), tuple(gcol) + (255,))
    gl.putalpha(full)
    canvas.alpha_composite(gl)

    # pastille "corbeille pleine"
    if key == "recycle_full":
        bx, by, r = BOX[2] - 70, BOX[1] + 70, 46
        d = ImageDraw.Draw(canvas)
        d.ellipse((bx - r - 7, by - r - 7, bx + r + 7, by + r + 7), fill=(255, 255, 255, 255))
        d.ellipse((bx - r, by - r, bx + r, by + r), fill=th["badge"] + (255,))

    out = canvas.resize((256, 256), Image.LANCZOS)
    if st.get("pixel"):
        small = out.resize((32, 32), Image.BOX)
        a = small.getchannel("A").point(lambda v: 255 if v > 110 else 0)
        rgb_ = small.convert("RGB").point(lambda v: (v >> 4) * 17)
        px = Image.merge("RGBA", (*rgb_.split(), a))
        out = px.resize((256, 256), Image.NEAREST)
    return out


# ----------------------------------------------------------------------------------------------
# Définition des thèmes
# ----------------------------------------------------------------------------------------------
CLASSIC_PAL = [(rgb("5B9BFF"), rgb("2F64D8")), (rgb("F6B04A"), rgb("E3801C")), (rgb("46C9A0"), rgb("1C9072")),
               (rgb("B36CF0"), rgb("7A3FD1")), (rgb("EB5A8C"), rgb("C2336A")), (rgb("4CC3E6"), rgb("2A8CB8")),
               (rgb("F2994A"), rgb("E0573A")), (rgb("7C8BA3"), rgb("4B5870"))]
WHITE = (255, 255, 255)

THEMES = [
    dict(name="Dark", cat="Classique", desc="Verre sombre élégant, le thème d'origine.", dark=1, corner=0.34, bw=1.2, shadow=76,
         glow="000000", badge=rgb("FF5A46"),
         c=dict(BarTop="D22E313E", BarBottom="DC161822", Border1="64FFFFFF", Border2="16FFFFFF", Sheen="22FFFFFF", Accent="FF6EAFFF",
                LabelBg="EE14161E", LabelFg="FFFFFFFF", LabelBorder="3CFFFFFF", Highlight="22FFFFFF", Divider="46FFFFFF"),
         ui=dict(Bg="181A22", Panel="20222E", Text="EEF0F6", Muted="969CAC"),
         tile=dict(shape="squircle", pal=CLASSIC_PAL, glyph=WHITE)),
    dict(name="Light", cat="Classique", desc="Verre clair et lumineux.", dark=0, corner=0.34, bw=1.2, shadow=52, glow="1E2846", badge=rgb("FF5A46"),
         c=dict(BarTop="DEFFFFFF", BarBottom="D2E8ECF5", Border1="BEFFFFFF", Border2="46788296", Sheen="46FFFFFF", Accent="FF006ED2",
                LabelBg="F0FCFCFE", LabelFg="FF1C1E24", LabelBorder="3C000000", Highlight="2E000000", Divider="46000000"),
         ui=dict(Bg="F3F5FA", Panel="E6EAF3", Text="1C1E24", Muted="667088"),
         tile=dict(shape="squircle", pal=CLASSIC_PAL, glyph=WHITE)),
    # ---------------- Informatique ----------------
    dict(name="Terminal", cat="Informatique", desc="Console verte sur fond noir, façon hacker.", dark=1, corner=0.16, bw=1.6, shadow=110,
         glow="00FF50", badge=rgb("33FF66"),
         c=dict(BarTop="E6061008", BarBottom="F0020804", Border1="C833FF66", Border2="4033FF66", Sheen="0A33FF66", Accent="FF33FF66",
                LabelBg="F0040A06", LabelFg="FF8BFFAA", LabelBorder="8C33FF66", Highlight="2833FF66", Divider="6033FF66"),
         ui=dict(Bg="050A06", Panel="0A140C", Text="B6FFC8", Muted="4FA86A"),
         tile=dict(shape="square", pal=[(rgb("0B1A0F"), rgb("020604")), (rgb("0A1B14"), rgb("020806"))], glyph=[rgb("39FF6A"), rgb("7CFFA0")],
                   pattern="scan", pattern_color=rgb("39FF6A"), border=(rgb("33FF66") + (200,)), border_w=5, sheen=False,
                   outer_glow=[rgb("0F8F3A")], glyph_glow=[rgb("1FD05A")])),
    dict(name="Circuit", cat="Informatique", desc="Carte électronique bleu nuit, pistes cuivre et cyan.", dark=1, corner=0.30, bw=1.4, shadow=96,
         glow="00B4FF", badge=rgb("FF9F43"),
         c=dict(BarTop="DC0C2238", BarBottom="E6050F1E", Border1="B419D3FF", Border2="3C19D3FF", Sheen="1419D3FF", Accent="FF19D3FF",
                LabelBg="F005101C", LabelFg="FFD6F6FF", LabelBorder="8C19D3FF", Highlight="2819D3FF", Divider="6419D3FF"),
         ui=dict(Bg="070F1A", Panel="0D1A2B", Text="DCF4FF", Muted="6FA3BF"),
         tile=dict(shape="squircle", pal=[(rgb("0F3556"), rgb("061424")), (rgb("0C4A5E"), rgb("061A2A")), (rgb("143A66"), rgb("071526"))],
                   glyph=[(rgb("B5F4FF"), rgb("19D3FF")), (rgb("FFD7A8"), rgb("FF9F43"))], pattern="circuit", pattern_color=rgb("19D3FF"),
                   border=rgb("19D3FF") + (170,), border_w=4, glyph_glow=[rgb("0A9CC8")])),
    dict(name="Silicon", cat="Informatique", desc="Aluminium et verre clair, sobre et technique.", dark=0, corner=0.36, bw=1.0, shadow=60,
         glow="28344F", badge=rgb("0A84FF"),
         c=dict(BarTop="E6F7F8FB", BarBottom="DCDDE1EA", Border1="D2FFFFFF", Border2="508C96AA", Sheen="5AFFFFFF", Accent="FF0A84FF",
                LabelBg="F2F7F8FB", LabelFg="FF22262E", LabelBorder="46000000", Highlight="2A0A84FF", Divider="50000000"),
         ui=dict(Bg="EEF0F5", Panel="E0E4ED", Text="20242C", Muted="6A7488"),
         tile=dict(shape="squircle", pal=[(rgb("FFFFFF"), rgb("D5DAE6")), (rgb("F4F6FB"), rgb("C9D0E0"))], glyph=[(rgb("4C8DFF"), rgb("1F57C9")), (rgb("5F6B85"), rgb("323A4D"))],
                   border=(255, 255, 255, 220), border_w=5, sheen_alpha=80, glyph_shadow=True)),
    # ---------------- Intelligence artificielle ----------------
    dict(name="Neural", cat="IA", desc="Réseau de neurones violet et magenta.", dark=1, corner=0.40, bw=1.4, shadow=96, glow="B050FF", badge=rgb("FF5AD1"),
         c=dict(BarTop="DC2A1352", BarBottom="E6120824", Border1="B4D79BFF", Border2="3CB46BFF", Sheen="1EFFFFFF", Accent="FFC77DFF",
                LabelBg="F0140A2A", LabelFg="FFFFFFFF", LabelBorder="8CB46BFF", Highlight="2EC77DFF", Divider="64D79BFF"),
         ui=dict(Bg="100A20", Panel="1A1030", Text="F1E8FF", Muted="A38CC8"),
         tile=dict(shape="circle", pal=[(rgb("8E3BFF"), rgb("3A1180")), (rgb("D13BFF"), rgb("55147F")), (rgb("5B5BFF"), rgb("231A80"))],
                   glyph=WHITE, pattern="neural", pattern_color=rgb("E7CCFF"), border=(255, 255, 255, 120), border_w=4,
                   outer_glow=[rgb("8E3BFF")], glyph_glow=[rgb("F0B8FF")])),
    dict(name="Quantum", cat="IA", desc="Orbites et particules cyan-violet, effet hexagonal.", dark=1, corner=0.30, bw=1.4, shadow=96, glow="22E6D2", badge=rgb("3EF0E0"),
         c=dict(BarTop="DC0A2432", BarBottom="E6060C26", Border1="B43EF0E0", Border2="3C8F6BFF", Sheen="1C3EF0E0", Accent="FF3EF0E0",
                LabelBg="F005121E", LabelFg="FFE0FFFA", LabelBorder="8C3EF0E0", Highlight="283EF0E0", Divider="643EF0E0"),
         ui=dict(Bg="06121C", Panel="0C1E2C", Text="DDFFFA", Muted="6DB8B2"),
         tile=dict(shape="hex", pal=[(rgb("14C9B8"), rgb("3A1FA8")), (rgb("1E9BFF"), rgb("5A1FB8")), (rgb("3EF0E0"), rgb("2A3FC8"))],
                   glyph=WHITE, pattern="orbit", pattern_color=rgb("D6FFFA"), border=(220, 255, 250, 150), border_w=5,
                   outer_glow=[rgb("19B8AA")], glyph_glow=[rgb("8FFFF2")])),
    dict(name="Aurora", cat="IA", desc="Aurore boréale : dégradés vert, violet et rose.", dark=1, corner=0.34, bw=1.2, shadow=78, glow="6446C8", badge=rgb("FF8CD2"),
         c=dict(BarTop="D6463880", BarBottom="E0161C4E", Border1="6EC8BEFF", Border2="1EA0AAFF", Sheen="28FFFFFF", Accent="FFFF96D2",
                LabelBg="EE1A163E", LabelFg="FFFFFFFF", LabelBorder="46C8BEFF", Highlight="28FFFFFF", Divider="50FFFFFF"),
         ui=dict(Bg="14122E", Panel="1E1B42", Text="F0EEFF", Muted="9C98CC"),
         tile=dict(shape="squircle", pal=[(rgb("2A8C9E"), rgb("3B1F78")), (rgb("6A3FC8"), rgb("1E2D78")), (rgb("B13FA8"), rgb("3A1F78"))],
                   glyph=WHITE, pattern="aurora", border=(255, 255, 255, 110), border_w=4, glyph_glow=[rgb("B9A6FF")])),
    # ---------------- Games ----------------
    dict(name="NeonArcade", cat="Games", desc="Salle d'arcade synthwave : néons magenta et cyan.", dark=1, corner=0.30, bw=2.0, shadow=120, glow="FF2BD6", badge=rgb("00F0FF"),
         c=dict(BarTop="E6140028", BarBottom="F00A0016", Border1="E6FF2BD6", Border2="C800F0FF", Sheen="0AFFFFFF", Accent="FFFF2BD6",
                LabelBg="F00A0014", LabelFg="FFE9FFFF", LabelBorder="C800F0FF", Highlight="32FF2BD6", Divider="8C00F0FF"),
         ui=dict(Bg="0B0014", Panel="160528", Text="F3E9FF", Muted="A482C8"),
         tile=dict(shape="squircle", pal=[(rgb("1C0A3A"), rgb("0A0418")), (rgb("12103A"), rgb("05041A"))], glyph=[rgb("FF4BE1"), rgb("19F3FF")],
                   pattern="grid", pattern_color=rgb("B04BFF"), border=[rgb("FF2BD6") + (255,), rgb("00F0FF") + (255,)], border_w=6,
                   sheen=False, outer_glow=[rgb("FF2BD6"), rgb("00C8E0")], glyph_glow=[rgb("FF2BD6"), rgb("00F0FF")])),
    dict(name="RetroPixel", cat="Games", desc="Pixel art 8 bits, palette Sweetie-16.", dark=1, corner=0.10, bw=3.0, shadow=140, glow="000000", badge=rgb("FFCD75"),
         c=dict(BarTop="FF333C57", BarBottom="FF1A1C2C", Border1="FFF4F4F4", Border2="FF566C86", Sheen="00FFFFFF", Accent="FFFFCD75",
                LabelBg="FF1A1C2C", LabelFg="FFF4F4F4", LabelBorder="FFF4F4F4", Highlight="3CFFCD75", Divider="FF566C86"),
         ui=dict(Bg="1A1C2C", Panel="262B44", Text="F4F4F4", Muted="94B0C2"),
         tile=dict(shape="square", pal=[(rgb("41A6F6"), rgb("3B5DC9")), (rgb("73EFF7"), rgb("29366F")), (rgb("A7F070"), rgb("38B764")),
                                       (rgb("FFCD75"), rgb("EF7D57")), (rgb("B13E53"), rgb("5D275D"))], glyph=rgb("F4F4F4"),
                   border=(26, 28, 44, 255), border_w=14, sheen=False, glyph_shadow=False, pixel=True, pattern="dither", glyph_scale=0.58)),
    dict(name="Lava", cat="Games", desc="Forge de RPG : pierre volcanique, or et braises.", dark=1, corner=0.28, bw=2.0, shadow=120, glow="FF4A1C", badge=rgb("FFB02E"),
         c=dict(BarTop="E6301008", BarBottom="F0180604", Border1="DCFFB02E", Border2="64A8501C", Sheen="14FFB02E", Accent="FFFFB02E",
                LabelBg="F0180804", LabelFg="FFFFE3A8", LabelBorder="B4FFB02E", Highlight="32FF7A1C", Divider="78FFB02E"),
         ui=dict(Bg="140806", Panel="22100C", Text="FFE8C4", Muted="B88458"),
         tile=dict(shape="squircle", pal=[(rgb("6A1608"), rgb("1E0504")), (rgb("86220C"), rgb("260804")), (rgb("5A2410"), rgb("1A0806"))],
                   glyph=[(rgb("FFE9A8"), rgb("FFB02E"))], pattern="lava", pattern_color=rgb("FF7A1C"), border=rgb("FFB02E") + (230,), border_w=6,
                   outer_glow=[rgb("FF4A1C")], glyph_glow=[rgb("FF7A1C")], sheen_alpha=36)),
]


def hexc(t):
    return "%02X%02X%02X" % tuple(t[:3])


def write_ini(path, th):
    st = th["tile"]
    pal = ",".join(hexc(a) + ":" + hexc(b) for a, b in st["pal"])
    L = ["[Theme]", f"Name={th['name']}", f"Category={th['cat']}", f"Description={th['desc']}", f"Dark={th['dark']}",
         f"CornerRadius={th['corner']}", f"BorderWidth={th['bw']}", f"ShadowAlpha={th['shadow']}", f"Glow={th['glow']}",
         f"TileShape={st['shape']}", "[Colors]"]
    L += [f"{k}={v}" for k, v in th["c"].items()]
    L += ["[UI]"] + [f"{k}={v}" for k, v in th["ui"].items()]
    L += ["[Tiles]", f"Palette={pal}"]
    with open(path, "w", encoding="utf-16", newline="") as f:
        f.write("\r\n".join(L) + "\r\n")


def main():
    for th in THEMES:
        d = os.path.join(OUT, th["name"])
        os.makedirs(os.path.join(d, "Icons"), exist_ok=True)
        write_ini(os.path.join(d, "theme.ini"), th)
        for i, key in enumerate(KEYS):
            make_tile(th, key, i).save(os.path.join(d, "Icons", key + ".png"), optimize=True)
        print("OK", th["name"])


if __name__ == "__main__":
    main()
