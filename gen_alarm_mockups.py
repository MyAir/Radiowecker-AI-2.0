"""Generate mockup PNGs for the alarm-setup and alarm-firing screens.

Produces 6 images @ 800x480 in examples/mockups/:
  - alarm_setup_{1,2,3}.png  (3 layout variants of the CRUD screen)
  - alarm_screen_{1,2,3}.png (3 layout variants of the firing screen)

All variants use a brighter palette than the dim bedroom-mode main screen.
Run from project root:    python gen_alarm_mockups.py
"""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 800, 480
FONT_DIR = "C:/Windows/Fonts/"
OUT_DIR = "examples/mockups"
ICON_DIR = "SD-Data/assets/weather_icons"

os.makedirs(OUT_DIR, exist_ok=True)


def font(name, size):
    return ImageFont.truetype(FONT_DIR + name + ".ttf", size)


def text_lm(d, x, y, s, f, c):
    d.text((x, y), s, font=f, fill=c, anchor="lm")


def text_mm(d, x, y, s, f, c):
    d.text((x, y), s, font=f, fill=c, anchor="mm")


def text_rm(d, x, y, s, f, c):
    d.text((x, y), s, font=f, fill=c, anchor="rm")


def rrect(d, box, radius, fill=None, outline=None, width=1):
    d.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def paste_icon(img, code, x, y, size):
    path = os.path.join(ICON_DIR, code + ".png")
    if not os.path.exists(path):
        return
    ic = Image.open(path).convert("RGBA").resize((size, size), Image.LANCZOS)
    img.paste(ic, (x, y), mask=ic.split()[3])


