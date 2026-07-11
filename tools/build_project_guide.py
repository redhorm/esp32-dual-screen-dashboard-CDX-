#!/usr/bin/env python3
"""Build the illustrated Italian project guide for the SEND//SYNC beta."""

from __future__ import annotations

import os
import textwrap
from pathlib import Path

from PIL import Image, ImageOps
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "docs" / "assets" / "guide"
TMP_SCREEN = ROOT / "tmp" / "pdfs" / "screens" / "portal-full.png"
OUT = ROOT / "output" / "pdf" / "Guida_Completa_ESP32_Dual_Screen_SEND_SYNC.pdf"

W, H = A4
M = 42

INK = colors.HexColor("#121713")
PAPER = colors.HexColor("#F3F1EA")
WHITE = colors.HexColor("#FFFFFF")
MUTED = colors.HexColor("#687169")
LINE = colors.HexColor("#D5D9D3")
DARK = colors.HexColor("#0A0E0B")
PANEL = colors.HexColor("#151B16")
LIME = colors.HexColor("#C8FF34")
AMBER = colors.HexColor("#FF9F1C")
CYAN = colors.HexColor("#5FF5FF")
RED = colors.HexColor("#EF4438")
SAND = colors.HexColor("#E9D7A6")


def prepare_assets() -> dict[str, Path]:
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    OUT.parent.mkdir(parents=True, exist_ok=True)

    logo_src = ASSET_DIR / "redhorm-logo.jpeg"
    if not logo_src.exists():
        raise FileNotFoundError(f"Logo non trovato: {logo_src}")

    logo = Image.open(logo_src).convert("L")
    logo = ImageOps.autocontrast(logo)
    bbox = ImageOps.invert(logo).getbbox()
    if bbox:
        logo = logo.crop(bbox)
    logo.thumbnail((1200, 1200), Image.Resampling.LANCZOS)

    def colored_mark(name: str, rgb: tuple[int, int, int]) -> Path:
        alpha = ImageOps.invert(logo).point(lambda p: 255 if p > 38 else 0)
        rgba = Image.new("RGBA", logo.size, rgb + (0,))
        rgba.putalpha(alpha)
        path = ASSET_DIR / name
        rgba.save(path)
        return path

    marks = {
        "logo_white": colored_mark("logo-white.png", (255, 255, 255)),
        "logo_lime": colored_mark("logo-lime.png", (200, 255, 52)),
        "logo_dark": colored_mark("logo-dark.png", (18, 23, 19)),
    }

    crops = {
        "portal_social": (0, 0, 1280, 970),
        "portal_game": (0, 970, 1280, 1470),
        "portal_reel": (0, 1440, 1280, 2220),
        "portal_themes": (0, 2160, 1280, 3090),
        "portal_photos": (0, 3030, 1280, 4020),
    }
    for key, box in crops.items():
        target = ASSET_DIR / f"{key.replace('_', '-')}.png"
        if TMP_SCREEN.exists() and not target.exists():
            full = Image.open(TMP_SCREEN).convert("RGB")
            safe_box = (
                max(0, box[0]), max(0, box[1]),
                min(full.width, box[2]), min(full.height, box[3]),
            )
            crop = full.crop(safe_box)
            crop.thumbnail((1800, 1800), Image.Resampling.LANCZOS)
            crop.save(target, optimize=True)
        if target.exists():
            marks[key] = target
    return marks


