# Custom Modes event banner generator — see docs/CUSTOM-MODES.md section 5.
# Usage: edit SRC + the crop band, run with python3, outputs 592x264 and 296x132 PNGs.
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageEnhance
import math, random

SRC = "/mnt/ssd/studio/d21-race.png"
W, H = 592, 264          # 2x the in-game banner (296x132)
OUT = "/mnt/ssd/studio"

FONT_BOLD = "/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Regular.ttf"

def base_crop():
    im = Image.open(SRC).convert("RGB")
    # take a 2.24:1 band through the car pack, skipping the HUD
    target_ar = W / H
    h = int(im.height * 0.36)
    w = int(h * target_ar)
    if w > im.width:
        w = im.width
        h = int(w / target_ar)
    x = (im.width - w) // 2
    y = int(im.height * 0.29)
    y = max(0, min(im.height - h, y))
    return im.crop((x, y, x + w, y + h)).resize((W, H), Image.LANCZOS)

def vignette(img, strength=0.85):
    mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(mask)
    for i in range(60):
        a = int(255 * (i / 60) ** 1.6)
        d.ellipse([-W * 0.35 + i * 4, -H * 0.6 + i * 3, W * 1.35 - i * 4, H * 1.6 - i * 3],
                  fill=255 - a)
    mask = mask.filter(ImageFilter.GaussianBlur(24))
    black = Image.new("RGB", (W, H), (0, 0, 0))
    return Image.composite(img, black, mask.point(lambda v: 255 - int(v * strength)))

def tint(img, colour, amount):
    layer = Image.new("RGB", img.size, colour)
    return Image.blend(img, layer, amount)

def bolt(draw, x, y, scale, colour):
    pts = [(0, 0), (-0.22, 0.55), (-0.02, 0.55), (-0.30, 1.18),
           (0.10, 0.62), (-0.10, 0.62), (0.18, 0.0)]
    poly = [(x + px * scale, y + py * scale) for px, py in pts]
    draw.polygon(poly, fill=colour)

def wordmark(img, title, sub, colour=(255, 255, 255), accent=(240, 200, 60), x=26, y=None):
    d = ImageDraw.Draw(img)
    f_title = ImageFont.truetype(FONT_BOLD, 46)
    f_sub = ImageFont.truetype(FONT_REG, 17)
    tw = d.textlength(title, font=f_title)
    th = 46
    if y is None:
        y = H - 62
    # shadow
    d.text((x + 2, y + 2), title, font=f_title, fill=(0, 0, 0))
    d.text((x, y), title, font=f_title, fill=colour)
    # accent rule
    d.rectangle([x + 2, y + th + 6, x + 12 + tw * 0.55, y + th + 9], fill=accent)
    d.text((x + 2, y + th + 14), sub, font=f_sub, fill=(225, 228, 235))

def save(img, name):
    img.save(f"{OUT}/{name}.png")
    print("saved", name)

base = base_crop()

# A — STORM
a = base.filter(ImageFilter.GaussianBlur(0.6))
a = ImageEnhance.Contrast(a).enhance(1.15)
a = tint(a, (30, 52, 96), 0.42)
a = vignette(a, 0.9)
d = ImageDraw.Draw(a, "RGBA")
bolt(d, W * 0.80, -10, 150, (200, 225, 255, 235))
bolt(d, W * 0.72, 30, 90, (150, 190, 255, 160))
wordmark(a, "ELIMINATION", "LAST CAR OUT  ·  NO FINISH LINE", accent=(120, 180, 255))
save(a, "banner_A_storm")

# B — INFERNO
b = base.filter(ImageFilter.GaussianBlur(1.2))
b = ImageEnhance.Contrast(b).enhance(1.25)
b = tint(b, (120, 30, 20), 0.38)
b = vignette(b, 0.95)
d = ImageDraw.Draw(b, "RGBA")
for i in range(30):
    x0 = random.randint(0, W)
    d.line([(x0, H), (x0 + random.randint(-90, 90), random.randint(0, H // 3))],
           fill=(255, random.randint(90, 150), 50, random.randint(6, 14)), width=random.randint(1, 2))
b = b.filter(ImageFilter.GaussianBlur(0.5))
wordmark(b, "ELIMINATION", "SURVIVE  ·  EVERY LAP", accent=(255, 140, 40))
save(b, "banner_B_inferno")

# C — GT CLEAN
c = ImageEnhance.Color(base).enhance(0.55)
c = ImageEnhance.Contrast(c).enhance(1.10)
c = vignette(c, 0.75)
d = ImageDraw.Draw(c, "RGBA")
d.rectangle([0, 0, W, 46], fill=(10, 12, 18, 190))
d.rectangle([0, H - 44, W, H], fill=(10, 12, 18, 200))
f_tag = ImageFont.truetype(FONT_REG, 14)
d.text((18, 14), "CUSTOM MODE", font=f_tag, fill=(255, 255, 255, 210))
d.text((W - 92, 14), "GT4", font=ImageFont.truetype(FONT_BOLD, 16), fill=(255, 255, 255, 190))
f_t = ImageFont.truetype(FONT_BOLD, 40)
d.text((18, H - 74), "ELIMINATION", font=f_t, fill=(255, 255, 255, 255))
d.rectangle([18, H - 28, 78, H - 25], fill=(235, 60, 50, 255))
d.text((88, H - 32), "LAST PLACE IS OUT", font=ImageFont.truetype(FONT_REG, 15), fill=(230, 230, 235, 220))
save(c, "banner_C_gt")

# small (in-game size) previews
for n in ["banner_A_storm", "banner_B_inferno", "banner_C_gt"]:
    im = Image.open(f"{OUT}/{n}.png").resize((296, 132), Image.LANCZOS)
    im.save(f"{OUT}/{n}_296.png")
print("done")