def days_chip_row(d, x, y, active_mask, font_small):
    """Draw 7 weekday chips Mo..So at (x, y), active ones highlighted."""
    labels = ["Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"]
    # weekday bit order in code: bit 0 = Sunday … bit 6 = Saturday.
    # active_mask uses that representation; map Mo..So accordingly.
    bit_of = [1, 2, 3, 4, 5, 6, 0]
    chip_w, chip_h, gap = 36, 32, 6
    for i, lab in enumerate(labels):
        active = (active_mask >> bit_of[i]) & 1
        bx = x + i * (chip_w + gap)
        if active:
            rrect(d, [bx, y, bx + chip_w, y + chip_h], radius=6,
                  fill=(60, 130, 200), outline=(120, 180, 230), width=1)
            tc = (255, 255, 255)
        else:
            rrect(d, [bx, y, bx + chip_w, y + chip_h], radius=6,
                  fill=(48, 52, 60), outline=(80, 84, 92), width=1)
            tc = (160, 165, 175)
        text_mm(d, bx + chip_w // 2, y + chip_h // 2, lab, font_small, tc)


# ============================================================================
# SETUP SCREEN 1 — "Two-Pane" (list left, editor right)
# Light grey theme, accent blue
# ============================================================================
def setup_1():
    BG = (236, 238, 242)
    PANEL = (250, 251, 253)
    TITLE = (28, 36, 52)
    SUB = (90, 100, 116)
    ACCENT = (40, 110, 200)
    DIV = (210, 215, 222)
    DANGER = (200, 70, 70)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # Header bar
    d.rectangle([0, 0, W, 54], fill=(245, 247, 250))
    d.line([(0, 54), (W, 54)], fill=DIV, width=1)
    rrect(d, [12, 10, 102, 44], radius=6, fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, 57, 27, "Zurück", font("segoeui", 17), TITLE)
    text_mm(d, W // 2, 27, "Alarme verwalten", font("segoeuib", 22), TITLE)
    rrect(d, [W - 130, 10, W - 12, 44], radius=6, fill=ACCENT, outline=None)
    text_mm(d, W - 71, 27, "+ Neu", font("segoeuib", 17), (255, 255, 255))

    # Left list
    LX, LY, LW = 12, 66, 340
    rrect(d, [LX, LY, LX + LW, H - 12], radius=10, fill=PANEL, outline=DIV, width=1)
    items = [
        ("Wecker Werktag", "Mo Di Mi Do Fr  06:30", True,  True),   # selected
        ("Wochenende",     "Sa So  08:00",          True,  False),
        ("Termin morgen",  "Einmalig  07:15",       False, False),
    ]
    iy = LY + 12
    for title, sub, enabled, selected in items:
        if selected:
            rrect(d, [LX + 6, iy, LX + LW - 6, iy + 64], radius=8,
                  fill=(225, 235, 250), outline=ACCENT, width=1)
        text_lm(d, LX + 16, iy + 18, title, font("segoeuib", 18), TITLE)
        text_lm(d, LX + 16, iy + 44, sub, font("segoeui", 14), SUB)
        # toggle
        tx = LX + LW - 60
        if enabled:
            rrect(d, [tx, iy + 18, tx + 44, iy + 38], radius=10, fill=ACCENT)
            d.ellipse([tx + 24, iy + 20, tx + 42, iy + 36], fill=(255, 255, 255))
        else:
            rrect(d, [tx, iy + 18, tx + 44, iy + 38], radius=10, fill=(200, 205, 212))
            d.ellipse([tx + 2, iy + 20, tx + 20, iy + 36], fill=(255, 255, 255))
        iy += 72

    # Right editor panel
    EX, EY, EW, EH = 364, 66, W - 376, H - 78
    rrect(d, [EX, EY, EX + EW, EY + EH], radius=10, fill=PANEL, outline=DIV, width=1)

    text_lm(d, EX + 16, EY + 22, "Titel", font("segoeuib", 14), SUB)
    rrect(d, [EX + 16, EY + 38, EX + EW - 16, EY + 76], radius=6,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_lm(d, EX + 24, EY + 57, "Wecker Werktag", font("segoeui", 18), TITLE)

    # Time pickers
    text_lm(d, EX + 16, EY + 96, "Uhrzeit", font("segoeuib", 14), SUB)
    rrect(d, [EX + 16, EY + 112, EX + 96, EY + 178], radius=6,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, EX + 56, EY + 145, "06", font("segoeuib", 36), TITLE)
    text_mm(d, EX + 112, EY + 145, ":", font("segoeuib", 36), TITLE)
    rrect(d, [EX + 128, EY + 112, EX + 208, EY + 178], radius=6,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, EX + 168, EY + 145, "30", font("segoeuib", 36), TITLE)

    # Volume slider
    text_lm(d, EX + 230, EY + 96, "Lautstärke", font("segoeuib", 14), SUB)
    sx, sy, sw = EX + 230, EY + 138, EW - 246
    d.rectangle([sx, sy, sx + sw, sy + 6], fill=(210, 215, 222))
    d.rectangle([sx, sy, sx + int(sw * 0.55), sy + 6], fill=ACCENT)
    d.ellipse([sx + int(sw * 0.55) - 9, sy - 6, sx + int(sw * 0.55) + 9, sy + 12], fill=ACCENT)
    text_lm(d, sx, sy + 22, "12 / 21", font("segoeui", 13), SUB)

    # Weekday chips
    text_lm(d, EX + 16, EY + 200, "Wochentage", font("segoeuib", 14), SUB)
    days_chip_row(d, EX + 16, EY + 220, (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5), font("segoeui", 13))

    # Sound dropdown
    text_lm(d, EX + 16, EY + 268, "Klang", font("segoeuib", 14), SUB)
    rrect(d, [EX + 16, EY + 286, EX + EW - 16, EY + 322], radius=6,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_lm(d, EX + 24, EY + 304, "[Stream] SRF 3", font("segoeui", 16), TITLE)
    text_rm(d, EX + EW - 24, EY + 304, "▾", font("segoeui", 18), SUB)

    # Preview + Stop + Save + Cancel + Delete
    by = EY + EH - 56
    rrect(d, [EX + 16, by, EX + 112, by + 40], radius=6, fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, EX + 64, by + 20, "Probe", font("segoeui", 16), ACCENT)
    rrect(d, [EX + 120, by, EX + 200, by + 40], radius=6, fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, EX + 160, by + 20, "Stop", font("segoeui", 16), TITLE)
    rrect(d, [EX + 208, by, EX + 296, by + 40], radius=6, fill=(255, 255, 255), outline=DANGER, width=1)
    text_mm(d, EX + 252, by + 20, "Löschen", font("segoeui", 16), DANGER)
    rrect(d, [EX + EW - 230, by, EX + EW - 124, by + 40], radius=6, fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, EX + EW - 177, by + 20, "Abbrechen", font("segoeui", 16), TITLE)
    rrect(d, [EX + EW - 116, by, EX + EW - 16, by + 40], radius=6, fill=ACCENT)
    text_mm(d, EX + EW - 66, by + 20, "Speichern", font("segoeuib", 16), (255, 255, 255))

    img.save(os.path.join(OUT_DIR, "alarm_setup_1.png"))
    print("setup_1 done")


# ============================================================================
# SETUP SCREEN 2 — "Full-screen list + modal edit"
# Dark-on-light cards
# ============================================================================
def setup_2():
    BG = (245, 246, 249)
    CARD = (255, 255, 255)
    TITLE = (24, 28, 38)
    SUB = (110, 120, 134)
    ACCENT = (90, 50, 180)   # purple
    GREEN = (60, 160, 110)
    GREY = (215, 218, 224)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # Header
    d.rectangle([0, 0, W, 60], fill=CARD)
    d.line([(0, 60), (W, 60)], fill=GREY, width=1)
    rrect(d, [16, 12, 96, 48], radius=8, fill=(245, 246, 249), outline=GREY, width=1)
    text_mm(d, 56, 30, "‹", font("segoeuib", 28), TITLE)
    text_lm(d, 116, 30, "Alarme", font("segoeuib", 26), TITLE)
    rrect(d, [W - 154, 12, W - 16, 48], radius=8, fill=ACCENT)
    text_mm(d, W - 85, 30, "+ Hinzufügen", font("segoeuib", 16), (255, 255, 255))

    # Three large cards
    cards = [
        ("Wecker Werktag",  "06:30", "Mo Di Mi Do Fr", "[Stream] SRF 3", True),
        ("Wochenende",      "08:00", "Sa  So",          "[SD] /Chef316.mp3", True),
        ("Termin morgen",   "07:15", "Einmalig 25.05.", "[Stream] Radio Swiss Pop", False),
    ]
    cy = 78
    for title, t, days, sound, en in cards:
        rrect(d, [16, cy, W - 16, cy + 116], radius=12, fill=CARD,
              outline=GREY, width=1)
        # big time
        text_lm(d, 36, cy + 36, t, font("segoeuib", 44), TITLE)
        # title + days + sound (stacked right of time)
        text_lm(d, 196, cy + 26, title, font("segoeuib", 22), TITLE)
        text_lm(d, 196, cy + 58, days, font("segoeui", 16), SUB)
        text_lm(d, 196, cy + 84, sound, font("segoeui", 15), ACCENT)
        # toggle
        tx = W - 110
        if en:
            rrect(d, [tx, cy + 44, tx + 60, cy + 76], radius=16, fill=GREEN)
            d.ellipse([tx + 32, cy + 46, tx + 58, cy + 74], fill=(255, 255, 255))
        else:
            rrect(d, [tx, cy + 44, tx + 60, cy + 76], radius=16, fill=(200, 205, 212))
            d.ellipse([tx + 2, cy + 46, tx + 28, cy + 74], fill=(255, 255, 255))
        cy += 128

    img.save(os.path.join(OUT_DIR, "alarm_setup_2.png"))
    print("setup_2 done")


# ============================================================================
# SETUP SCREEN 3 — "Tabbed: List | Edit | Preview"
# ============================================================================
def setup_3():
    BG = (250, 248, 240)
    PANEL = (255, 254, 248)
    TITLE = (40, 30, 14)
    SUB = (120, 100, 70)
    ACCENT = (200, 110, 20)  # warm orange
    DIV = (220, 210, 190)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # Header
    d.rectangle([0, 0, W, 50], fill=(244, 238, 225))
    d.line([(0, 50), (W, 50)], fill=DIV, width=1)
    rrect(d, [12, 9, 92, 41], radius=6, fill=PANEL, outline=DIV, width=1)
    text_mm(d, 52, 25, "Zurück", font("segoeui", 16), TITLE)
    text_lm(d, 112, 25, "Alarme – Bearbeiten", font("segoeuib", 22), TITLE)

    # Tab strip
    TAB_Y = 62
    tabs = [("Liste (3)", False), ("Bearbeiten", True), ("Vorschau", False)]
    tx = 16
    for lab, active in tabs:
        tw = 150
        if active:
            rrect(d, [tx, TAB_Y, tx + tw, TAB_Y + 36], radius=6,
                  fill=ACCENT)
            text_mm(d, tx + tw // 2, TAB_Y + 18, lab, font("segoeuib", 16), (255, 255, 255))
        else:
            rrect(d, [tx, TAB_Y, tx + tw, TAB_Y + 36], radius=6,
                  fill=PANEL, outline=DIV, width=1)
            text_mm(d, tx + tw // 2, TAB_Y + 18, lab, font("segoeui", 16), SUB)
        tx += tw + 8

    # Main edit panel
    PX, PY, PW, PH = 16, 110, W - 32, 320
    rrect(d, [PX, PY, PX + PW, PY + PH], radius=10, fill=PANEL,
          outline=DIV, width=1)

    # Big time pickers center
    cx = PX + 200
    text_mm(d, cx, PY + 36, "Uhrzeit", font("segoeuib", 14), SUB)
    rrect(d, [cx - 90, PY + 56, cx - 10, PY + 156], radius=8,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, cx - 50, PY + 76, "05", font("segoeui", 22), (180, 170, 150))
    text_mm(d, cx - 50, PY + 108, "06", font("segoeuib", 38), TITLE)
    text_mm(d, cx - 50, PY + 140, "07", font("segoeui", 22), (180, 170, 150))
    text_mm(d, cx + 10, PY + 108, ":", font("segoeuib", 38), TITLE)
    rrect(d, [cx + 30, PY + 56, cx + 110, PY + 156], radius=8,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_mm(d, cx + 70, PY + 76, "29", font("segoeui", 22), (180, 170, 150))
    text_mm(d, cx + 70, PY + 108, "30", font("segoeuib", 38), TITLE)
    text_mm(d, cx + 70, PY + 140, "31", font("segoeui", 22), (180, 170, 150))

    # Right column: title, weekday chips, sound, volume
    rx = PX + 350
    text_lm(d, rx, PY + 30, "Titel", font("segoeuib", 14), SUB)
    rrect(d, [rx, PY + 46, PX + PW - 16, PY + 78], radius=6,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_lm(d, rx + 10, PY + 62, "Wecker Werktag", font("segoeui", 16), TITLE)

    text_lm(d, rx, PY + 96, "Wochentage", font("segoeuib", 14), SUB)
    days_chip_row(d, rx, PY + 114, (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5),
                  font("segoeui", 13))

    text_lm(d, rx, PY + 162, "Klang", font("segoeuib", 14), SUB)
    rrect(d, [rx, PY + 178, PX + PW - 16, PY + 214], radius=6,
          fill=(255, 255, 255), outline=DIV, width=1)
    text_lm(d, rx + 10, PY + 196, "[Stream] SRF 3", font("segoeui", 16), TITLE)
    text_rm(d, PX + PW - 26, PY + 196, "▾", font("segoeui", 18), SUB)

    text_lm(d, rx, PY + 230, "Lautstärke", font("segoeuib", 14), SUB)
    sx, sy, sw = rx, PY + 260, (PX + PW - 16) - rx
    d.rectangle([sx, sy, sx + sw, sy + 6], fill=(220, 210, 190))
    d.rectangle([sx, sy, sx + int(sw * 0.55), sy + 6], fill=ACCENT)
    d.ellipse([sx + int(sw * 0.55) - 10, sy - 7, sx + int(sw * 0.55) + 10, sy + 13], fill=ACCENT)
    text_rm(d, PX + PW - 16, sy + 22, "12 / 21", font("segoeui", 13), SUB)

    # Action row
    by = H - 50
    rrect(d, [16, by, 156, by + 38], radius=6, fill=PANEL, outline=DIV, width=1)
    text_mm(d, 86, by + 19, "▶ Probe", font("segoeuib", 16), ACCENT)
    rrect(d, [164, by, 264, by + 38], radius=6, fill=PANEL, outline=DIV, width=1)
    text_mm(d, 214, by + 19, "Stop", font("segoeui", 16), TITLE)
    rrect(d, [W - 360, by, W - 188, by + 38], radius=6, fill=PANEL, outline=(190, 60, 60), width=1)
    text_mm(d, W - 274, by + 19, "Löschen", font("segoeui", 16), (190, 60, 60))
    rrect(d, [W - 180, by, W - 16, by + 38], radius=6, fill=ACCENT)
    text_mm(d, W - 98, by + 19, "Speichern", font("segoeuib", 16), (255, 255, 255))

    img.save(os.path.join(OUT_DIR, "alarm_setup_3.png"))
    print("setup_3 done")


# ============================================================================
# ALARM SCREEN 1 — "Horizontal weather strip across bottom"
# Bright cream theme
# ============================================================================
def alarm_1():
    BG = (252, 246, 232)
    CARD = (255, 252, 244)
    TITLE = (40, 30, 12)
    SUB = (110, 92, 60)
    ACCENT = (220, 110, 20)
    DIV = (220, 210, 190)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # Top: alarm title centered, date/time below
    text_mm(d, W // 2, 42, "♪  Wecker Werktag", font("segoeuib", 28), ACCENT)
    text_mm(d, W // 2, 74, "Dienstag, 26. Mai 2026", font("segoeui", 22), SUB)

    # Huge clock
    text_mm(d, W // 2, 180, "06:30", font("segoeuib", 160), TITLE)

    # Now-playing / metadata
    rrect(d, [80, 268, W - 80, 312], radius=8, fill=CARD,
          outline=DIV, width=1)
    text_mm(d, W // 2, 290, "SRF 3 – Pegasus: Mañana Mañana", font("segoeui", 20), TITLE)

    # Weather strip — 4 tiles horizontally
    WY, WH = 330, 100
    titles = ["Jetzt", "Morgen früh", "Nachmittag", "Morgen"]
    temps = ["14°", "11°", "20°", "18°"]
    pops = [None, "20 %", "10 %", "60 %"]
    icons = ["10d", "04d", "02d", "10d"]
    cw = (W - 80) // 4
    cx = 40
    for i in range(4):
        bx = cx + i * cw
        rrect(d, [bx + 6, WY, bx + cw - 6, WY + WH], radius=10,
              fill=CARD, outline=DIV, width=1)
        text_mm(d, bx + cw // 2, WY + 16, titles[i], font("segoeuib", 14), SUB)
        paste_icon(img, icons[i], bx + cw // 2 - 26, WY + 22, 52)
        text_mm(d, bx + cw // 2 + 50, WY + 56, temps[i], font("segoeuib", 22), TITLE)
        if pops[i]:
            text_mm(d, bx + cw // 2, WY + WH - 14, "💧 " + pops[i],
                    font("segoeui", 13), (60, 120, 200))

    # Snooze / Stop big buttons
    by = 442
    rrect(d, [60, by - 22, 360, by + 22], radius=22, fill=(230, 222, 200),
          outline=DIV, width=1)
    text_mm(d, 210, by, "Schlummern (9 Min)", font("segoeuib", 22), TITLE)
    rrect(d, [440, by - 22, 740, by + 22], radius=22, fill=ACCENT)
    text_mm(d, 590, by, "Stop", font("segoeuib", 22), (255, 255, 255))

    img.save(os.path.join(OUT_DIR, "alarm_screen_1.png"))
    print("alarm_1 done")


# ============================================================================
# ALARM SCREEN 2 — "2x2 weather grid on the right"
# ============================================================================
def alarm_2():
    BG = (240, 248, 255)
    CARD = (255, 255, 255)
    TITLE = (20, 30, 60)
    SUB = (90, 110, 140)
    ACCENT = (30, 110, 200)
    DIV = (210, 220, 235)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # Left column: title, big time, metadata
    text_lm(d, 24, 40, "♪  Wecker Werktag", font("segoeuib", 26), ACCENT)
    text_lm(d, 24, 72, "Dienstag, 26. Mai 2026", font("segoeui", 18), SUB)

    text_mm(d, 230, 220, "06:30", font("segoeuib", 150), TITLE)

    rrect(d, [24, 308, 440, 364], radius=8, fill=CARD,
          outline=DIV, width=1)
    text_lm(d, 38, 326, "Jetzt läuft:", font("segoeui", 13), SUB)
    text_lm(d, 38, 348, "SRF 3 – Pegasus: Mañana Mañana", font("segoeuib", 16), TITLE)

    # Right column: 2x2 weather grid
    GX, GY, GW, GH = 460, 24, W - 484, 348
    rrect(d, [GX, GY, GX + GW, GY + GH], radius=12, fill=CARD,
          outline=DIV, width=1)
    text_mm(d, GX + GW // 2, GY + 24, "Wetter", font("segoeuib", 18), ACCENT)
    tiles = [
        ("Jetzt", "14°", None, "10d"),
        ("Morgen früh", "11°", "20 %", "04d"),
        ("Nachmittag", "20°", "10 %", "02d"),
        ("Morgen", "18°", "60 %", "10d"),
    ]
    cell_w, cell_h = GW // 2, (GH - 48) // 2
    for i, (lab, t, pop, ic) in enumerate(tiles):
        cx = GX + (i % 2) * cell_w
        cy = GY + 48 + (i // 2) * cell_h
        text_mm(d, cx + cell_w // 2, cy + 18, lab, font("segoeuib", 14), SUB)
        paste_icon(img, ic, cx + cell_w // 2 - 30, cy + 28, 60)
        text_mm(d, cx + cell_w // 2 + 56, cy + 60, t, font("segoeuib", 22), TITLE)
        if pop:
            text_mm(d, cx + cell_w // 2, cy + cell_h - 14, "💧 " + pop,
                    font("segoeui", 13), (60, 120, 200))

    # Snooze / Stop buttons across bottom
    by = 432
    rrect(d, [40, by - 22, 380, by + 22], radius=22, fill=(225, 232, 245),
          outline=DIV, width=1)
    text_mm(d, 210, by, "Schlummern (9 Min)", font("segoeuib", 22), TITLE)
    rrect(d, [420, by - 22, 760, by + 22], radius=22, fill=ACCENT)
    text_mm(d, 590, by, "Stop", font("segoeuib", 22), (255, 255, 255))

    img.save(os.path.join(OUT_DIR, "alarm_screen_2.png"))
    print("alarm_2 done")


# ============================================================================
# ALARM SCREEN 3 — "Hero current + small forecast row"
# Warm sunrise theme
# ============================================================================
def alarm_3():
    BG = (255, 245, 232)
    CARD = (255, 252, 244)
    TITLE = (60, 30, 10)
    SUB = (140, 90, 40)
    ACCENT = (220, 100, 30)
    DIV = (230, 215, 190)

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # Slim top bar: title + date/time
    text_lm(d, 24, 30, "♪  Wecker Werktag", font("segoeuib", 24), ACCENT)
    text_rm(d, W - 24, 30, "Dienstag, 26. Mai 2026  06:30:14",
            font("segoeui", 18), SUB)
    d.line([(24, 56), (W - 24, 56)], fill=DIV, width=1)

    # Left: hero current weather
    HX, HY, HW, HH = 24, 76, 360, 256
    rrect(d, [HX, HY, HX + HW, HY + HH], radius=14, fill=CARD,
          outline=DIV, width=1)
    text_mm(d, HX + HW // 2, HY + 26, "Jetzt", font("segoeuib", 18), SUB)
    paste_icon(img, "10d", HX + HW // 2 - 64, HY + 40, 128)
    text_mm(d, HX + HW // 2, HY + 196, "14°", font("segoeuib", 56), TITLE)
    text_mm(d, HX + HW // 2, HY + 234, "Leichter Regen", font("segoeui", 18), SUB)

    # Right: time hero
    text_mm(d, 580, 196, "06:30", font("segoeuib", 160), TITLE)

    # Now-playing strip below time
    rrect(d, [400, 296, W - 24, 336], radius=8, fill=CARD,
          outline=DIV, width=1)
    text_mm(d, (400 + W - 24) // 2, 316,
            "SRF 3 – Pegasus: Mañana Mañana", font("segoeui", 18), TITLE)

    # 3-tile forecast row at the bottom
    fy, fh = 348, 80
    tiles = [("Morgen früh", "11°", "20 %", "04d"),
             ("Nachmittag", "20°", "10 %", "02d"),
             ("Morgen", "18°", "60 %", "10d")]
    tw = (W - 320) // 3
    for i, (lab, t, pop, ic) in enumerate(tiles):
        bx = 24 + i * tw
        rrect(d, [bx + 6, fy, bx + tw - 6, fy + fh], radius=10,
              fill=CARD, outline=DIV, width=1)
        text_lm(d, bx + 18, fy + 20, lab, font("segoeuib", 14), SUB)
        text_lm(d, bx + 18, fy + 48, t, font("segoeuib", 22), TITLE)
        paste_icon(img, ic, bx + tw - 56, fy + 14, 50)
        text_rm(d, bx + tw - 14, fy + fh - 12, "💧 " + pop,
                font("segoeui", 13), (60, 120, 200))

    # Buttons at bottom right (compact)
    by = 452
    rrect(d, [W - 480, by - 18, W - 264, by + 18], radius=18, fill=(245, 230, 210),
          outline=DIV, width=1)
    text_mm(d, W - 372, by, "Schlummern", font("segoeuib", 18), TITLE)
    rrect(d, [W - 252, by - 18, W - 24, by + 18], radius=18, fill=ACCENT)
    text_mm(d, W - 138, by, "Stop", font("segoeuib", 18), (255, 255, 255))

    img.save(os.path.join(OUT_DIR, "alarm_screen_3.png"))
    print("alarm_3 done")


if __name__ == "__main__":
    setup_1()
    setup_2()
    setup_3()
    alarm_1()
    alarm_2()
    alarm_3()
    print("\nAll mockups generated under", OUT_DIR)
