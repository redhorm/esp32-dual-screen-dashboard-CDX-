#!/usr/bin/env python3
"""Generate the concise six-page Ultra4 Microdash beta 4.1 quick guide."""

from __future__ import annotations

from pathlib import Path

from reportlab.graphics import renderPDF
from reportlab.graphics.barcode.qr import QrCodeWidget
from reportlab.graphics.shapes import Drawing
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs" / "assets" / "guide"
OUT = ROOT / "output" / "pdf" / "Guida_Rapida_ESP32_Dual_Screen.pdf"

W, H = A4
M = 42
TOTAL_PAGES = 6
PAPER = colors.HexColor("#F3F0E8")
INK = colors.HexColor("#171512")
DARK = colors.HexColor("#11100E")
PANEL = colors.HexColor("#201C16")
WHITE = colors.white
MUTED = colors.HexColor("#736B60")
LINE = colors.HexColor("#D7D0C5")
ORANGE = colors.HexColor("#EE9E1F")
CYAN = colors.HexColor("#5FF5FF")
MAGENTA = colors.HexColor("#FF2BD6")
LIME = colors.HexColor("#CFFF45")
AMBER = colors.HexColor("#FFC247")
RED = colors.HexColor("#EF4438")


class QuickGuide:
    def __init__(self):
        OUT.parent.mkdir(parents=True, exist_ok=True)
        self.c = canvas.Canvas(str(OUT), pagesize=A4, pageCompression=1)
        self.c.setTitle("Guida rapida Ultra4 Microdash beta 4.1")
        self.c.setAuthor("redhorm / Codex")
        self.c.setSubject("Installazione e uso essenziale")
        self.page = 0
        self.body = ParagraphStyle(
            "body", fontName="Helvetica", fontSize=10.5, leading=14,
            textColor=INK, alignment=TA_LEFT,
        )
        self.small = ParagraphStyle(
            "small", fontName="Helvetica", fontSize=8.7, leading=11.5,
            textColor=MUTED, alignment=TA_LEFT,
        )

    def save(self):
        self.c.save()

    def end(self):
        self.c.showPage()

    def header(self, step: str, title: str, subtitle: str = "") -> float:
        self.page += 1
        c = self.c
        c.setFillColor(PAPER)
        c.rect(0, 0, W, H, fill=1, stroke=0)
        c.setFillColor(ORANGE)
        c.rect(0, H - 9, W, 9, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 9)
        c.drawString(M, H - 38, step.upper())
        c.setFont("Helvetica-Bold", 26)
        c.drawString(M, H - 75, title)
        if subtitle:
            c.setFillColor(MUTED)
            c.setFont("Helvetica", 10)
            c.drawString(M, H - 96, subtitle)
        c.setStrokeColor(LINE)
        c.line(M, H - 111, W - M, H - 111)
        c.setFillColor(MUTED)
        c.setFont("Helvetica", 7)
        c.drawString(M, 23, "ULTRA4 MICRODASH // GUIDA RAPIDA 4.1")
        c.drawRightString(W - M, 23, f"{self.page}/{TOTAL_PAGES}")
        return H - 134

    def paragraph(self, text: str, x: float, y: float, width: float,
                  style: ParagraphStyle | None = None) -> float:
        p = Paragraph(text, style or self.body)
        _, height = p.wrap(width, H)
        p.drawOn(self.c, x, y - height)
        return y - height

    def card(self, x: float, top: float, width: float, height: float,
             title: str, text: str, accent=ORANGE, dark=False):
        c = self.c
        c.setFillColor(PANEL if dark else WHITE)
        c.setStrokeColor(colors.HexColor("#3B352C") if dark else LINE)
        c.roundRect(x, top - height, width, height, 11, fill=1, stroke=1)
        c.setFillColor(accent)
        c.rect(x, top - height, 6, height, fill=1, stroke=0)
        c.setFillColor(WHITE if dark else INK)
        c.setFont("Helvetica-Bold", 10.5)
        c.drawString(x + 17, top - 24, title)
        style = ParagraphStyle(
            "card", parent=self.small, fontSize=9.2, leading=12.3,
            textColor=colors.HexColor("#D0C8BB") if dark else MUTED,
        )
        self.paragraph(text, x + 17, top - 40, width - 32, style)

    def step(self, number: str, title: str, text: str, x: float, top: float,
             width: float, accent=ORANGE) -> float:
        c = self.c
        c.setFillColor(accent)
        c.circle(x + 18, top - 20, 18, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 12)
        c.drawCentredString(x + 18, top - 24, number)
        c.setFont("Helvetica-Bold", 12)
        c.drawString(x + 49, top - 16, title)
        end = self.paragraph(text, x + 49, top - 27, width - 49, self.body)
        return min(top - 49, end - 8)

    def code(self, line: str, x: float, top: float, width: float):
        c = self.c
        c.setFillColor(DARK)
        c.roundRect(x, top - 48, width, 48, 9, fill=1, stroke=0)
        c.setFillColor(ORANGE)
        c.setFont("Courier-Bold", 8.8)
        c.drawString(x + 14, top - 29, line)

    def qr(self, payload: str, x: float, y: float, size: float):
        widget = QrCodeWidget(payload)
        bounds = widget.getBounds()
        bw, bh = bounds[2] - bounds[0], bounds[3] - bounds[1]
        drawing = Drawing(size, size,
                          transform=[size / bw, 0, 0, size / bh, 0, 0])
        drawing.add(widget)
        renderPDF.draw(drawing, self.c, x, y)

    def portal_visual(self, x: float, top: float, width: float, height: float):
        c = self.c
        c.setFillColor(DARK)
        c.roundRect(x, top - height, width, height, 14, fill=1, stroke=0)
        c.setFillColor(ORANGE)
        c.setFont("Helvetica-Bold", 8)
        c.drawString(x + 17, top - 20,
                     "PORTALE LOCALE // 5 SCHEDE // IT + EN")
        card_w = (width - 54) / 2
        blocks = [
            ("HOME", "04  IL TUO RIG", ORANGE),
            ("DASH", "CLASSIC / REAL / OEM", CYAN),
            ("MEDIA", "ANTEPRIMA  100%", AMBER),
            ("GAME", "DESERT BLITZ", MAGENTA),
        ]
        for index, (name, value, accent) in enumerate(blocks):
            row, column = divmod(index, 2)
            bx = x + 17 + column * (card_w + 20)
            by = top - 38 - row * 65
            c.setFillColor(colors.HexColor("#1A1712"))
            c.setStrokeColor(accent)
            c.roundRect(bx, by - 51, card_w, 51, 7, fill=1, stroke=1)
            c.setFillColor(accent)
            c.setFont("Helvetica-Bold", 7)
            c.drawString(bx + 10, by - 17, name)
            c.setFillColor(WHITE)
            c.setFont("Helvetica-Bold", 9)
            c.drawString(bx + 10, by - 37, value)
        c.setFillColor(colors.HexColor("#070706"))
        c.roundRect(x + 17, top - height + 14, width - 34, 24, 7,
                    fill=1, stroke=0)
        c.setFont("Helvetica-Bold", 6.5)
        for index, name in enumerate(["HOME", "DASH", "MEDIA", "GAME", "RTL"]):
            c.setFillColor(ORANGE if index == 2 else colors.HexColor("#B5AA98"))
            c.drawCentredString(x + 58 + index * (width - 82) / 4,
                               top - height + 23, name)

    def game_visual(self, x: float, top: float, width: float, height: float):
        c = self.c
        c.setFillColor(colors.HexColor("#080312"))
        c.roundRect(x, top - height, width, height, 14, fill=1, stroke=0)
        c.setFillColor(MAGENTA)
        c.circle(x + width / 2, top - 73, 34, fill=1, stroke=0)
        c.setFillColor(colors.HexColor("#080312"))
        for line in range(4):
            c.rect(x + width / 2 - 35, top - 54 - line * 11,
                   70, 4, fill=1, stroke=0)
        c.setFillColor(colors.HexColor("#24131A"))
        c.setStrokeColor(CYAN)
        road = c.beginPath()
        road.moveTo(x + width / 2, top - 85)
        road.lineTo(x + width - 64, top - height + 18)
        road.lineTo(x + 64, top - height + 18)
        road.close()
        c.drawPath(road, fill=1, stroke=1)
        for ox, oy, radius in [(-52, -118, 8), (44, -149, 12),
                               (-18, -181, 16)]:
            cx = x + width / 2 + ox
            cy = top + oy
            c.setFillColor(colors.HexColor("#74523F"))
            c.setStrokeColor(AMBER)
            c.circle(cx, cy, radius, fill=1, stroke=1)
        c.setFillColor(LIME)
        c.roundRect(x + width / 2 - 17, top - height + 30,
                    34, 24, 5, fill=1, stroke=0)
        c.setFillColor(WHITE)
        c.setFont("Helvetica-Bold", 8)
        c.drawString(x + 14, top - 20, "DESERT BLITZ")
        c.setFillColor(CYAN)
        c.drawRightString(x + width - 14, top - 20, "SCORE 001240")

    def state_strip(self, x: float, top: float, width: float):
        c = self.c
        states = ["FORWARD", "NEUTRAL", "BRAKING", "ARMED", "REVERSE"]
        gap = 8
        box_w = (width - gap * 4) / 5
        for index, state in enumerate(states):
            bx = x + index * (box_w + gap)
            accent = RED if state == "BRAKING" else (
                WHITE if state == "REVERSE" else ORANGE)
            c.setFillColor(PANEL)
            c.setStrokeColor(accent)
            c.roundRect(bx, top - 42, box_w, 42, 6, fill=1, stroke=1)
            c.setFillColor(accent)
            c.setFont("Helvetica-Bold", 6.7)
            c.drawCentredString(bx + box_w / 2, top - 25, state)
            if index < 4:
                c.setFillColor(MUTED)
                c.setFont("Helvetica-Bold", 9)
                c.drawCentredString(bx + box_w + gap / 2, top - 25, ">")

    def table(self, x: float, top: float, widths: list[float],
              rows: list[list[str]], row_height=27) -> float:
        y = top
        for row_index, row in enumerate(rows):
            self.c.setFillColor(INK if row_index == 0 else
                                (WHITE if row_index % 2 else
                                 colors.HexColor("#E8E3D9")))
            self.c.rect(x, y - row_height, sum(widths), row_height,
                        fill=1, stroke=0)
            xx = x
            for column, value in enumerate(row):
                self.c.setStrokeColor(LINE)
                self.c.rect(xx, y - row_height, widths[column], row_height,
                            fill=0, stroke=1)
                self.c.setFillColor(ORANGE if row_index == 0 else INK)
                self.c.setFont("Helvetica-Bold" if row_index == 0 else
                               "Helvetica", 7.3)
                self.c.drawString(xx + 6, y - 17, value)
                xx += widths[column]
            y -= row_height
        return y


