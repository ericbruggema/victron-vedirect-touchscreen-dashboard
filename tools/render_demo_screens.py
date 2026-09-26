"""Illustrative renders of the dashboard pages (NOT photos of the real screen).

Re-implements the layout math from widgets.h / pages_*.h with the demo values
below and draws it with Pillow at 3x. Run: python tools/render_demo_screens.py
Output: docs/screenshots/*.png
"""
import math, os
from PIL import Image, ImageDraw, ImageFont

S = 3
W, H = 320, 240
FONTDIR = "C:/Windows/Fonts/"


def F(px):
    for n in ("arial.ttf", "ARIAL.TTF"):
        try:
            return ImageFont.truetype(FONTDIR + n, int(px * S))
        except OSError:
            pass
    return ImageFont.load_default()


FONTS = {1: F(8.5), 2: F(12.5), 4: F(21), 3: F(42)}   # 3 = font4 at size 2
HEIGHT = {1: 8, 2: 16, 4: 26, 3: 52}

C = dict(bg=(10, 13, 16), card=(20, 24, 29), border=(38, 44, 51), text=(232, 236, 239),
         muted=(139, 149, 161), amber=(245, 166, 35), green=(46, 204, 143), coral=(255, 107, 74),
         amberMid=(240, 168, 63), red=(255, 77, 77), blue=(77, 163, 255), purple=(127, 119, 221))
VBLUE = (0, 105, 170)
CONTENT_TOP, CONTENT_BOTTOM = 28, H - 16

# ---- demo data (sunny afternoon, LiFePO4 12V / 300Ah) ----
soc, pvW, pvV, pvA = 78, 240, 62.3, 3.9
battV, battA = 13.3, 4.6
battW = battV * battA
loadW = int(pvW - battW)
pvPeak, loadPeak = 612, 412
pvHour = [0, 0, 0, 0, 0, 0, 12, 48, 110, 190, 260, 320, 340, 300, 240, 180, 90, 30, 0, 0, 0, 0, 0, 0]
socHour = [62, 60, 59, 58, 57, 56, 57, 60, 64, 68, 71, 74, 76, 78, 78, 77, 76, 75, 74, 72, 70, 68, 66, 64]
week = [3.2, 4.1, 2.6, 5.0, 4.4, 1.9, 3.8]


def load_color(w):
    return C['green'] if w < 80 else C['amber'] if w < 150 else C['amberMid'] if w < 250 else C['red']


def soc_color(s):
    return C['green'] if s > 20 else C['amber'] if s > 10 else C['red']


def fmtW(w):
    return f"{w/1000:.1f}kW" if abs(w) >= 1000 else f"{int(w)}W"


OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "screenshots")
os.makedirs(OUT, exist_ok=True)