class Guide:
    def __init__(self, out: Path, assets: dict[str, Path]):
        self.c = canvas.Canvas(str(out), pagesize=A4, pageCompression=1)
        self.assets = assets
        self.page = 0
        self.chapter = ""
        self.c.setTitle("Guida completa ESP32 Dual Screen Dashboard - SEND//SYNC")
        self.c.setAuthor("redhorm / Codex")
        self.c.setSubject("Montaggio, installazione, uso e manutenzione del progetto")
        self.body = ParagraphStyle(
            "body", fontName="Helvetica", fontSize=9.2, leading=13,
            textColor=INK, alignment=TA_LEFT, spaceAfter=0,
        )
        self.small = ParagraphStyle(
            "small", fontName="Helvetica", fontSize=7.6, leading=10.5,
            textColor=MUTED, alignment=TA_LEFT,
        )
        self.white = ParagraphStyle(
            "white", fontName="Helvetica", fontSize=9, leading=12.4,
            textColor=WHITE, alignment=TA_LEFT,
        )

    def finish(self):
        self.c.save()

    def cover(self):
        self.page += 1
        c = self.c
        c.setFillColor(DARK)
        c.rect(0, 0, W, H, fill=1, stroke=0)
        c.setFillColor(LIME)
        c.rect(0, H - 12, W, 12, fill=1, stroke=0)
        c.rect(0, 0, 16, H, fill=1, stroke=0)

        mark = ImageReader(str(self.assets["logo_white"]))
        c.drawImage(mark, W - 250, H - 285, width=190, height=190,
                    preserveAspectRatio=True, mask="auto", anchor="c")

        c.setFillColor(LIME)
        c.setFont("Helvetica-Bold", 10)
        c.drawString(52, H - 72, "REDHORM // PROJECT GUIDE")
        c.setFillColor(WHITE)
        c.setFont("Helvetica-Bold", 32)
        c.drawString(52, H - 350, "ESP32 DUAL SCREEN")
        c.drawString(52, H - 389, "DASHBOARD")
        c.setFillColor(LIME)
        c.setFont("Helvetica-Bold", 22)
        c.drawString(52, H - 430, "SEND//SYNC SOCIAL BETA")

        c.setFillColor(colors.HexColor("#AAB5AC"))
        c.setFont("Helvetica", 12)
        c.drawString(52, H - 474, "Guida completa dalla A alla Z")
        c.setFont("Helvetica", 9)
        c.drawString(52, H - 494, "Montaggio  |  Flash  |  Portale AP  |  Foto  |  Social  |  Gioco")

        c.setFillColor(PANEL)
        c.roundRect(52, 88, W - 104, 84, 12, fill=1, stroke=0)
        c.setFillColor(WHITE)
        c.setFont("Helvetica-Bold", 11)
        c.drawString(70, 143, "VERSIONE DOCUMENTATA")
        c.setFillColor(LIME)
        c.setFont("Helvetica-Bold", 17)
        c.drawString(70, 116, "2.0.0-beta.1")
        c.setFillColor(colors.HexColor("#AAB5AC"))
        c.setFont("Helvetica", 8)
        c.drawRightString(W - 70, 120, "ESP32-S3 DevKitC-1")
        c.drawRightString(W - 70, 105, "ST7735S 160x80 + ST7789 240x240")
        c.setFillColor(colors.HexColor("#69736B"))
        c.setFont("Helvetica", 7)
        c.drawString(52, 40, "Documento generato dal codice reale del branch codex/social-beta")
        c.showPage()

    def _header(self, label: str, title: str, subtitle: str = "", dark: bool = False):
        self.page += 1
        self.chapter = label
        c = self.c
        bg = DARK if dark else PAPER
        c.setFillColor(bg)
        c.rect(0, 0, W, H, fill=1, stroke=0)
        c.setFillColor(LIME)
        c.rect(0, H - 8, W, 8, fill=1, stroke=0)
        c.setFillColor(LIME if dark else INK)
        c.setFont("Helvetica-Bold", 8)
        c.drawString(M, H - 38, label.upper())
        c.setFillColor(WHITE if dark else INK)
        c.setFont("Helvetica-Bold", 23)
        c.drawString(M, H - 72, title)
        if subtitle:
            c.setFillColor(colors.HexColor("#AAB5AC") if dark else MUTED)
            c.setFont("Helvetica", 9)
            c.drawString(M, H - 91, subtitle)
        c.setStrokeColor(colors.HexColor("#303830") if dark else LINE)
        c.line(M, H - 106, W - M, H - 106)
        self._footer(dark)
        return H - 128

    def _footer(self, dark: bool = False):
        c = self.c
        c.setFillColor(colors.HexColor("#78837A") if dark else MUTED)
        c.setFont("Helvetica", 7)
        c.drawString(M, 23, "ESP32 DUAL SCREEN DASHBOARD // SEND//SYNC")
        c.drawRightString(W - M, 23, f"{self.page:02d}")

    def end(self):
        self.c.showPage()

    def paragraph(self, text: str, x: float, y: float, w: float,
                  style: ParagraphStyle | None = None) -> float:
        p = Paragraph(text, style or self.body)
        _, h = p.wrap(w, H)
        p.drawOn(self.c, x, y - h)
        return y - h

    def section(self, title: str, y: float, x: float = M, color=INK) -> float:
        self.c.setFillColor(color)
        self.c.setFont("Helvetica-Bold", 12)
        self.c.drawString(x, y, title)
        return y - 18

    def pill(self, text: str, x: float, y: float, color=LIME, text_color=INK):
        c = self.c
        width = stringWidth(text, "Helvetica-Bold", 7) + 16
        c.setFillColor(color)
        c.roundRect(x, y - 10, width, 16, 8, fill=1, stroke=0)
        c.setFillColor(text_color)
        c.setFont("Helvetica-Bold", 7)
        c.drawString(x + 8, y - 5, text)
        return width

    def card(self, x: float, y_top: float, w: float, h: float, title: str,
             text: str, accent=LIME, dark=False, number: str | None = None):
        c = self.c
        c.setFillColor(PANEL if dark else WHITE)
        c.setStrokeColor(colors.HexColor("#323A33") if dark else LINE)
        c.roundRect(x, y_top - h, w, h, 11, fill=1, stroke=1)
        c.setFillColor(accent)
        c.rect(x, y_top - h, 5, h, fill=1, stroke=0)
        tx = x + 16
        if number:
            c.setFillColor(accent)
            c.setFont("Helvetica-Bold", 18)
            c.drawString(tx, y_top - 27, number)
            tx += 38
        c.setFillColor(WHITE if dark else INK)
        c.setFont("Helvetica-Bold", 10)
        c.drawString(tx, y_top - 22, title)
        style = self.white if dark else self.small
        self.paragraph(text, x + 16, y_top - 38, w - 29, style)

    def image(self, key: str, x: float, y_top: float, w: float, h: float,
              radius: float = 10):
        if key not in self.assets:
            return
        c = self.c
        c.saveState()
        path = str(self.assets[key])
        im = Image.open(path)
        iw, ih = im.size
        scale = max(w / iw, h / ih)
        dw, dh = iw * scale, ih * scale
        ox, oy = x + (w - dw) / 2, y_top - h + (h - dh) / 2
        clip = c.beginPath()
        clip.roundRect(x, y_top - h, w, h, radius)
        c.clipPath(clip, stroke=0, fill=0)
        c.drawImage(ImageReader(im), ox, oy, dw, dh)
        c.restoreState()
        c.setStrokeColor(LINE)
        c.roundRect(x, y_top - h, w, h, radius, fill=0, stroke=1)

    def table(self, x: float, y_top: float, widths: list[float], rows: list[list[str]],
              header=True, row_h: float = 25, font_size: float = 7.7,
              accents: set[int] | None = None):
        c = self.c
        accents = accents or set()
        y = y_top
        for r, row in enumerate(rows):
            h = row_h
            c.setFillColor(INK if header and r == 0 else (colors.HexColor("#E7E9E3") if r % 2 else WHITE))
            c.rect(x, y - h, sum(widths), h, fill=1, stroke=0)
            xx = x
            for j, cell in enumerate(row):
                c.setStrokeColor(LINE)
                c.rect(xx, y - h, widths[j], h, fill=0, stroke=1)
                c.setFillColor(LIME if (header and r == 0) else (RED if r in accents else INK))
                c.setFont("Helvetica-Bold" if (header and r == 0) or j == 0 else "Helvetica", font_size)
                lines = textwrap.wrap(str(cell), max(8, int(widths[j] / (font_size * 0.52)))) or [""]
                baseline = y - 10
                for line in lines[:2]:
                    c.drawString(xx + 6, baseline, line)
                    baseline -= font_size + 2
                xx += widths[j]
            y -= h
        return y

    def code(self, text: str, x: float, y_top: float, w: float, h: float):
        c = self.c
        c.setFillColor(DARK)
        c.roundRect(x, y_top - h, w, h, 9, fill=1, stroke=0)
        c.setFillColor(LIME)
        c.setFont("Courier-Bold", 7.7)
        y = y_top - 18
        for line in text.splitlines():
            c.drawString(x + 13, y, line)
            y -= 11

    def checklist(self, items: list[str], x: float, y: float, w: float,
                  color=LIME) -> float:
        for item in items:
            self.c.setFillColor(color)
            self.c.roundRect(x, y - 8, 9, 9, 2, fill=0, stroke=1)
            y = self.paragraph(item, x + 18, y + 1, w - 18, self.body) - 6
        return y

    # ----- vector diagrams -----
    def system_map(self, y_top: float):
        c = self.c
        x0 = 60
        boxes = [
            (x0, y_top - 88, 115, 58, "TELEFONO", "Browser iOS / Android"),
            (240, y_top - 88, 115, 58, "ESP32-S3", "AP + logica + storage"),
            (420, y_top - 35, 115, 58, "DISPLAY A", "Pilota 160x80"),
            (420, y_top - 141, 115, 58, "DISPLAY B", "Copilota 240x240"),
        ]
        for x, y, w, h, title, sub in boxes:
            c.setFillColor(WHITE)
            c.setStrokeColor(LINE)
            c.roundRect(x, y, w, h, 10, fill=1, stroke=1)
            c.setFillColor(INK)
            c.setFont("Helvetica-Bold", 9)
            c.drawCentredString(x + w / 2, y + 34, title)
            c.setFillColor(MUTED)
            c.setFont("Helvetica", 6.8)
            c.drawCentredString(x + w / 2, y + 19, sub)
        c.setStrokeColor(LIME)
        c.setLineWidth(3)
        c.line(175, y_top - 59, 240, y_top - 59)
        c.line(355, y_top - 59, 392, y_top - 59)
        c.line(392, y_top - 112, 392, y_top - 6)
        c.line(392, y_top - 6, 420, y_top - 6)
        c.line(392, y_top - 112, 420, y_top - 112)
        self.pill("Wi-Fi AP", 180, y_top - 45, LIME)
        self.pill("SPI2 + DMA", 360, y_top + 10, CYAN)
        self.pill("SPI3 + DMA", 360, y_top - 96, AMBER)

    def wiring(self, title: str, pins: list[tuple[str, str]], y_top: float, accent):
        c = self.c
        c.setFillColor(DARK)
        c.roundRect(M, y_top - 330, W - 2 * M, 330, 14, fill=1, stroke=0)
        c.setFillColor(WHITE)
        c.setFont("Helvetica-Bold", 14)
        c.drawString(M + 20, y_top - 28, title)
        esp_x, dsp_x = M + 28, W - M - 155
        c.setFillColor(PANEL)
        c.setStrokeColor(accent)
        c.roundRect(esp_x, y_top - 285, 130, 220, 11, fill=1, stroke=1)
        c.roundRect(dsp_x, y_top - 285, 130, 220, 11, fill=1, stroke=1)
        c.setFillColor(accent)
        c.setFont("Helvetica-Bold", 10)
        c.drawCentredString(esp_x + 65, y_top - 84, "ESP32-S3")
        c.drawCentredString(dsp_x + 65, y_top - 84, "DISPLAY")
        start_y = y_top - 112
        for i, (gpio, signal) in enumerate(pins):
            yy = start_y - i * 25
            c.setStrokeColor(accent if i < len(pins) - 2 else (RED if signal == "3.3 V" else colors.HexColor("#8D948E")))
            c.setLineWidth(2)
            c.line(esp_x + 110, yy, dsp_x + 20, yy)
            c.setFillColor(WHITE)
            c.setFont("Courier-Bold", 7.2)
            c.drawString(esp_x + 12, yy - 3, gpio)
            c.drawRightString(dsp_x + 118, yy - 3, signal)


