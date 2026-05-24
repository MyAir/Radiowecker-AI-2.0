from PIL import Image, ImageDraw, ImageFont
import os

W, H = 800, 480
FONT_DIR = "C:/Windows/Fonts/"

def font(name, size):
    return ImageFont.truetype(FONT_DIR + name + ".ttf", size)

def centered_text(draw, y, text, f, color, img_w=W):
    draw.text((img_w // 2, y), text, font=f, fill=color, anchor="mm")

def right_text(draw, x, y, text, f, color):
    draw.text((x, y), text, font=f, fill=color, anchor="rm")

def left_text(draw, x, y, text, f, color):
    draw.text((x, y), text, font=f, fill=color, anchor="lm")

# ─────────────────────────────────────────────────
# DESIGN 1 — "Warm Amber" (v2)
# - Day name then date stacked at top
# - Massive time below
# - Alarm row with Skip button
# - Brighter/larger sensor strip
# ─────────────────────────────────────────────────
A_BRIGHT = (200, 110, 15)   # amber — time
A_MED    = (160,  90, 12)   # amber — date / day
A_DIM    = (110,  60,  8)   # amber — alarm label
A_ALARM  = (190, 105, 14)   # amber — alarm value

img1 = Image.new("RGB", (W, H), (0, 0, 0))
d = ImageDraw.Draw(img1)

# Status bar
d.rectangle([0, 0, W, 26], fill=(18, 18, 18))
left_text(d, 10, 13, "WiFi: MyNetwork", font("segoeui", 13), (100, 100, 100))
right_text(d, W-10, 13, "192.168.1.42  |  87%", font("segoeui", 13), (100, 100, 100))

# Day name + date stacked
centered_text(d,  75, "DONNERSTAG",        font("segoeuib", 22), A_DIM)
centered_text(d, 112, "31. Dezember 2025", font("segoeui",  30), A_MED)

# Time — massive
centered_text(d, 268, "23:47", font("segoeuib", 200), A_BRIGHT)

# Separator
d.line([(180, 392), (620, 392)], fill=(80, 45, 5), width=1)

# Alarm row — label left-ish, value centered, Skip button right
ALARM_Y = 428
alarm_label = "Nachster Alarm:"
alarm_value = "Mo  02.01.2026   06:30"
BTN_W, BTN_H = 90, 34
BTN_X = W - 60 - BTN_W   # right-aligned with 60px margin
BTN_Y = ALARM_Y - BTN_H // 2

# Content area width (excluding skip button zone)
content_right = BTN_X - 16
d.text((W // 2 - 60, ALARM_Y), alarm_label, font=font("segoeui", 20), fill=A_DIM,    anchor="rm")
d.text((W // 2 - 44, ALARM_Y), alarm_value, font=font("segoeuib", 22), fill=A_ALARM, anchor="lm")

# Skip button — rounded rect outline + label
d.rounded_rectangle([BTN_X, BTN_Y, BTN_X + BTN_W, BTN_Y + BTN_H],
                    radius=6, outline=(100, 55, 8), width=1)
d.text((BTN_X + BTN_W // 2, ALARM_Y), "Skip", font=font("segoeui", 17), fill=(120, 68, 10), anchor="mm")

# Sensor strip — brighter and larger
SENSOR_H = 36
d.rectangle([0, H - SENSOR_H, W, H], fill=(14, 14, 14))
d.line([(0, H - SENSOR_H), (W, H - SENSOR_H)], fill=(45, 25, 3), width=1)
sx = W // 8
for i, (lbl, val) in enumerate([("TEMP","21.3 oC"),("FEUCHTE","54 %"),("CO2","812 ppm"),("TVOC","45 ppb")]):
    x = sx + i * (W // 4)
    d.text((x, H - SENSOR_H + 9),  lbl, font=font("segoeui",  12), fill=(110, 65, 10), anchor="mm")
    d.text((x, H - SENSOR_H + 25), val, font=font("segoeuib", 14), fill=(160, 95, 14), anchor="mm")

img1.save("mockup_1_warm_amber.png")
print("Saved mockup 1")

# ─────────────────────────────────────────────────
# DESIGN 2 — "Cool Night"
# Blue-white on deep navy, date above time, clean lines
# ─────────────────────────────────────────────────
BG2    = (4, 8, 20)
DIM    = (40, 70, 140)
TEXT   = (140, 170, 230)
BRIGHT = (190, 210, 255)

img2 = Image.new("RGB", (W, H), BG2)
d = ImageDraw.Draw(img2)

d.rectangle([0, 0, W, 30], fill=(8, 14, 35))
left_text(d, 10, 15, "WiFi: MyNetwork", font("segoeui", 13), (50, 70, 120))
right_text(d, W-10, 15, "192.168.1.42   87%", font("segoeui", 13), (50, 70, 120))

centered_text(d, 85, "Donnerstag, 31. Dezember 2025", font("segoeui", 26), DIM)
d.line([(160, 108), (640, 108)], fill=(30, 55, 110), width=1)

centered_text(d, 248, "23:47", font("segoeuib", 200), BRIGHT)

d.line([(160, 385), (640, 385)], fill=(30, 55, 110), width=1)
centered_text(d, 418, "Nachster Alarm  -  Mo 02.01.2026  -  06:30", font("segoeui", 24), TEXT)

d.text((20, H-14), "21.3 oC  -  54 %  -  CO2 812  -  TVOC 45",
       font=font("segoeui", 12), fill=(35, 55, 95), anchor="lm")

img2.save("mockup_2_cool_night.png")
print("Saved mockup 2")

# ─────────────────────────────────────────────────
# DESIGN 3 — "Ember Glow"
# Off-center layout, rotated day-name shadow left column, right-heavy time
# ─────────────────────────────────────────────────
BG3   = (6, 2, 2)
RED1  = (160, 30, 20)
RED2  = (100, 20, 12)
RED3  = (55,  12,  8)
GOLD  = (160, 100, 20)

img3 = Image.new("RGB", (W, H), BG3)
d = ImageDraw.Draw(img3)

d.rectangle([0, 0, W, 24], fill=(12, 4, 4))
left_text(d, 10, 12, "WiFi: MyNetwork", font("segoeui", 12), (70, 30, 28))
right_text(d, W-10, 12, "192.168.1.42  87%", font("segoeui", 12), (70, 30, 28))

day_img = Image.new("RGBA", (200, 600), (0, 0, 0, 0))
dd = ImageDraw.Draw(day_img)
dd.text((10, 10), "DONNERSTAG", font=font("segoeuib", 48), fill=(40, 10, 8, 255))
day_rot = day_img.rotate(90, expand=True)
img3.paste(day_rot, (-20, 80), mask=day_rot.split()[3])

d.line([(150, 30), (150, H-32)], fill=(35, 10, 8), width=1)

CONTENT_X = 230
d.text((CONTENT_X, 60), "UHRZEIT", font=font("segoeuib", 13), fill=RED3, anchor="lm")

d.text((W//2 + 30, 248), "23:47", font=font("segoeuib", 200), fill=RED1, anchor="mm")

d.text((W-40, 375), "31. Dezember 2025", font=font("segoeui", 28), fill=RED2, anchor="rm")
d.line([(CONTENT_X, 398), (W-30, 398)], fill=RED3, width=1)

d.text((CONTENT_X, 428), "Nachster Alarm:", font=font("segoeui", 19), fill=RED2, anchor="lm")
d.text((W-40, 428), "Mo 02.01.2026  06:30", font=font("segoeuib", 22), fill=GOLD, anchor="rm")

d.rectangle([0, H-28, W, H], fill=(10, 3, 3))
d.text((W//2, H-14), "21.3 oC   -   54 %   -   CO2 812 ppm   -   TVOC 45 ppb",
       font=font("segoeui", 12), fill=(50, 18, 16), anchor="mm")

img3.save("mockup_3_ember_glow.png")
print("Saved mockup 3")
print("All done.")