class Screen:
    def __init__(self):
        self.img = Image.new("RGB", (W * S, H * S), C['bg'])
        self.d = ImageDraw.Draw(self.img)

    def rect(self, x, y, w, h, col):
        self.d.rectangle([x * S, y * S, (x + w) * S - 1, (y + h) * S - 1], fill=col)

    def rrect(self, x, y, w, h, r, fill=None, outline=None):
        self.d.rounded_rectangle([x * S, y * S, (x + w) * S - 1, (y + h) * S - 1], radius=r * S,
                                 fill=fill, outline=outline, width=S)

    def circle(self, cx, cy, r, col, fill=True):
        b = [(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S]
        self.d.ellipse(b, fill=col if fill else None, outline=None if fill else col, width=S)

    def line(self, x1, y1, x2, y2, col, w=1):
        self.d.line([x1 * S, y1 * S, x2 * S, y2 * S], fill=col, width=w * S)

    def tri(self, a, b, c, col):
        self.d.polygon([(a[0] * S, a[1] * S), (b[0] * S, b[1] * S), (c[0] * S, c[1] * S)], fill=col)

    def arc(self, cx, cy, r, thick, start, end, col):
        # TFT_eSPI: 0 deg = 6 o'clock, clockwise; PIL: 0 = 3 o'clock, clockwise
        b = [(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S]
        if end - start >= 360:
            self.d.ellipse(b, outline=col, width=int(thick * S))
            return
        self.d.arc(b, 90 + start, 90 + end, fill=col, width=int(thick * S))

    def tw(self, s, f):
        return self.d.textlength(s, font=FONTS[f]) / S

    def text(self, x, y, s, f, col, datum="TL"):
        w = self.tw(s, f)
        h = HEIGHT[f]
        vx = {"L": x, "C": x - w / 2, "R": x - w}[datum[1]]
        vy = {"T": y, "M": y - h / 2, "B": y - h}[datum[0]]
        self.d.text((vx * S, vy * S), s, font=FONTS[f], fill=col, anchor="lt")

    def save(self, name):
        self.img.save(os.path.join(OUT, name), optimize=True)


# ---- icons ----
def icon_sun(s, cx, cy, r, col):
    s.circle(cx, cy, r * 0.55, col)
    for a in range(0, 360, 45):
        rd = math.radians(a)
        s.line(cx + math.cos(rd) * r * .78, cy + math.sin(rd) * r * .78,
               cx + math.cos(rd) * r, cy + math.sin(rd) * r, col)


def icon_battery(s, cx, cy, r, col):
    w, h = int(r * 1.6), int(r * 1.1)
    x, y = cx - w // 2, cy - h // 2
    s.rrect(x, y, w, h, 2, outline=col)
    s.rrect(x + 1, y + 1, w - 2, h - 2, 2, outline=col)
    s.rect(x + w, y + h // 4, int(r * .18) + 1, h // 2, col)


def icon_bolt(s, cx, cy, r, col):
    s.tri((cx + r * .15, cy - r), (cx - r * .5, cy + r * .15), (cx + r * .05, cy + r * .15), col)
    s.tri((cx - r * .15, cy + r), (cx + r * .5, cy - r * .15), (cx - r * .05, cy - r * .15), col)


def icon_arrow(s, cx, cy, r, col):
    s.line(cx - r, cy, cx + r * .4, cy, col)
    s.line(cx - r, cy + 1, cx + r * .4, cy + 1, col)
    s.tri((cx + r * .4, cy - r * .5), (cx + r * .4, cy + r * .5), (cx + r, cy), col)


def icon_chevleft(s, cx, cy, r, col):
    for o in (0, -1):
        s.line(cx + r * .3 + o, cy - r, cx - r * .5 + o, cy, col)
        s.line(cx - r * .5 + o, cy, cx + r * .3 + o, cy + r, col)


def icon_grid(s, cx, cy, r, col):
    sz = max(2, r // 3)
    for i in (-1, 1):
        for j in (-1, 1):
            s.rect(cx + i * r / 2 - sz / 2, cy + j * r / 2 - sz / 2, sz, sz, col)


def icon_clock(s, cx, cy, r, col):
    s.circle(cx, cy, r, col, fill=False)
    s.line(cx, cy, cx, cy - r * .6, col)
    s.line(cx, cy, cx + r * .5, cy, col)


# ---- chrome ----
def status_bar(s):
    s.rect(0, 0, W, 28, C['bg'])
    s.line(0, 27, W, 27, C['border'])
    icon_grid(s, W - 16, 14, 7, C['muted'])
    icon_clock(s, 14, 14, 7, C['muted'])
    s.text(26, 14, "3u 42m", 2, C['muted'], "ML")
    xr = W - 16 - 14
    s.text(xr, 14, "MPPT", 2, C['muted'], "MR")
    dx = xr - s.tw("MPPT", 2) - 10
    s.circle(dx, 14, 3, C['green'])
    xr = dx - 12
    s.text(xr, 14, "SHUNT", 2, C['muted'], "MR")
    dx = xr - s.tw("SHUNT", 2) - 10
    s.circle(dx, 14, 3, C['green'])


def sub_header(s, title, grid=True):
    s.rect(0, 0, W, 28, C['bg'])
    s.line(0, 27, W, 27, C['border'])
    icon_chevleft(s, 16, 14, 6, C['muted'])
    s.text(30, 14, title, 2, C['text'], "ML")
    if grid:
        icon_grid(s, W - 16, 14, 7, C['muted'])


def nav_dots(s, active):
    n, sp = 7, 14
    sx = W // 2 - ((n - 1) * sp) // 2
    y = CONTENT_BOTTOM + (H - CONTENT_BOTTOM) // 2
    for i in range(n):
        s.circle(sx + i * sp, y, 3, C['text'] if i == active else C['border'])


# ---- widgets ----
def w_row(s, x, y, w, label, value, col):
    s.text(x, y + 2, label, 2, C['muted'], "TL")
    f = 1 if s.tw(label, 2) + s.tw(value, 2) + 8 > w else 2
    s.text(x + w, y + 2 if f == 2 else y + 5, value, f, col, "TR")


def stat_tile(s, x, y, w, h, label, value, col):
    s.rrect(x, y, w, h, 8, fill=C['card'], outline=C['border'])
    s.text(x + w / 2, y + 6, label, 2, C['muted'], "TC")
    f = 2 if s.tw(value, 4) > w - 10 else 4
    s.text(x + w / 2, y + h / 2 + 10, value, f, col, "MC")


def pill(s, x, y, bg, col, txt):
    w = s.tw(txt, 2) + 20
    s.rrect(x, y, w, 20, 10, fill=bg)
    s.text(x + w / 2, y + 10, txt, 2, col, "MC")


def big_ring(s, cx, cy, r, sw, col, pct, big, small):
    s.arc(cx, cy, r, sw, 0, 360, C['border'])
    s.arc(cx, cy, r, sw, 0, 3.6 * pct, col)
    s.circle(cx, cy, r - sw - 2, C['bg'])
    s.text(cx, cy - 8, big, 4, C['text'], "MC")
    s.text(cx, cy + 16, small, 2, C['muted'], "MC")


def bar_graph(s, x, y, w, h, vals, color, hi=-1, hicol=None):
    mx = max(max(vals), 0.001)
    n = len(vals)
    gap = 3
    bw = max(1, (w - gap * (n - 1)) // n)
    for i, v in enumerate(vals):
        bh = int(v / mx * (h - 2))
        if bh < 1 and v > 0:
            bh = 1
        if bh > 0:
            s.rect(x + i * (bw + gap), y + h - bh, bw, bh, hicol if i == hi else color)


def sparkline(s, x, y, w, h, vals, col):
    pts = [(x + i / (len(vals) - 1) * (w - 1), y + h - 1 - max(0, min(100, v)) / 100 * (h - 2))
           for i, v in enumerate(vals)]
    for a, b in zip(pts, pts[1:]):
        s.line(a[0], a[1], b[0], b[1], col)


def seg_bar(s, x, y, w, h, fr, cols):
    cx = x
    for i, f in enumerate(fr):
        seg = (x + w - cx) if i == len(fr) - 1 else int(f * w)
        s.rect(cx, y, seg, h, cols[i])
        cx += seg


def toggle_seg(s, x, y, a, b, sel):
    wa, wb = s.tw(a, 2) + 24, s.tw(b, 2) + 24
    s.rrect(x, y, wa + wb, 24, 6, fill=C['card'])
    s.rrect(x if sel == 0 else x + wa, y, wa if sel == 0 else wb, 24, 6, fill=C['blue'])
    s.text(x + wa / 2, y + 12, a, 2, C['bg'] if sel == 0 else C['muted'], "MC")
    s.text(x + wa + wb / 2, y + 12, b, 2, C['bg'] if sel == 1 else C['muted'], "MC")


def card(s, x, y, w, h, label, big, sub, footL, footV, accent, ringPct, icon):
    ringR = max(24, min(40, min(w, h) // 2 - 26))
    cx = x + w // 2
    iconShift, valueShift = 4, 2
    cy = y + 14 + ringR - iconShift
    anchor = (y + 14 + ringR) + ringR + 2
    labelY = anchor + 2 - iconShift
    bigY = anchor + 16 - valueShift + 3 + 4
    subY = anchor + 42 - valueShift + 3 + 2 + 4
    fy = (y + h) - 24 - iconShift - 3 - 3 - 5
    s.rrect(x, y, w, h, 10, fill=C['card'], outline=C['border'])
    if ringPct >= 0:
        s.arc(cx, cy, ringR, 6, 0, 360, C['border'])
        if ringPct > 0:
            s.arc(cx, cy, ringR, 6, 0, 3.6 * ringPct, accent)
    icon(s, cx, cy, ringR - 14, accent)
    s.text(cx, labelY, label, 2, C['muted'], "TC")
    s.text(cx, bigY, big, 4, C['text'], "TC")
    s.text(cx, subY, sub, 2, C['muted'], "TC")
    s.line(x + 10, fy - 4, x + w - 10, fy - 4, C['border'])
    s.text(cx, fy + 2, footL, 1, C['muted'], "TC")
    s.text(cx, fy + 12, footV, 2, C['text'], "TC")


# ---- pages ----
def page_dashboard():
    s = Screen()
    status_bar(s)
    y = CONTENT_TOP + 1
    h = CONTENT_BOTTOM - y - 1
    hours = (300 * (100 - soc) / 100) / battA
    card(s, 8, y, 94, h, "ZON", fmtW(pvW), f"{pvV:.1f}V . {pvA:.1f}A", "Piek 24u", fmtW(pvPeak),
         C['amber'], pvW * 100 // pvPeak, icon_sun)
    card(s, 112, y, 94, h, "ACCU", f"{soc}%", f"{battV:.1f}V . {battA:.1f}A", "Tot vol",
         f"{int(hours)}u{int((hours - int(hours)) * 60)}m", soc_color(soc), soc, icon_battery)
    card(s, 216, y, 94, h, "VERBRUIK", fmtW(loadW), f"{loadW / battV:.1f}A", "Piek 24u", fmtW(loadPeak),
         load_color(loadW), min(100, loadW * 100 // 250), icon_bolt)
    icon_arrow(s, 104, CONTENT_TOP + 45, 8, C['amber'])
    icon_arrow(s, 208, CONTENT_TOP + 45, 8, load_color(loadW))
    nav_dots(s, 0)
    return s


def page_zon():
    s = Screen()
    sub_header(s, "ZON & DETAILS")
    icon_sun(s, 60, CONTENT_TOP + 38, 22, C['amber'])
    s.text(60, CONTENT_TOP + 66, fmtW(pvW), 3, C['amber'], "TC")
    s.text(28, CONTENT_TOP + 122, "vermogen", 2, C['muted'])
    rx, rw = 150, W - 8 - 150
    w_row(s, rx, CONTENT_TOP + 30, rw, "Stroom", f"{pvA:.1f} A", C['text'])
    w_row(s, rx, CONTENT_TOP + 58, rw, "Spanning", f"{pvV:.1f} V", C['text'])
    w_row(s, rx, CONTENT_TOP + 86, rw, "Laatste 24u", f"{sum(pvHour) / 1000:.1f} kWh", C['text'])
    gy, gh = CONTENT_TOP + 142, 26
    bar_graph(s, 8, gy, 304, gh, pvHour, C['amber'], -1)
    cy = gy + gh + 4
    s.text(8, cy, "-24u", 2, C['muted'])
    s.text(148, cy, "PV per uur", 2, C['muted'])
    s.text(294, cy, "nu", 2, C['muted'])
    nav_dots(s, 1)
    return s


def page_accu():
    s = Screen()
    sub_header(s, "ACCU DETAILS")
    big_ring(s, 90, CONTENT_TOP + 62, 50, 9, soc_color(soc), soc, f"{soc}%", "SoC")
    rx, rw, ry = 185, 312 - 185, CONTENT_TOP + 4
    hours = (300 * (100 - soc) / 100) / battA
    w_row(s, rx, ry, rw, "Spanning", f"{battV:.1f} V", C['text'])
    ry += 22
    w_row(s, rx, ry, rw, "Stroom", f"{battA:.1f} A", C['green'])
    ry += 22
    w_row(s, rx, ry, rw, "Tot vol", f"{int(hours)}u {int((hours - int(hours)) * 60)}m", C['text'])
    ry += 22
    w_row(s, rx, ry, rw, "Accu 1/2", "13.3V / 13.3V", C['green'])
    ry += 22 + 6
    pill(s, rx, ry, C['amber'], C['bg'], "Bulk")
    sy = CONTENT_TOP + 128
    sparkline(s, 8, sy, 304, 32, socHour, C['green'])
    cy = sy + 32 + 2
    s.text(8, cy, "-24u", 2, C['muted'])
    s.text(150, cy, "SoC (rollend 24u)", 2, C['muted'])
    s.text(270, cy, "nu", 2, C['muted'])
    nav_dots(s, 2)
    return s


def page_energie():
    s = Screen()
    sub_header(s, "ENERGIE")
    m, g = 8, 8
    tw_ = (W - 2 * m - 2 * g) // 3
    th = ((CONTENT_BOTTOM - CONTENT_TOP) - 2 * m - g) // 2
    cols = [m, m + tw_ + g, m + 2 * (tw_ + g)]
    r1 = CONTENT_TOP + m
    r2 = r1 + th + g
    direct = pvW - battW
    tiles = [("direct zon", f"{int(direct * 100 / pvW)}%", C['text']),
             ("zelfconsumptie", f"{int(direct * 100 / loadW)}%", C['text']),
             ("naar accu", fmtW(int(battW)), C['green']), ("uit accu", "0W", C['text']),
             ("netto accu", "+" + fmtW(int(battW)), C['green']),
             ("zon - verbruik", "+" + fmtW(pvW - loadW), C['green'])]
    for i, (l, v, c) in enumerate(tiles):
        stat_tile(s, cols[i % 3], r1 if i < 3 else r2, tw_, th, l, v, c)
    nav_dots(s, 3)
    return s


def page_historie():
    s = Screen()
    sub_header(s, "DAG & HISTORIE")
    m, g = 8, 6
    tw_ = (W - 2 * m - 3 * g) // 4
    ty = CONTENT_TOP + 4
    for i, (l, v, c) in enumerate([("opbrengst", "2.2 kWh", C['text']), ("verbruik", "3.1 kWh", C['text']),
                                   ("geladen", "1.4 kWh", C['green']), ("ontladen", "2.3 kWh", C['coral'])]):
        stat_tile(s, m + i * (tw_ + g), ty, tw_, 54, l, v, c)
    iy = CONTENT_TOP + 66
    w_row(s, 8, iy, 104, "Accu", "12.8-13.5V", C['text'])
    w_row(s, 120, iy, 104, "I", "-14.2..21.5A", C['text'])
    w_row(s, 230, iy, 82, "Piek", fmtW(loadPeak), C['text'])
    by = CONTENT_TOP + 90
    seg_bar(s, 8, by, 304, 16, [.28, .22, .50], [C['amber'], C['amberMid'], C['green']])
    ly = by + 20
    s.text(8, ly, "bulk", 2, C['muted'])
    s.text(140, ly, "absorptie", 2, C['muted'])
    s.text(280, ly, "float", 2, C['muted'])
    gy = CONTENT_TOP + 126
    bar_graph(s, 8, gy, 304, 32, week, C['purple'], week.index(max(week)), C['text'])
    cy = gy + 32 + 4
    s.text(8, cy, "D-6", 2, C['muted'])
    s.text(290, cy, "D0", 2, C['muted'])
    s.text(160, cy, "beste: D-3 5.0 kWh", 2, C['muted'], "TC")
    nav_dots(s, 4)
    return s


def page_apparaten():
    s = Screen()
    sub_header(s, "APPARATEN")
    s.line(160, CONTENT_TOP + 4, 160, CONTENT_TOP + 4 + CONTENT_BOTTOM - CONTENT_TOP - 8, C['border'])
    iy = CONTENT_TOP + 16
    icon_sun(s, 24, iy, 10, C['amber'])
    s.text(42, iy - 8, "MPPT", 2, C['text'])
    s.text(8, CONTENT_TOP + 28, "SmartSolar 100/30", 2, C['muted'])
    s.text(8, CONTENT_TOP + 42, "FW v1.61", 2, C['muted'])
    ry = CONTENT_TOP + 62
    for i, (l, v) in enumerate([("max laadstroom", "30.0 A"), ("absorptie", "14.4 V"), ("float", "13.5 V")]):
        w_row(s, 8, ry + 22 * i, 144, l, v, C['text'])
    icon_battery(s, 184, iy, 10, C['green'])
    s.text(202, iy - 8, "SHUNT", 2, C['text'])
    s.text(168, CONTENT_TOP + 28, "SmartShunt 500A", 2, C['muted'])
    s.text(168, CONTENT_TOP + 42, "FW v4.19", 2, C['muted'])
    for i, (l, v) in enumerate([("capaciteit", "300 Ah"), ("charged v", "14.2 V"), ("peukert", "1.05")]):
        w_row(s, 168, ry + 22 * i, 144, l, v, C['text'])
    nav_dots(s, 5)
    return s


def page_systeem():
    s = Screen()
    sub_header(s, "SYSTEEM")
    y0 = CONTENT_TOP + 12
    left = [("uptime", "3u 42m", C['text']), ("vrij RAM", "182 KB", C['text']),
            ("NVS", "OK", C['green']), ("systeem", "12V", C['text'])]
    right = [("SD-kaart", "niet gevonden", C['muted']), ("bron", "MPPT+SHUNT live", C['green']),
             ("cycli", "27", C['text']), ("firmware", "1.0-cyd", C['muted'])]
    for i, (l, v, c) in enumerate(left):
        w_row(s, 8, y0 + 30 * i, 140, l, v, c)
    for i, (l, v, c) in enumerate(right):
        w_row(s, 164, y0 + 30 * i, 140, l, v, c)
    nav_dots(s, 6)
    return s


def page_settings():
    s = Screen()
    sub_header(s, "INSTELLINGEN", grid=False)
    y0 = CONTENT_TOP + 10
    for i, t in enumerate(["Helderheid", "Scherm uit na screensaver", "Accuprofiel",
                           "Aanraakscherm kalibreren", "SD-kaart"]):
        s.text(8, y0 + 30 * i, t, 2, C['muted'])
    s.text(290, y0 + 90, ">", 2, C['muted'])
    s.rrect(140, y0 + 4, 160, 6, 3, fill=C['border'])
    s.rrect(140, y0 + 4, 128, 6, 3, fill=C['blue'])
    s.circle(268, y0 + 7, 8, C['text'])
    toggle_seg(s, 200, y0 + 28, "Ja", "Nee", 0)
    toggle_seg(s, 130, y0 + 58, "LiFePO4", "Lood", 0)
    s.text(300, y0 + 120, "niet aangesloten", 2, C['muted'], "TR")
    return s


def page_boot():
    s = Screen()
    s.text(W / 2, 26, "ENERGIE MONITOR", 4, C['text'], "TC")
    s.text(W / 2, 52, "gemaakt door Eric Bruggema", 1, C['muted'], "TC")
    s.text(W / 2, 74, "zoeken: VE.Direct", 2, VBLUE, "TC")
    bw, bh, bx, by = 140, 26, W // 2 - 70, 100
    s.rrect(bx, by, bw, bh, 4, outline=C['text'])
    s.rect(bx + 2, by + 2, int((bw - 4) * .42), bh - 4, C['amber'])
    s.text(W / 2, by + bh + 12, "9", 4, VBLUE, "TC")
    cy0, vh = by + bh + 38, 72
    s.line(8, cy0 - 4, W - 8, cy0 - 4, C['border'])
    lines = [("MPPT LADER", VBLUE), (" TX  ->  GPIO 35", C['text']), (" RX  ->  GPIO 27", C['text']),
             (" GND ->  GND", C['muted']), ("", 0), ("BATTERY SHUNT", VBLUE),
             (" TX  ->  GPIO 22", C['text']), (" RX  ->  GPIO 27", C['text']), (" GND ->  GND", C['muted'])]
    for i, (t, c) in enumerate(lines):
        ly = cy0 + 2 + i * 14 - 14
        if ly < cy0 or ly > cy0 + vh - 8 or not t:
            continue
        s.text(W / 2, ly, t, 1, C['muted'] if ly < cy0 + vh / 3 else c, "TC")
    return s


def page_saver():
    s = Screen()
    gx, gy = 60, 46
    sx, sy = gx + 30, gy + 40
    hx, hy = gx + 170, gy + 40
    bx, by = gx + 100 - 17, gy + 74
    icon_sun(s, sx, sy, 13, C['amber'])
    s.circle(sx, sy, 8, C['amber'])
    s.rect(hx - 8, hy - 2, 16, 12, C['muted'])
    s.tri((hx - 10, hy - 2), (hx + 10, hy - 2), (hx, hy - 12), C['muted'])
    s.rect(bx + 13, by - 3, 8, 3, C['text'])
    s.rrect(bx, by, 34, 26, 0, outline=C['text'])
    s.rect(bx + 1, by + 1, 32, 24, C['card'])
    fh = int(24 * soc / 100)
    s.rect(bx + 1, by + 25 - fh, 32, fh, soc_color(soc))
    nx, ny = gx + 100, gy + 40
    for k in range(3):
        f = (0.2 + k / 3) % 1
        s.circle(sx + 16 + (nx - 3 - sx - 16) * f, ny, 2, C['amber'])
        s.circle(nx + 3 + (hx - 16 - nx - 3) * f, ny, 2, load_color(loadW))
        s.circle(nx, ny + 4 + (by - ny - 8) * f, 2, C['green'])
    s.text(sx, sy + 16, fmtW(pvW), 2, C['amber'], "TC")
    s.text(hx, hy + 16, fmtW(loadW), 2, C['text'], "TC")
    s.text(bx + 17, by + 30, "+" + fmtW(int(battW)), 2, C['green'], "TC")
    s.text(bx + 17, by + 46, f"{soc}%", 2, soc_color(soc), "TC")
    return s


if __name__ == "__main__":
    for name, fn in [("01-dashboard", page_dashboard), ("02-zon", page_zon), ("03-accu", page_accu),
                     ("04-energie", page_energie), ("05-historie", page_historie),
                     ("06-apparaten", page_apparaten), ("07-systeem", page_systeem),
                     ("08-instellingen", page_settings), ("09-opstart", page_boot),
                     ("10-screensaver", page_saver)]:
        fn().save(name + ".png")
        print("wrote", name)