def build() -> Path:
    assets = prepare_assets()
    g = Guide(OUT, assets)
    c = g.c

    g.cover()

    y = g._header("00 / Prima di iniziare", "Come usare questa guida",
                  "Percorso consigliato: leggi, cabla a scheda spenta, testa, poi personalizza.")
    g.card(M, y, W - 2 * M, 78, "OBIETTIVO",
           "Portare il progetto da componenti separati a dashboard dual-screen funzionante, configurabile dal telefono e pronta per la beta SEND//SYNC.", LIME)
    y -= 96
    y = g.section("Regole fondamentali", y)
    y = g.checklist([
        "Usa esclusivamente logica a 3.3 V sui pin dell'ESP32-S3.",
        "Togli alimentazione prima di spostare un filo. Un solo pin errato può danneggiare display o scheda.",
        "Prima prova ogni display con cavi corti, poi realizza l'installazione definitiva.",
        "Questa dashboard è dimostrativa e simulata: non sostituisce strumenti omologati o sistemi di sicurezza del veicolo.",
    ], M, y, W - 2 * M)
    y -= 8
    g.card(M, y, 238, 118, "SE SEI PRINCIPIANTE",
           "Segui i capitoli in ordine. Ogni comando va copiato nel Terminale senza il simbolo $. Se qualcosa non coincide, fermati e consulta la tabella Problemi e soluzioni.", CYAN)
    g.card(M + 254, y, 257, 118, "SE SEI ESPERTO",
           "Pin, clock, offset, FPS e unità sono centralizzati in include/config.h. Il core è C++ puro; HAL, scene e servizi sono separati per mantenere testabilità e determinismo.", AMBER)
    y -= 140
    g.section("Legenda rapida", y)
    g.pill("AZIONE", M, y - 26, LIME)
    g.pill("CONTROLLO", M + 82, y - 26, CYAN)
    g.pill("ATTENZIONE", M + 178, y - 26, RED, WHITE)
    g.pill("RISULTATO", M + 290, y - 26, AMBER)
    g.end()

    y = g._header("01 / Panoramica", "Che cosa costruirai",
                  "Un unico ESP32-S3 controlla due display e un portale web locale.")
    g.system_map(y)
    y -= 186
    g.card(M, y, 156, 118, "DISPLAY PILOTA", "ST7735S 160x80. RPM, velocità, marcia, bussola, warning e takeover social.", CYAN, number="A")
    g.card(M + 170, y, 156, 118, "DISPLAY COPILOTA", "ST7789 240x240. Roadbook, waypoint, telemetria, foto, show e gioco.", AMBER, number="B")
    g.card(M + 340, y, 171, 118, "TELEFONO", "Si collega all'access point ESP32. Nessuna app, router o cloud obbligatorio.", LIME, number="AP")
    y -= 142
    g.section("Cosa funziona senza Internet", y)
    y = g.checklist([
        "Dashboard, navigator, temi, luminosità, unità e salvataggio impostazioni.",
        "Upload foto di boot, galleria da 24 slot e cornice digitale.",
        "Dual Live Mirror, Moment Director, Spotter Squad, Trail Badges e Rock Line Rush.",
        "Reel Forge genera il poster sul telefono; Internet serve solo dopo, per pubblicarlo online.",
    ], M, y, W - 2 * M)
    g.end()

    y = g._header("02 / Materiali", "Lista completa dei componenti",
                  "Acquista i pannelli con controller e risoluzione esatti indicati sotto.")
    rows = [
        ["Q.tà", "Componente", "Specifica minima", "Nota"],
        ["1", "ESP32-S3 DevKitC-1", "USB nativo, PSRAM consigliata", "Scheda principale"],
        ["1", "Display pilota", "ST7735S IPS 0.96, 160x80", "7/8 pin SPI"],
        ["1", "Display copilota", "ST7789 IPS 1.3, 240x240", "SPI; CS opzionale"],
        ["1", "Cavo USB dati", "USB-C con linee dati", "Non solo ricarica"],
        ["16+", "Cavi Dupont", "Femmina-femmina, corti", "Sotto 10 cm per prova"],
        ["1", "Alimentazione", "5 V USB stabile", "Almeno 1 A"],
        ["Opz.", "Breadboard", "Passo 2.54 mm", "Consigliata nel prototipo"],
        ["Opz.", "Condensatori", "100 nF + 10 uF", "Vicino ai display"],
        ["Opz.", "Case/supporti", "Non conduttivi", "Protezione vibrazioni"],
    ]
    y = g.table(M, y, [38, 130, 174, 169], rows, row_h=31)
    y -= 18
    g.card(M, y, W - 2 * M, 95, "COME RICONOSCERE I DISPLAY",
           "Leggi la sigla del controller nell'inserzione o sul retro. La diagonale non basta: ST7735S e ST7789 richiedono inizializzazioni, offset e risoluzioni differenti. Verifica anche i nomi dei pin: SDA equivale a MOSI, SCL equivale a SCLK, A0 equivale a DC.", RED)
    y -= 116
    g.section("Strumenti utili", y)
    g.paragraph("Piccolo cacciavite, pinzetta, multimetro, nastro Kapton, fascette e saldatore solo per il montaggio definitivo. Il prototipo iniziale può essere realizzato interamente con connettori Dupont.", M, y - 18, W - 2 * M)
    g.end()

    y = g._header("03 / Cablaggio", "Display A - pilota 160x80",
                  "Bus FSPI dedicato. Collega esattamente un segnale alla volta.")
    pins_a = [("GPIO 11", "SDA / MOSI"), ("GPIO 12", "SCL / SCLK"), ("GPIO 10", "CS"),
              ("GPIO 9", "DC / A0"), ("GPIO 8", "RST"), ("GPIO 46", "BL / BLK"),
              ("3.3 V", "3.3 V"), ("GND", "GND")]
    g.wiring("ST7735S // 160x80", pins_a, y, CYAN)
    y -= 350
    rows = [["Modulo", "ESP32-S3", "Funzione"],
            ["SDA / MOSI", "GPIO 11", "Dati dal micro al display"],
            ["SCL / SCLK", "GPIO 12", "Clock SPI"], ["CS", "GPIO 10", "Selezione display"],
            ["DC / A0", "GPIO 9", "Comando oppure dati"], ["RST", "GPIO 8", "Reset pannello"],
            ["BL / BLK", "GPIO 46", "Luminosità PWM"], ["VCC", "3.3 V", "Alimentazione"],
            ["GND", "GND", "Massa comune"]]
    g.table(M, y, [120, 105, 286], rows, row_h=24)
    g.end()

    y = g._header("03 / Cablaggio", "Display B - copilota 240x240",
                  "Bus HSPI separato: i due display possono aggiornarsi in parallelo.")
    pins_b = [("GPIO 13", "SDA / MOSI"), ("GPIO 14", "SCL / SCLK"), ("GPIO 15", "CS"),
              ("GPIO 16", "DC"), ("GPIO 17", "RST"), ("GPIO 18", "BL / BLK"),
              ("3.3 V", "3.3 V"), ("GND", "GND")]
    g.wiring("ST7789 // 240x240", pins_b, y, AMBER)
    y -= 350
    g.card(M, y, W - 2 * M, 82, "MODULI SENZA PIN CS",
           "Alcuni ST7789 240x240 non espongono CS. In quel caso lascia GPIO 15 libero e imposta DISP_B_CS a -1 in include/config.h. Non collegare un segnale a un pin che il modulo non possiede.", AMBER)
    y -= 100
    g.section("Controllo elettrico prima dell'accensione", y)
    g.checklist([
        "VCC di entrambi i display va a 3.3 V, non a un GPIO.",
        "Tutte le masse GND devono essere in comune.",
        "Nessun filo scoperto deve toccare il telaio o un pin vicino.",
        "MOSI e SCLK dei due display sono distinti: non unirli.",
    ], M, y - 18, W - 2 * M)
    g.end()

    y = g._header("04 / Montaggio", "Assemblaggio fisico senza sorprese",
                  "Prima banco prova, poi installazione definitiva sul mezzo.")
    steps = [
        ("01", "Monta l'ESP32", "Appoggialo su breadboard o supporto isolante. Lascia accessibili USB, BOOT e RESET."),
        ("02", "Collega il Display A", "Segui la pagina precedente e controlla ogni filo con il multimetro in continuità."),
        ("03", "Collega il Display B", "Mantieni separati i due bus SPI. Fissa provvisoriamente i cavi per evitare strappi."),
        ("04", "Prima accensione", "Alimenta da USB del computer. Se una parte scalda, scollega immediatamente."),
        ("05", "Test software", "Esegui build, upload e monitor. Correggi rotazione, inversione e offset prima del case."),
        ("06", "Finitura", "Usa cavi corti, scarico di trazione, distanziali e isolamento. Evita vibrazioni sui connettori."),
    ]
    for i, (num, title, text) in enumerate(steps):
        col = i % 2
        row = i // 2
        g.card(M + col * 258, y - row * 136, 242, 116, title, text, LIME if col == 0 else AMBER, number=num)
    y -= 426
    g.card(M, y, W - 2 * M, 90, "ALIMENTAZIONE IN UN VEICOLO",
           "Non collegare direttamente l'ESP32 alla batteria 12 V. Usa un convertitore automotive 12 V -> 5 V protetto da fusibile, sovratensioni e inversione di polarità. Se non hai esperienza con impianti veicolari, fai realizzare questa parte da un tecnico.", RED)
    g.end()

    y = g._header("05 / Software", "Installare gli strumenti",
                  "Metodo consigliato: Visual Studio Code + estensione PlatformIO IDE.")
    g.card(M, y, W - 2 * M, 88, "OPZIONE 1 - PIU SEMPLICE",
           "Installa Visual Studio Code, apri Estensioni, cerca PlatformIO IDE ufficiale e premi Installa. PlatformIO Core è già incluso: non servono Homebrew, brew o una seconda installazione globale.", LIME)
    y -= 106
    g.section("Aprire il progetto", y)
    y = g.checklist([
        "Scarica o clona il repository GitHub.",
        "In Visual Studio Code scegli File > Open Folder e seleziona la cartella che contiene platformio.ini.",
        "Attendi il completamento dell'installazione automatica della piattaforma Espressif e di LovyanGFX.",
        "La barra PlatformIO deve mostrare l'ambiente esp32-s3-devkitc-1.",
    ], M, y - 18, W - 2 * M)
    y -= 6
    g.card(M, y, W - 2 * M, 92, "OPZIONE 2 - TERMINALE",
           "PlatformIO consiglia il proprio installer isolato. In alternativa, per ambienti controllati, Python può installare il comando con: python3 -m pip install -U platformio. Verifica poi con pio --version.", CYAN)
    y -= 112
    g.code("# verifica il comando\npio --version\n\n# entra nella cartella del progetto\ncd \"esp32 dashboard\"\n\n# prima compilazione\npio run", M, y, W - 2 * M, 112)
    y -= 132
    g.paragraph("Riferimento ufficiale: docs.platformio.org/en/latest/integration/ide/vscode.html", M, y, W - 2 * M, g.small)
    g.end()

    y = g._header("06 / Compilazione", "Test, build, flash e monitor",
                  "Esegui i comandi dalla cartella che contiene platformio.ini.")
    g.code("# 1. test del core su computer\npio test -e native\n\n# 2. compilazione firmware ESP32-S3\npio run\n\n# 3. upload sulla scheda collegata via USB\npio run -t upload\n\n# 4. log seriale\npio device monitor", M, y, W - 2 * M, 166)
    y -= 190
    rows = [["Comando", "Risultato atteso", "Se fallisce"],
            ["pio test -e native", "58 test superati", "Controlla dipendenze e branch"],
            ["pio run", "SUCCESS + memoria usata", "Leggi il primo errore, non l'ultimo"],
            ["pio run -t upload", "Scrittura e verifica flash", "Cavo dati, porta, BOOT"],
            ["pio device monitor", "Log a 115200 baud", "Chiudi altri monitor seriali"]]
    y = g.table(M, y, [128, 177, 206], rows, row_h=38)
    y -= 22
    g.card(M, y, W - 2 * M, 90, "SE L'UPLOAD NON PARTE",
           "Tieni premuto BOOT, premi e rilascia RESET, poi rilascia BOOT e ripeti l'upload. Il progetto abilita USB nativo e auto-reset a 1200 bps, ma la procedura manuale resta il recupero più affidabile al primo flash.", RED)
    g.end()

    y = g._header("07 / Primo avvio", "Collegarsi al portale AP",
                  "L'ESP32 crea una rete privata locale: il telefono parla direttamente con la dashboard.")
    g.card(M, y, 238, 112, "RETE PREDEFINITA", "SSID: ULTRA4-DASH<br/>Password: ultra4setup<br/>Indirizzo: http://192.168.4.1", LIME)
    g.card(M + 254, y, 257, 112, "NESSUN ROUTER", "La rete può indicare 'nessuna connessione Internet': è normale. Rimani connesso e apri Safari, Chrome o il pannello captive.", CYAN)
    y -= 138
    g.section("Procedura universale", y)
    y = g.checklist([
        "Accendi l'ESP32 e attendi circa 5 secondi.",
        "Apri le impostazioni Wi-Fi del telefono, tablet o computer.",
        "Seleziona ULTRA4-DASH e inserisci ultra4setup.",
        "Se il portale non compare da solo, apri il browser e scrivi 192.168.4.1 nella barra indirizzi.",
        "Tocca Salva e applica dopo ogni gruppo di modifiche.",
    ], M, y - 18, W - 2 * M)
    y -= 10
    g.card(M, y, W - 2 * M, 112, "iPHONE / ANDROID / ALTRI DISPOSITIVI",
           "Il portale usa HTML standard, Canvas, input file image/* e Web Share quando disponibile. Funziona su iOS, iPadOS, Android, Windows, macOS e Linux. Se il mini-pannello captive limita upload o condivisione, apri il browser completo sull'indirizzo 192.168.4.1.", AMBER)
    y -= 134
    g.section("Cambiare nome rete e password", y)
    g.paragraph("Modifica CONFIG_AP_SSID e CONFIG_AP_PASSWORD in include/config.h, poi ricompila e rifai l'upload. La password deve contenere almeno 8 caratteri.", M, y - 18, W - 2 * M)
    g.end()

    y = g._header("08 / Personalizzazione", "Temi, display e unità",
                  "Pilota e copilota possono usare temi diversi nello stesso momento.")
    g.image("portal_themes", M, y, W - 2 * M, 300)
    y -= 322
    rows = [["Tema", "Personalità", "Uso ideale"],
            ["Classic Rally", "Originale light-blue", "Uso generale e fedeltà al progetto"],
            ["Baja Amber", "Deserto ad alto contrasto", "Luce calda, show e racing"],
            ["Night Ops", "Ciano NVG + lime", "Notte e riprese cyber"],
            ["Trail Topo", "Olive, sabbia, expedition", "Overland ed esposizione"]]
    y = g.table(M, y, [112, 176, 223], rows, row_h=32)
    y -= 18
    g.paragraph("Luminosità e preferenze MPH/km/h, miglia/km, °F/°C vengono applicate in tempo reale e salvate in NVS, quindi restano dopo lo spegnimento.", M, y, W - 2 * M)
    g.end()

    y = g._header("09 / Immagine di boot", "La tua foto all'accensione",
                  "La conversione avviene sul telefono: l'ESP32 riceve solo file già ottimizzati.")
    g.image("portal_photos", M, y, 250, 330)
    g.card(M + 268, y, 243, 96, "01 / SCEGLI", "Tocca Scegli dalla galleria e seleziona qualsiasi immagine supportata dal dispositivo.", LIME)
    g.card(M + 268, y - 108, 243, 96, "02 / RITAGLIA", "Riempi occupa tutto lo schermo; Adatta conserva l'intera foto aggiungendo margini.", CYAN)
    g.card(M + 268, y - 216, 243, 96, "03 / CARICA", "Premi Carica foto boot. Il browser crea versioni 160x80 e 240x240 e le invia.", AMBER)
    y -= 354
    g.section("Parametri", y)
    rows = [["Voce", "Intervallo", "Effetto"],
            ["Mostra al boot", "On / Off", "Abilita la foto senza cancellarla"],
            ["Durata", "0.5 - 3.5 s", "Tempo di permanenza prima della dashboard"],
            ["Ritaglio", "Riempi / Adatta", "Applicato ai nuovi upload"],
            ["Rimuovi", "Azione", "Cancella entrambe le versioni dal filesystem"]]
    g.table(M, y, [130, 130, 251], rows, row_h=32)
    g.end()

    y = g._header("10 / Cornice digitale", "Modalità foto da esposizione",
                  "Trasforma uno o entrambi i display in una cornice autonoma.")
    g.card(M, y, W - 2 * M, 78, "FLUSSO RAPIDO",
           "Aggiungi una o più foto, scegli il display target, regola intervallo e transizione, abilita Modalità cornice digitale, poi premi Salva e applica.", LIME)
    y -= 98
    rows = [["Parametro", "Scelte / limiti", "Spiegazione semplice"],
            ["Galleria", "Fino a 24 foto", "Ogni slot contiene formato pilota + copilota"],
            ["Display", "Entrambi / pilota / copilota", "Dove appare lo slideshow"],
            ["Intervallo", "3 - 300 secondi", "Tempo tra una foto e la successiva"],
            ["Transizione", "Dissolvenza / taglio", "Dithering progressivo oppure cambio netto"],
            ["Ordine", "Sequenziale / casuale", "Percorso prevedibile o sempre diverso"],
            ["Overlay", "On / Off", "Indice foto e conto alla prossima"],
            ["Spazio", "Circa 4.9 MB LittleFS", "Controllato dal portale in tempo reale"]]
    y = g.table(M, y, [118, 154, 239], rows, row_h=40)
    y -= 18
    g.card(M, y, W - 2 * M, 90, "PER PRESTAZIONI STABILI",
           "Le immagini vengono ridimensionate esattamente alle due risoluzioni reali. La dissolvenza non alloca un secondo framebuffer 240x240: usa dithering progressivo e preserva RAM e compatibilità DMA.", CYAN)
    g.end()

    y = g._header("11 / Dashboard", "Cosa mostrano i due schermi",
                  "Il loop dimostrativo attraversa BOOT, LIVE e FINISH senza blocchi.")
    c.setFillColor(DARK)
    c.roundRect(M, y - 190, 306, 190, 14, fill=1, stroke=0)
    c.setFillColor(CYAN)
    c.setFont("Helvetica-Bold", 8)
    c.drawString(M + 16, y - 24, "DISPLAY A // PILOTA")
    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 46)
    c.drawString(M + 22, y - 90, "6840")
    c.setFillColor(LIME)
    c.setFont("Helvetica-Bold", 50)
    c.drawRightString(M + 286, y - 90, "4")
    c.setFillColor(colors.HexColor("#89928B"))
    c.setFont("Courier-Bold", 8)
    c.drawString(M + 22, y - 122, "68 MPH  |  13.9 V  |  72 C")
    c.setFillColor(LIME)
    c.rect(M + 22, y - 151, 230, 13, fill=1, stroke=0)
    c.setFillColor(RED)
    c.rect(M + 252, y - 151, 32, 13, fill=1, stroke=0)
    c.setFillColor(WHITE)
    c.setFont("Courier-Bold", 7)
    c.drawString(M + 22, y - 174, "287 DEG  |  4WD LOCK")
    c.setFillColor(WHITE)
    c.setStrokeColor(AMBER)
    c.roundRect(M + 324, y - 190, 187, 190, 14, fill=1, stroke=1)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 8)
    c.drawString(M + 340, y - 24, "DISPLAY B // ROADBOOK")
    rows_y = y - 48
    for i, label in enumerate(["0.20  L HAIRPIN", "1.30  STRAIGHT", "2.80  WATER", "4.10  R 90"]):
        c.setFillColor(colors.HexColor("#F0B52A") if i == 0 else colors.HexColor("#E9E7E0"))
        c.rect(M + 340, rows_y - i * 28 - 19, 120, 22, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Courier-Bold", 6.8)
        c.drawString(M + 346, rows_y - i * 28 - 12, label)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 26)
    c.drawCentredString(M + 486, y - 95, "68")
    c.setFont("Helvetica-Bold", 18)
    c.drawCentredString(M + 486, y - 135, "G4")
    y -= 216
    phases = [("BOOT", "Logo, sweep RPM, GPS acquiring e SAT LOCK", CYAN),
              ("LIVE", "Telemetria, roadbook, warning e waypoint", LIME),
              ("FINISH", "Tempo stage, massimi e riavvio automatico", AMBER)]
    for i, (title, text, col) in enumerate(phases):
        g.card(M + i * 172, y, 158, 112, title, text, col, number=str(i + 1))
    y -= 136
    g.paragraph("Il demo driver è roadbook-aware: accelera sul dritto e calcola la frenata prima di hairpin, curve e guadi. I valori provengono dal simulatore interno; per sensori reali va aggiunto un livello di acquisizione dedicato.", M, y, W - 2 * M)
    g.end()

    y = g._header("12 / Social Beta", "Cinque funzioni SEND//SYNC",
                  "Pensate per creare momenti visivi condivisibili senza app e senza cloud.")
    g.image("portal_social", M, y, W - 2 * M, 285)
    y -= 307
    features = [
        ("01", "DUAL LIVE MIRROR", "Ricostruisce i cockpit in Canvas a 4 Hz, usando pochi valori JSON."),
        ("02", "MOMENT DIRECTOR", "Mud Splash, Water Ford, Night Raid, Winch Pull e Send It sincronizzati."),
        ("03", "SPOTTER SQUAD", "Cue LEFT, RIGHT, STOP, WATER, CLEAR, WINCH e CHAMPION sui due display."),
        ("04", "TRAIL BADGES", "Achievement automatici per velocità, redline, 4x4, linea pulita e finish."),
        ("05", "REEL FORGE", "Poster JPEG 1080x1920 con foto, identità, telemetria e badge."),
    ]
    for i, (num, title, text) in enumerate(features):
        yy = y - i * 61
        c.setFillColor(LIME if i % 2 == 0 else AMBER)
        c.circle(M + 17, yy - 18, 17, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 8)
        c.drawCentredString(M + 17, yy - 21, num)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 8.7)
        c.drawString(M + 48, yy - 12, title)
        g.paragraph(text, M + 48, yy - 20, W - M - (M + 48), g.small)
    g.end()

    y = g._header("13 / Mini-gioco", "Rock Line Rush",
                  "Il telefono controlla; l'ESP32 decide fisica, collisioni e punteggio.")
    g.image("portal_game", M, y, W - 2 * M, 260)
    y -= 282
    g.card(M, y, 156, 96, "OBIETTIVO", "Attraversa 12 gate restando vicino alla linea sicura tra i massi.", LIME)
    g.card(M + 170, y, 156, 96, "CONTROLLI", "LEFT e RIGHT sterzano. THROTTLE aumenta la velocità. Tastiera: WASD.", CYAN)
    g.card(M + 340, y, 171, 96, "VITE E COMBO", "Tre vite. Perfect e Clean aumentano combo; Hit la azzera e toglie una vita.", AMBER)
    y -= 120
    rows = [["Risultato", "Errore dalla linea", "Punteggio"],
            ["PERFECT", "0 - 10 unità", "200 + combo x 35"],
            ["CLEAN", "11 - 32 unità", "100 + combo x 25 + precisione"],
            ["HIT", "Oltre 32 unità", "Vita -1, combo azzerata"],
            ["FINISH", "12 gate", "Bonus 300 per ogni vita rimasta"]]
    y = g.table(M, y, [112, 175, 224], rows, row_h=34)
    y -= 18
    g.paragraph("Watchdog di sicurezza: dopo 650 ms senza comandi, sterzo e gas tornano neutri. Il record e le vittorie sono persistenti in NVS.", M, y, W - 2 * M)
    g.end()

    y = g._header("14 / Studio social", "Reel Forge e Digital Garage",
                  "Il poster viene composto localmente nel browser del telefono.")
    g.image("portal_reel", M, y, 275, 338)
    g.card(M + 293, y, 218, 108, "DIGITAL GARAGE", "Callsign (16), team (20), handle (24), numero (4) e colore accent. I limiti corrispondono ai font embedded.", LIME)
    g.card(M + 293, y - 122, 218, 98, "GENERA", "Aggiungi una foto del mezzo e premi Genera poster. La foto resta nella memoria del browser.", CYAN)
    g.card(M + 293, y - 234, 218, 104, "CONDIVIDI", "Web Share apre la condivisione se supportata; altrimenti viene salvato un JPEG 1080x1920.", AMBER)
    y -= 362
    g.section("Flusso consigliato per pubblicare", y)
    y = g.checklist([
        "Collegati a ULTRA4-DASH e crea il poster.",
        "Salva il JPEG sul telefono.",
        "Riconnettiti alla rete Internet mobile o Wi-Fi.",
        "Pubblica il file dalla galleria o dalla schermata di condivisione.",
    ], M, y - 18, W - 2 * M)
    g.end()

    y = g._header("15 / Dati", "Memoria, API e privacy",
                  "Tutto resta locale tra telefono ed ESP32, salvo la condivisione scelta dall'utente.")
    g.card(M, y, 242, 122, "NVS / PREFERENCES", "Temi, unità, luminosità, foto mode, Digital Garage, record e vittorie. Dati piccoli e persistenti.", LIME)
    g.card(M + 258, y, 253, 122, "LITTLEFS", "Foto di boot e 24 coppie di immagini ottimizzate. Partizione circa 4.9 MB.", CYAN)
    y -= 144
    rows = [["Metodo", "Endpoint", "Funzione"],
            ["GET", "/api/v2/capabilities", "Versione e feature"],
            ["GET", "/api/v2/state", "Snapshot live completo"],
            ["GET/POST", "/api/v2/profile", "Digital Garage"],
            ["POST", "/api/v2/moment", "Avvia / ferma takeover"],
            ["POST", "/api/v2/callout", "Invia cue spotter"],
            ["POST", "/api/v2/game/start", "Nuova partita"],
            ["POST", "/api/v2/game/input", "Sterzo e throttle"],
            ["POST", "/api/v2/game/action", "Quit / restart"],
            ["GET/POST", "/api/settings", "Impostazioni display e foto"]]
    y = g.table(M, y, [67, 190, 254], rows, row_h=30)
    y -= 18
    g.card(M, y, W - 2 * M, 80, "PRIVACY",
           "Non esistono account, tracker o upload cloud nel firmware. Le foto della galleria vengono ridimensionate dal browser prima dell'invio; la foto di Reel Forge non viene caricata sull'ESP32.", AMBER)
    g.end()

    y = g._header("16 / Architettura", "Come è organizzato il codice",
                  "Separazione netta tra logica testabile, hardware, grafica e servizi.")
    layers = [
        ("src/core", "Simulatore, roadbook, race, formatter, gioco, social beta", LIME),
        ("src/app", "Scene dashboard, navigator, temi, takeover e gioco", CYAN),
        ("src/hal", "LovyanGFX, ST7735S, ST7789, SPI2/SPI3 e DMA", AMBER),
        ("src/services", "Portale AP, REST, pagina web, LittleFS e foto", SAND),
        ("src/main.cpp", "Scheduler: core 1 dash/sim, core 0 navigator", RED),
    ]
    for i, (name, text, col) in enumerate(layers):
        yy = y - i * 88
        c.setFillColor(col)
        c.roundRect(M, yy - 64, 110, 64, 9, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Courier-Bold", 9)
        c.drawCentredString(M + 55, yy - 37, name)
        c.setFillColor(WHITE)
        c.setStrokeColor(LINE)
        c.roundRect(M + 126, yy - 64, 385, 64, 9, fill=1, stroke=1)
        c.setFillColor(INK)
        c.setFont("Helvetica", 8.5)
        c.drawString(M + 144, yy - 37, text)
    y -= 460
    g.card(M, y, W - 2 * M, 92, "REGOLE DA NON ROMPERE",
           "Il core non deve dipendere da Arduino; i due display devono restare su host SPI separati; gli sprite restano in RAM interna per DMA; se aggiungi una sottocartella sotto src, aggiornala in build_src_filter di platformio.ini.", RED)
    y -= 112
    g.paragraph("I frame rate sono disaccoppiati: dashboard circa 30 FPS, navigator circa 12 FPS. Il navigator rallenta durante alcuni effetti critici per lasciare tempo CPU alla dash.", M, y, W - 2 * M)
    g.end()

    y = g._header("17 / Regolazione", "Rotazione, colori, offset e clock",
                  "Quasi tutti i problemi visivi iniziali sono configurazione, non codice.")
    rows = [["Sintomo", "Parametro", "Azione"],
            ["Immagine spostata", "DISP_A_COL_OFFSET / ROW_OFFSET", "Prova col 26/24 e row 1/0"],
            ["Sfondo o colori invertiti", "DISP_A_INVERT", "Inverti true / false"],
            ["Rosso e blu scambiati", "DISP_A_RGB_ORDER", "Inverti true / false"],
            ["Display capovolto", "DISP_A_ROTATION", "Prova 1 oppure 3"],
            ["Navigator capovolto", "DISP_B_ROTATION", "Prova 0 oppure 2"],
            ["Glitch / righe casuali", "DISP_A/B_SPI_HZ", "Riduci clock e accorcia i cavi"],
            ["Retroilluminazione fissa", "DISP_A/B_BL", "Usa -1 se BL è legato a 3.3 V"],
            ["ST7789 senza CS", "DISP_B_CS", "Imposta -1 e lascia GPIO libero"]]
    y = g.table(M, y, [150, 178, 183], rows, row_h=44)
    y -= 18
    g.code("// include/config.h - esempio conservativo\n#define DISP_A_SPI_HZ  27000000UL\n#define DISP_B_SPI_HZ  40000000UL\n#define DISP_A_COL_OFFSET 26\n#define DISP_A_ROW_OFFSET  1", M, y, W - 2 * M, 92)
    y -= 110
    g.paragraph("Dopo ogni modifica: salva, esegui pio run, carica con pio run -t upload e verifica entrambi i display.", M, y, W - 2 * M)
    g.end()

    y = g._header("18 / Problemi e soluzioni", "Diagnosi rapida",
                  "Parti sempre dall'alimentazione, poi cablaggio, poi software.")
    rows = [["Problema", "Causa probabile", "Soluzione"],
            ["Nessun display", "VCC/GND errati o firmware assente", "Misura 3.3 V; rifai upload"],
            ["Solo un display", "Bus o pin del pannello", "Ricontrolla la tabella del display"],
            ["Schermo bianco", "Invert/driver/reset", "Controlla controller, RST e INVERT"],
            ["Pixel casuali", "Cavi lunghi / clock alto", "Cavi <10 cm; abbassa SPI_HZ"],
            ["Upload non trova porta", "Cavo solo ricarica", "Usa cavo dati; prova BOOT + RESET"],
            ["pio non trovato", "Shell non configurata", "Usa terminale PlatformIO in VS Code"],
            ["AP non visibile", "Boot incompleto o alimentazione", "Attendi 5 s; leggi monitor seriale"],
            ["Portale non apre", "Pannello captive chiuso", "Apri http://192.168.4.1"],
            ["Foto non carica", "Spazio/formato/browser captive", "Elimina slot; usa Safari/Chrome completo"],
            ["Gioco non risponde", "Telefono non invia più input", "Resta sulla pagina; riavvia la partita"],
            ["Impostazioni perse", "NVS non scritta / reset flash", "Salva; evita erase completo"],
    ]
    g.table(M, y, [126, 178, 207], rows, row_h=49, font_size=7.2)
    g.end()

    y = g._header("19 / Manutenzione", "Aggiornare senza perdere il controllo",
                  "Mantieni main stabile e usa un branch beta per ogni evoluzione importante.")
    g.card(M, y, 242, 112, "PRIMA DI AGGIORNARE", "Annota pin e regolazioni funzionanti, salva il branch, esegui i test nativi e crea un commit identificabile.", LIME)
    g.card(M + 258, y, 253, 112, "DOPO L'AGGIORNAMENTO", "Compila, controlla uso RAM/flash, carica, verifica AP, entrambi i display, foto e gioco.", CYAN)
    y -= 136
    g.code("git status\ngit switch codex/social-beta\ngit pull\npio test -e native\npio run\npio run -t upload\npio device monitor", M, y, W - 2 * M, 132)
    y -= 156
    g.section("Backup e ripristino", y)
    y = g.checklist([
        "Il codice è salvato su GitHub; conserva anche una copia locale di include/config.h funzionante.",
        "Le impostazioni NVS e le foto LittleFS vivono sulla scheda e possono essere cancellate da un erase completo.",
        "Per un ripristino pulito, rifai build e upload; ricarica poi foto e profilo dal portale.",
        "Non fare merge della beta in main finché test e prova hardware non sono completati.",
    ], M, y - 18, W - 2 * M)
    y -= 10
    g.card(M, y, W - 2 * M, 94, "CONTROLLO PERIODICO OFF-ROAD",
           "Verifica connettori, fascette, isolamento, ossidazione e stabilità dell'alimentatore. Vibrazioni e polvere causano più guasti dei bug software. Scollega sempre l'alimentazione prima di ispezionare il cablaggio.", AMBER)
    g.end()

    y = g._header("20 / Collaudo", "Checklist finale completa",
                  "Spunta tutto prima di considerare il progetto pronto.")
    left = [
        "ESP32-S3 fissato e isolato",
        "Display A cablato sui GPIO 8-12 e 46",
        "Display B cablato sui GPIO 13-18",
        "Masse comuni e alimentazione 3.3 V verificate",
        "Nessun cavo dati più lungo del necessario",
        "58 test nativi superati",
        "Build firmware completata senza errori",
        "Upload e monitor seriale funzionanti",
    ]
    right = [
        "Entrambi i display leggibili e orientati",
        "AP ULTRA4-DASH visibile",
        "Portale aperto su 192.168.4.1",
        "Temi e luminosità persistono al reboot",
        "Foto boot verificata su entrambi i formati",
        "Slideshow verificato per almeno tre foto",
        "Moment, Spotter e Mirror verificati",
        "Rock Line Rush completabile dal telefono",
    ]
    g.card(M, y, 244, 430, "HARDWARE E FIRMWARE", "", CYAN)
    g.checklist(left, M + 16, y - 50, 212, CYAN)
    g.card(M + 260, y, 251, 430, "PORTALE E FUNZIONI", "", LIME)
    g.checklist(right, M + 276, y - 50, 219, LIME)
    y -= 454
    g.card(M, y, W - 2 * M, 100, "CRITERIO DI PRONTO",
           "Il progetto è pronto quando può essere spento, riacceso e usato da un secondo telefono seguendo soltanto SSID, password e indirizzo. Se servono correzioni manuali ogni volta, torna al capitolo Problemi e soluzioni.", AMBER)
    y -= 120
    g.paragraph("Build di riferimento verificata: RAM 15.4%, flash 31.8%, 58/58 test nativi superati.", M, y, W - 2 * M, g.small)
    g.end()

    y = g._header("21 / Riferimento rapido", "Una pagina da tenere a portata di mano",
                  "Le informazioni essenziali per configurare, aggiornare e recuperare il sistema.")
    g.card(M, y, 242, 130, "PORTALE", "SSID: ULTRA4-DASH<br/>Password: ultra4setup<br/>URL: http://192.168.4.1<br/>Client massimi: 2", LIME)
    g.card(M + 258, y, 253, 130, "DISPLAY", "A: ST7735S 160x80 / SPI2 / 30 FPS<br/>B: ST7789 240x240 / SPI3 / 12 FPS<br/>Seriale: 115200 baud", CYAN)
    y -= 154
    g.code("pio test -e native     # test\npio run                # build\npio run -t upload      # flash\npio device monitor     # log", M, y, W - 2 * M, 88)
    y -= 110
    rows = [["Display A", "GPIO", "Display B", "GPIO"],
            ["MOSI", "11", "MOSI", "13"], ["SCLK", "12", "SCLK", "14"],
            ["CS", "10", "CS", "15 / -1"], ["DC", "9", "DC", "16"],
            ["RST", "8", "RST", "17"], ["BL", "46", "BL", "18"]]
    y = g.table(M, y, [113, 70, 145, 183], rows, row_h=31)
    y -= 18
    g.card(M, y, W - 2 * M, 96, "RECUPERO UPLOAD",
           "1) Tieni premuto BOOT. 2) Premi e rilascia RESET. 3) Rilascia BOOT. 4) Avvia pio run -t upload. 5) Dopo il flash premi RESET se la scheda non riparte automaticamente.", RED)
    g.end()

    y = g._header("22 / Glossario e fonti", "Parole chiave senza gergo",
                  "Ultimo controllo prima di mettere le mani sul progetto.")
    glossary = [
        ["Termine", "Significato"],
        ["AP", "Access Point: rete Wi-Fi creata direttamente dall'ESP32."],
        ["SPI", "Bus veloce usato per inviare i pixel ai display."],
        ["DMA", "Trasferimento dati hardware con meno lavoro per la CPU."],
        ["NVS", "Memoria persistente per piccole impostazioni."],
        ["LittleFS", "Filesystem interno usato per foto e immagini di boot."],
        ["GPIO", "Pin programmabile dell'ESP32."],
        ["Framebuffer", "Area RAM che contiene un fotogramma completo."],
        ["Branch", "Linea di sviluppo separata in Git."],
        ["Captive portal", "Pagina che si apre collegandosi alla rete AP."],
    ]
    y = g.table(M, y, [112, 399], glossary, row_h=34)
    y -= 20
    g.section("Fonti e collegamenti", y)
    refs = [
        "Repository: github.com/redhorm/esp32-dual-screen-dashboard-CDX-",
        "PlatformIO IDE: docs.platformio.org/en/latest/integration/ide/vscode.html",
        "PlatformIO Core: docs.platformio.org/en/latest/core/installation/",
        "ESP32-S3 DevKitC: docs.espressif.com/projects/esp-dev-kits/",
        "LovyanGFX: github.com/lovyan03/LovyanGFX",
    ]
    y = g.checklist(refs, M, y - 18, W - 2 * M, AMBER)
    y -= 8
    g.card(M, y, W - 2 * M, 98, "FINE DELLA GUIDA - INIZIO DEL PROGETTO",
           "Procedi con calma, fotografa il cablaggio funzionante e modifica una sola variabile alla volta. Questa disciplina rende il sistema ripetibile, riparabile e pronto a crescere senza perdere la continuità del progetto originale.", LIME)
    g.end()

    g.finish()
    return OUT


if __name__ == "__main__":
    print(build())