def build() -> Path:
    guide = QuickGuide()
    c = guide.c

    # 1 - cover
    guide.page = 1
    c.setFillColor(DARK)
    c.rect(0, 0, W, H, fill=1, stroke=0)
    c.setFillColor(ORANGE)
    c.rect(0, H - 12, W, 12, fill=1, stroke=0)
    c.rect(0, 0, 15, H, fill=1, stroke=0)
    logo = ASSETS / "logo-white.png"
    if logo.exists():
        c.drawImage(ImageReader(str(logo)), W - 232, H - 280, 170, 170,
                    preserveAspectRatio=True, mask="auto")
    c.setFillColor(ORANGE)
    c.setFont("Helvetica-Bold", 10)
    c.drawString(52, H - 76, "QUICK START // BETA 4.1")
    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 31)
    c.drawString(52, H - 348, "ULTRA4 MICRODASH")
    c.drawString(52, H - 388, "DUAL SCREEN")
    c.setFillColor(ORANGE)
    c.setFont("Helvetica-Bold", 22)
    c.drawString(52, H - 435, "PRONTO IN 5 MINUTI")
    c.setFillColor(colors.HexColor("#AAB4AC"))
    c.setFont("Helvetica", 11)
    c.drawString(52, H - 468,
                 "Accendi · collega il telefono · inserisci 3 dati")
    labels = ["1  USB", "2  WI-FI", "3  SALVA"]
    for index, label in enumerate(labels):
        x = 52 + index * 164
        c.setFillColor(PANEL)
        c.roundRect(x, 188, 148, 65, 11, fill=1, stroke=0)
        c.setFillColor(ORANGE if index != 1 else CYAN)
        c.setFont("Helvetica-Bold", 10)
        c.drawCentredString(x + 74, 218, label)
    c.setFillColor(colors.HexColor("#7C877E"))
    c.setFont("Helvetica", 7)
    c.drawString(52, 42, "Firmware 4.1.0-beta.1 | ESP32-S3 DevKitC-1")
    c.drawRightString(W - 52, 42, f"1/{TOTAL_PAGES}")
    guide.end()

    # 2 - power and firmware
    y = guide.header("01 / Accensione", "Collega e accendi",
                     "Se è già programmato, passa direttamente alla pagina 3.")
    y = guide.step("1", "Collega i display",
                   "Usa i connettori predisposti e non spostare fili a sistema acceso.",
                   M, y, W - 2 * M, CYAN)
    y = guide.step("2", "Collega USB-C",
                   "Usa un cavo dati. Dopo circa 5 secondi compaiono rete e password.",
                   M, y - 8, W - 2 * M, ORANGE)
    y = guide.step("3", "Controlla i due schermi",
                   "Entrambi devono accendersi. Se un componente scalda o senti odore, scollega subito.",
                   M, y - 8, W - 2 * M, AMBER)
    y -= 20
    guide.card(M, y, W - 2 * M, 68, "AGGIORNAMENTO - MACOS / LINUX",
               "Non serve Homebrew.", CYAN)
    guide.code("python3 tools/flash_dashboard.py --flash",
               M + 15, y - 76, W - 2 * M - 30)
    y -= 140
    guide.card(M, y, W - 2 * M, 68, "AGGIORNAMENTO - WINDOWS",
               "Al primo utilizzo conferma l’installazione di PlatformIO.", ORANGE)
    guide.code(r"py tools\flash_dashboard.py --flash",
               M + 15, y - 76, W - 2 * M - 30)
    guide.card(M, y - 142, W - 2 * M, 82, "SCHEDA NON TROVATA",
               "Cambia cavo. Tieni premuto BOOT, premi e rilascia RESET, rilascia BOOT e ripeti il comando.", RED)
    guide.end()

    # 3 - connection and onboarding
    y = guide.header("02 / Telefono", "Connetti e inserisci 3 dati",
                     "Funziona con Apple, Android e computer.")
    c.setFillColor(WHITE)
    c.roundRect(M, y - 246, 235, 246, 13, fill=1, stroke=0)
    guide.qr("http://192.168.4.1", M + 39, y - 184, 156)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 10)
    c.drawCentredString(M + 117, y - 213, "APRILO DOPO IL WI-FI")
    guide.card(M + 253, y, 258, 72, "1  RETE",
               "Scegli ULTRA4-DASH-XXXXXXXX e usa la password mostrata.", ORANGE)
    guide.card(M + 253, y - 86, 258, 72, "2  PAGINA",
               "Resta collegato e apri 192.168.4.1.", CYAN)
    guide.card(M + 253, y - 172, 258, 74, "3  CREDENZIALI",
               "Dopo il setup, tieni premuto BOOT per rivederle.", AMBER)
    y -= 270
    guide.card(M, y, W - 2 * M, 118, "PRIMA CONFIGURAZIONE",
               "Scegli <b>IT</b> o <b>EN</b>, poi inserisci soltanto <b>numero gara</b>, <b>nome veicolo</b> e <b>pilota</b>. Premi SALVA E INIZIA. La scelta lingua vale anche sui due display.", ORANGE)
    y -= 140
    guide.card(M, y, W - 2 * M, 76, "NESSUNA INTERNET È NORMALE",
               "Il portale è dentro il dispositivo. Non cambiare rete durante il salvataggio.", CYAN)
    guide.end()

    # 4 - portal, dashboard and photos
    y = guide.header("03 / Uso", "Cinque schede, tutto a portata di pollice",
                     "Il menu resta fisso in basso.")
    guide.portal_visual(M, y, W - 2 * M, 205)
    y -= 224
    guide.card(M, y, 245, 108, "DASH - 3 TEMI",
               "Classic Rally, Race Realism (pilota IQ3S-style + copilota onX-style) oppure Auto di serie OEM offroad.", CYAN)
    guide.card(M + 261, y, 250, 108, "LINGUA IT / EN",
               "Un solo selettore traduce portale e due display. La preferenza resta memorizzata.", ORANGE)
    y -= 126
    guide.card(M, y, W - 2 * M, 122, "MEDIA - 4 PASSAGGI",
               "1. Scegli JPG, JPEG, PNG o WEBP.  2. Controlla l’anteprima compatta.  3. Premi CARICA.  4. Attendi il 100%. La foto viene ottimizzata per entrambi gli schermi e resta disponibile dopo refresh e riavvio.", AMBER)
    y -= 140
    guide.card(M, y, W - 2 * M, 82, "MEMORIA LITTLEFS",
               "Mount corretto, recupero automatico e salvataggio atomico proteggono le foto anche se il collegamento si interrompe.", CYAN)
    guide.end()

    # 5 - game and ESC/RTL
    y = guide.header("04 / Game e luci", "DESERT BLITZ + controllo RTL",
                     "Arcade semplice; stati ESC distinti e verificabili.")
    guide.game_visual(M, y, 300, 205)
    guide.card(M + 318, y, 193, 96, "TOUCH",
               "Sinistra, destra e GAS. START, PAUSA e RIPRENDI.", MAGENTA, dark=True)
    guide.card(M + 318, y - 110, 193, 95, "TASTIERA",
               "Frecce o A/D. GAS: su, W o spazio. P: pausa.", CYAN, dark=True)
    y -= 225
    guide.card(M, y, W - 2 * M, 78, "OSTACOLI VISIBILI",
               "Il display pilota mostra massi, tronchi e solchi. L’ESP32 gestisce vite, collisioni, punteggio e riavvio.", MAGENTA)
    y -= 98
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 11)
    c.drawString(M, y, "ESC: frenata e retromarcia non vengono confuse")
    guide.state_strip(M, y - 14, W - 2 * M)
    y -= 72
    guide.card(M, y, W - 2 * M, 92, "RTL / ESC",
               "GPIO 4 controlla la barra WS2812. GPIO 5 legge il PWM ESC solo se abilitato. Filtro, isteresi, zona neutra e conferma reverse producono rosso in frenata e bianco soltanto in retromarcia confermata.", RED)
    guide.end()

    # 6 - installer and support
    y = guide.header("05 / Assistenza", "Cablaggio e controlli finali",
                     "Solo per installatori. Lavorare a dispositivo spento.")
    rows = [
        ["SEGNALE", "DISPLAY A", "GPIO", "DISPLAY B", "GPIO"],
        ["MOSI", "SDA", "11", "SDA", "13"],
        ["SCLK", "SCL", "12", "SCL", "14"],
        ["CS", "CS", "10", "CS", "15 / -1"],
        ["DC", "A0", "9", "DC", "16"],
        ["RST", "RST", "8", "RST", "17"],
        ["BACKLIGHT", "BL", "46", "BL", "18"],
        ["POWER", "VCC/GND", "3.3V/GND", "VCC/GND", "3.3V/GND"],
    ]
    y = guide.table(M, y, [90, 92, 79, 108, 142], rows)
    y -= 15
    guide.card(M, y, W - 2 * M, 94, "WS2812 E SEGNALE ESC",
               "WS2812 DATA: GPIO 4, alimentazione 5 V separata, fusibile, level shifter e massa comune. ESC PWM: GPIO 5, condizionato a 3,3 V. Mai collegare un GPIO ai 12 V.", RED)
    y -= 112
    guide.card(M, y, 315, 126, "CONTROLLO RAPIDO",
               "• rete assente: riavvia dopo 10 secondi<br/>• pagina assente: apri 192.168.4.1<br/>• foto ferma: leggi l’errore sotto la barra<br/>• componente caldo o odore: scollega subito", AMBER)
    c.setFillColor(WHITE)
    c.roundRect(M + 331, y - 126, 180, 126, 11, fill=1, stroke=1)
    guide.qr("https://github.com/redhorm/esp32-dual-screen-dashboard-CDX-",
             M + 344, y - 112, 92)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 8)
    c.drawString(M + 443, y - 47, "CODICE")
    c.drawString(M + 443, y - 64, "SUPPORTO")
    c.setFillColor(MUTED)
    c.setFont("Helvetica", 7)
    c.drawString(M + 443, y - 83, "Repository")
    c.drawString(M + 443, y - 98, "Aggiornamenti")
    y -= 145
    guide.card(M, y, W - 2 * M, 78, "NOTA",
               "La barra è scenografica e non sostituisce luci omologate, freno o sistemi di sicurezza indipendenti.", RED)
    guide.end()

    guide.save()
    return OUT


if __name__ == "__main__":
    print(build())
