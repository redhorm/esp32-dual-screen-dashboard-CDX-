#!/usr/bin/env python3
"""Generate the concise mass-distribution guide for the dual-screen dashboard."""

from __future__ import annotations

from pathlib import Path

from PIL import Image
from reportlab.graphics import renderPDF
from reportlab.graphics.barcode.qr import QrCodeWidget
from reportlab.graphics.shapes import Drawing
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
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
PAPER = colors.HexColor("#F4F2EB")
INK = colors.HexColor("#111612")
DARK = colors.HexColor("#080D09")
PANEL = colors.HexColor("#151B16")
WHITE = colors.white
MUTED = colors.HexColor("#667068")
LINE = colors.HexColor("#D3D8D2")
LIME = colors.HexColor("#C8FF34")
CYAN = colors.HexColor("#5FF5FF")
MAGENTA = colors.HexColor("#FF2BD6")
AMBER = colors.HexColor("#FF9F1C")
RED = colors.HexColor("#EF4438")


class QuickGuide:
    def __init__(self):
        OUT.parent.mkdir(parents=True, exist_ok=True)
        self.c = canvas.Canvas(str(OUT), pagesize=A4, pageCompression=1)
        self.c.setTitle("Guida rapida ESP32 Dual Screen Dashboard")
        self.c.setAuthor("redhorm / Codex")
        self.c.setSubject("Installazione e uso rapido per distribuzione di massa")
        self.page = 0
        self.body = ParagraphStyle(
            "body", fontName="Helvetica", fontSize=11.5, leading=15.5,
            textColor=INK, alignment=TA_LEFT,
        )
        self.small = ParagraphStyle(
            "small", fontName="Helvetica", fontSize=9, leading=12,
            textColor=MUTED, alignment=TA_LEFT,
        )
        self.center = ParagraphStyle(
            "center", fontName="Helvetica-Bold", fontSize=12, leading=16,
            textColor=INK, alignment=TA_CENTER,
        )

    def save(self):
        self.c.save()

    def next_page(self, step: str, title: str, subtitle: str = "") -> float:
        self.page += 1
        c = self.c
        c.setFillColor(PAPER)
        c.rect(0, 0, W, H, fill=1, stroke=0)
        c.setFillColor(LIME)
        c.rect(0, H - 9, W, 9, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 9)
        c.drawString(M, H - 38, step.upper())
        c.setFont("Helvetica-Bold", 27)
        c.drawString(M, H - 76, title)
        if subtitle:
            c.setFillColor(MUTED)
            c.setFont("Helvetica", 10.5)
            c.drawString(M, H - 97, subtitle)
        c.setStrokeColor(LINE)
        c.line(M, H - 112, W - M, H - 112)
        c.setFillColor(MUTED)
        c.setFont("Helvetica", 7)
        c.drawString(M, 23, "ESP32 DUAL SCREEN // GUIDA RAPIDA")
        c.drawRightString(W - M, 23, f"{self.page}/8")
        return H - 135

    def end(self):
        self.c.showPage()

    def paragraph(self, text: str, x: float, y: float, w: float,
                  style: ParagraphStyle | None = None) -> float:
        p = Paragraph(text, style or self.body)
        _, h = p.wrap(w, H)
        p.drawOn(self.c, x, y - h)
        return y - h

    def card(self, x: float, y_top: float, w: float, h: float, title: str,
             text: str, accent=LIME, dark=False, number: str | None = None):
        c = self.c
        c.setFillColor(PANEL if dark else WHITE)
        c.setStrokeColor(colors.HexColor("#323A33") if dark else LINE)
        c.roundRect(x, y_top - h, w, h, 12, fill=1, stroke=1)
        c.setFillColor(accent)
        c.rect(x, y_top - h, 6, h, fill=1, stroke=0)
        tx = x + 18
        if number:
            c.setFillColor(accent)
            c.circle(tx + 14, y_top - 27, 14, fill=1, stroke=0)
            c.setFillColor(INK)
            c.setFont("Helvetica-Bold", 10)
            c.drawCentredString(tx + 14, y_top - 30, number)
            tx += 40
        c.setFillColor(WHITE if dark else INK)
        c.setFont("Helvetica-Bold", 11)
        c.drawString(tx, y_top - 25, title)
        style = ParagraphStyle(
            "card", parent=self.small, fontSize=9.7, leading=13,
            textColor=colors.HexColor("#C8D0C9") if dark else MUTED,
        )
        self.paragraph(text, x + 18, y_top - 43, w - 34, style)

    def step(self, number: str, title: str, text: str, x: float, y: float,
             w: float, color=LIME) -> float:
        self.c.setFillColor(color)
        self.c.circle(x + 22, y - 24, 22, fill=1, stroke=0)
        self.c.setFillColor(INK)
        self.c.setFont("Helvetica-Bold", 16)
        self.c.drawCentredString(x + 22, y - 29, number)
        self.c.setFillColor(INK)
        self.c.setFont("Helvetica-Bold", 14)
        self.c.drawString(x + 58, y - 19, title)
        end = self.paragraph(text, x + 58, y - 31, w - 58, self.body)
        return min(y - 54, end - 10)

    def screenshot(self, filename: str, x: float, y_top: float, w: float, h: float):
        path = ASSETS / filename
        if not path.exists():
            return
        im = Image.open(path).convert("RGB")
        iw, ih = im.size
        scale = max(w / iw, h / ih)
        dw, dh = iw * scale, ih * scale
        ox, oy = x + (w - dw) / 2, y_top - h + (h - dh) / 2
        self.c.saveState()
        clip = self.c.beginPath()
        clip.roundRect(x, y_top - h, w, h, 12)
        self.c.clipPath(clip, fill=0, stroke=0)
        self.c.drawImage(ImageReader(im), ox, oy, dw, dh)
        self.c.restoreState()
        self.c.setStrokeColor(LINE)
        self.c.roundRect(x, y_top - h, w, h, 12, fill=0, stroke=1)

    def code(self, lines: list[str], x: float, y_top: float, w: float, h: float):
        self.c.setFillColor(DARK)
        self.c.roundRect(x, y_top - h, w, h, 12, fill=1, stroke=0)
        self.c.setFillColor(LIME)
        self.c.setFont("Courier-Bold", 9.4)
        y = y_top - 25
        for line in lines:
            self.c.drawString(x + 18, y, line)
            y -= 16

    def experience_visual(self, x: float, y_top: float, w: float, h: float):
        """Compact code-native illustration: three-screen RIFT + arcade controls."""
        c = self.c
        c.setFillColor(DARK)
        c.roundRect(x, y_top - h, w, h, 14, fill=1, stroke=0)
        c.setFillColor(CYAN)
        c.setFont("Helvetica-Bold", 8)
        c.drawString(x + 18, y_top - 20, "RIFT//RUN  //  THREE-SCREEN LINK")
        # Phone, driver and navigator screens.
        screens = [(x + 20, y_top - 175, 70, 132),
                   (x + 112, y_top - 118, 126, 63),
                   (x + 260, y_top - 175, 132, 132)]
        for i, (sx, sy, sw, sh) in enumerate(screens):
            c.setFillColor(colors.HexColor("#13091F"))
            c.setStrokeColor(MAGENTA if i != 1 else CYAN)
            c.roundRect(sx, sy, sw, sh, 7, fill=1, stroke=1)
            c.setStrokeColor(colors.HexColor("#57316B"))
            for row in range(4):
                yy = sy + 14 + row * max(9, sh / 6)
                c.line(sx + 5, yy, sx + sw - 5, yy)
        # The same Jeep travels through all three screens.
        for cx, cy in [(x + 74, y_top - 121), (x + 174, y_top - 88),
                       (x + 306, y_top - 112)]:
            c.setFillColor(LIME)
            c.roundRect(cx - 12, cy - 8, 24, 16, 3, fill=1, stroke=0)
            c.setFillColor(INK)
            c.circle(cx - 8, cy - 9, 3, fill=1, stroke=0)
            c.circle(cx + 8, cy - 9, 3, fill=1, stroke=0)
        c.setStrokeColor(AMBER)
        c.setLineWidth(2)
        c.line(x + 91, y_top - 108, x + 111, y_top - 108)
        c.line(x + 239, y_top - 108, x + 259, y_top - 108)
        # Arcade wheel and T lever.
        base = x + 414
        c.setFillColor(colors.HexColor("#0B0711"))
        c.roundRect(base, y_top - 175, w - 432, 132, 8, fill=1, stroke=0)
        c.setStrokeColor(CYAN)
        c.setLineWidth(7)
        c.circle(base + 38, y_top - 104, 26, fill=0, stroke=1)
        c.setLineWidth(3)
        c.line(base + 16, y_top - 104, base + 60, y_top - 104)
        c.line(base + 38, y_top - 126, base + 38, y_top - 82)
        c.setStrokeColor(WHITE)
        c.line(base + 82, y_top - 145, base + 82, y_top - 70)
        c.setFillColor(MAGENTA)
        c.roundRect(base + 66, y_top - 82, 32, 15, 4, fill=1, stroke=0)
        c.setFillColor(WHITE)
        c.setFont("Helvetica-Bold", 7)
        c.drawCentredString(base + 38, y_top - 158, "WHEEL")
        c.drawCentredString(base + 82, y_top - 158, "T-LEVER")

    def settings_visual(self, x: float, y_top: float, w: float, h: float):
        """Stable V3 illustration; avoids screenshots becoming obsolete."""
        c = self.c
        c.setFillColor(DARK)
        c.roundRect(x, y_top - h, w, h, 14, fill=1, stroke=0)
        c.setFillColor(CYAN)
        c.setFont("Helvetica-Bold", 8)
        c.drawString(x + 18, y_top - 20, "DISPLAY CONTROL  //  DIGITAL GARAGE")
        names = [("CLASSIC", colors.HexColor("#75B6D5")),
                 ("BAJA", AMBER), ("NIGHT", CYAN), ("TRAIL", colors.HexColor("#9BA476"))]
        for i, (name, accent) in enumerate(names):
            sx=x+18+(i%2)*118; sy=y_top-67-(i//2)*82
            c.setFillColor(colors.HexColor("#120C18"))
            c.setStrokeColor(accent)
            c.roundRect(sx,sy-62,104,62,7,fill=1,stroke=1)
            c.setFillColor(accent);c.rect(sx+8,sy-23,88,6,fill=1,stroke=0)
            c.setFont("Helvetica-Bold",7);c.drawString(sx+8,sy-52,name)
        px=x+272
        c.setFillColor(colors.HexColor("#100A17"))
        c.roundRect(px,y_top-190,w-290,148,9,fill=1,stroke=0)
        c.setFillColor(WHITE);c.setFont("Helvetica-Bold",9)
        c.drawString(px+14,y_top-61,"FOTO DI AVVIO")
        c.setFillColor(colors.HexColor("#272031"));c.roundRect(px+14,y_top-124,w-318,47,6,fill=1,stroke=0)
        c.setFillColor(LIME);c.setFont("Helvetica-Bold",7)
        c.drawCentredString(px+(w-290)/2,y_top-103,"SCEGLI DALLA GALLERIA")
        c.setFillColor(WHITE);c.setFont("Helvetica-Bold",9)
        c.drawString(px+14,y_top-147,"CORNICE DIGITALE")
        for i in range(5):
            c.setFillColor([CYAN,MAGENTA,AMBER,LIME,colors.HexColor("#A98CFF")][i])
            c.roundRect(px+14+i*37,y_top-181,29,24,4,fill=1,stroke=0)

    def qr(self, payload: str, x: float, y: float, size: float):
        widget = QrCodeWidget(payload)
        bounds = widget.getBounds()
        bw, bh = bounds[2] - bounds[0], bounds[3] - bounds[1]
        drawing = Drawing(size, size, transform=[size / bw, 0, 0, size / bh, 0, 0])
        drawing.add(widget)
        renderPDF.draw(drawing, self.c, x, y)

    def mini_table(self, x: float, y_top: float, widths: list[float], rows: list[list[str]], row_h=29):
        y = y_top
        for i, row in enumerate(rows):
            self.c.setFillColor(INK if i == 0 else (WHITE if i % 2 else colors.HexColor("#E5E8E2")))
            self.c.rect(x, y - row_h, sum(widths), row_h, fill=1, stroke=0)
            xx = x
            for j, cell in enumerate(row):
                self.c.setStrokeColor(LINE)
                self.c.rect(xx, y - row_h, widths[j], row_h, fill=0, stroke=1)
                self.c.setFillColor(LIME if i == 0 else INK)
                self.c.setFont("Helvetica-Bold" if i == 0 else "Helvetica", 7.5)
                self.c.drawString(xx + 6, y - 18, cell)
                xx += widths[j]
            y -= row_h
        return y


def build() -> Path:
    g = QuickGuide()
    c = g.c

    # 1 - cover
    g.page = 1
    c.setFillColor(DARK)
    c.rect(0, 0, W, H, fill=1, stroke=0)
    c.setFillColor(LIME)
    c.rect(0, H - 12, W, 12, fill=1, stroke=0)
    c.rect(0, 0, 15, H, fill=1, stroke=0)
    logo = ASSETS / "logo-white.png"
    if logo.exists():
        c.drawImage(ImageReader(str(logo)), W - 232, H - 280, 170, 170,
                    preserveAspectRatio=True, mask="auto")
    c.setFillColor(LIME)
    c.setFont("Helvetica-Bold", 10)
    c.drawString(52, H - 76, "QUICK START // RIFT//RUN")
    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 31)
    c.drawString(52, H - 348, "ESP32 DUAL SCREEN")
    c.drawString(52, H - 388, "DASHBOARD")
    c.setFillColor(LIME)
    c.setFont("Helvetica-Bold", 23)
    c.drawString(52, H - 434, "PRONTO IN 10 MINUTI")
    c.setFillColor(colors.HexColor("#AAB4AC"))
    c.setFont("Helvetica", 11)
    c.drawString(52, H - 468, "Guida semplice per installazione e uso quotidiano")
    items = [("1", "COLLEGA USB"), ("2", "INSTALLA"), ("3", "APRI DAL TELEFONO")]
    for i, (n, label) in enumerate(items):
        x = 52 + i * 164
        c.setFillColor(PANEL)
        c.roundRect(x, 188, 148, 78, 12, fill=1, stroke=0)
        c.setFillColor(LIME if i != 1 else AMBER)
        c.circle(x + 25, 227, 16, fill=1, stroke=0)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 11)
        c.drawCentredString(x + 25, 223, n)
        c.setFillColor(WHITE)
        c.setFont("Helvetica-Bold", 8.5)
        c.drawString(x + 50, 223, label)
    c.setFillColor(colors.HexColor("#7C877E"))
    c.setFont("Helvetica", 7)
    c.drawString(52, 42, "Versione 3.0.0-beta.2  |  ESP32-S3 DevKitC-1")
    c.drawRightString(W - 52, 42, "1/8")
    g.end()

    # 2 - first power
    y = g.next_page("01 / Prima accensione", "Collega. Non serve altro.",
                    "Se il dispositivo è già programmato, vai direttamente a pagina 4.")
    y = g.step("1", "Collega i due display", "Usa i connettori predisposti. Non forzare e non spostare fili mentre il sistema è acceso.", M, y, W - 2 * M, CYAN)
    y = g.step("2", "Collega il cavo USB", "Deve essere un cavo dati USB-C. Il dispositivo si accende automaticamente.", M, y - 12, W - 2 * M, LIME)
    y = g.step("3", "Attendi 5 secondi", "I due schermi mostrano l'avvio e poi la dashboard. Se accade, l'hardware è pronto.", M, y - 12, W - 2 * M, AMBER)
    y -= 18
    g.card(M, y, 243, 112, "DEVE ACCADERE", "Entrambi gli schermi si illuminano, mostrano un'animazione e arrivano alla schermata principale.", LIME)
    g.card(M + 259, y, 252, 112, "FERMATI SE...", "Un componente scalda, senti odore o uno schermo resta completamente bianco. Scollega USB e vai a pagina 7.", RED)
    y -= 138
    g.card(M, y, W - 2 * M, 92, "SICUREZZA",
           "Alimenta l'ESP32 da USB 5 V. Non collegarlo direttamente alla batteria 12 V del veicolo. Per installazioni automotive usa un convertitore protetto realizzato da un tecnico.", AMBER)
    g.end()

    # 3 - flash
    y = g.next_page("02 / Firmware", "Un comando. Il tool fa il resto.",
                    "Serve solo quando la scheda è nuova o quando vuoi aggiornarla.")
    g.card(M, y, W - 2 * M, 74, "PRIMA", "Collega USB. Al primo uso resta online e rispondi S alla richiesta PlatformIO. Non serve Homebrew.", CYAN)
    y -= 96
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 12)
    c.drawString(M, y, "macOS / Linux")
    g.code(["python3 tools/flash_dashboard.py --flash"], M, y - 12, W - 2 * M, 55)
    y -= 90
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 12)
    c.drawString(M, y, "Windows")
    g.code([r"py tools\flash_dashboard.py --flash"], M, y - 12, W - 2 * M, 55)
    y -= 92
    g.step("1", "Controllo automatico", "Il tool trova PlatformIO, la scheda e la porta USB.", M, y, W - 2 * M, LIME)
    g.step("2", "Compilazione", "Prepara il firmware corretto per ESP32-S3.", M, y - 82, W - 2 * M, CYAN)
    g.step("3", "Installazione", "Scrive il firmware e mostra INSTALLAZIONE COMPLETATA.", M, y - 164, W - 2 * M, AMBER)
    g.card(M, y - 260, W - 2 * M, 92, "SE NON TROVA LA SCHEDA",
           "Cambia cavo USB. Poi tieni premuto BOOT, premi e rilascia RESET, rilascia BOOT e ripeti lo stesso comando.", RED)
    g.end()

    # 4 - Wi-Fi
    y = g.next_page("03 / Telefono", "Apri il pannello di controllo",
                    "Funziona su iPhone, iPad, Android, Windows, macOS e Linux.")
    c.setFillColor(WHITE)
    c.roundRect(M, y - 300, 250, 300, 14, fill=1, stroke=0)
    g.qr("WIFI:T:WPA;S:ULTRA4-DASH;P:ultra4setup;;", M + 46, y - 236, 158)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 11)
    c.drawCentredString(M + 125, y - 267, "SCANSIONA PER IL WI-FI")
    c.setFillColor(MUTED)
    c.setFont("Helvetica", 8)
    c.drawCentredString(M + 125, y - 284, "oppure inserisci i dati a destra")
    g.card(M + 270, y, 241, 96, "RETE", "Nome: ULTRA4-DASH<br/>Password: ultra4setup", LIME)
    g.card(M + 270, y - 112, 241, 96, "PAGINA", "Apri Safari o Chrome e scrivi:<br/><b>http://192.168.4.1</b>", CYAN)
    g.card(M + 270, y - 224, 241, 76, "NORMALE", "Il telefono può dire 'nessuna connessione Internet'. Rimani collegato.", AMBER)
    y -= 330
    g.step("1", "Scansiona o scegli la rete", "Apri le impostazioni Wi-Fi del telefono.", M, y, W - 2 * M, LIME)
    g.step("2", "Apri il browser", "Se il pannello non compare da solo, usa l'indirizzo 192.168.4.1.", M, y - 80, W - 2 * M, CYAN)
    g.step("3", "Salva e applica", "Le modifiche restano memorizzate anche dopo lo spegnimento.", M, y - 160, W - 2 * M, AMBER)
    g.end()

    # 5 - appearance and photos
    y = g.next_page("04 / Personalizza", "Grafica e foto in una pagina",
                    "Scegli, controlla l'anteprima e premi Salva e applica.")
    g.settings_visual(M, y, W - 2 * M, 220)
    y -= 244
    g.card(M, y, 244, 110, "GRAFICA",
           "Scegli Classic, Baja, Night o Trail per pilota e copilota. Regola luminosità e unità.", CYAN, number="1")
    g.card(M + 260, y, 251, 110, "FOTO DI AVVIO",
           "Scegli dalla galleria, premi Carica foto boot e regola la durata.", LIME, number="2")
    y -= 128
    g.card(M, y, 244, 118, "CORNICE DIGITALE",
           "Aggiungi fino a 24 foto, scegli display e intervallo, poi attiva la modalità.", AMBER, number="3")
    g.card(M + 260, y, 251, 118, "SALVA",
           "Premi Salva e applica. Funziona su Apple, Android e computer.", LIME, number="4")
    y -= 138
    g.card(M, y, W - 2 * M, 88, "SALVATAGGIO SICURO",
           "Le due versioni vengono verificate e attivate insieme. Se la connessione si interrompe, lo slot non appare e la precedente foto boot resta valida.", CYAN)
    g.end()

    # 6 - RIFT and arcade
    y = g.next_page("05 / Effetto wow", "RIFT//RUN e Neon Trail Rush '86",
                    "Una scena a tre schermi e un arcade retrò. Tutto locale, senza Internet.")
    g.experience_visual(M, y, W - 2 * M, 205)
    y -= 227
    g.card(M, y, 244, 132, "RIFT//RUN",
           "Metti telefono, pilota e copilota nella stessa inquadratura. Scegli l'ordine, tieni premuto APRI IL PORTALE e riprendi la Jeep che attraversa i tre schermi.", MAGENTA, number="1")
    g.card(M + 260, y, 251, 132, "NEON TRAIL RUSH '86",
           "Premi ENTER ARCADE e INSERT COIN. Sterza con il volante, porta in alto la leva a T per accelerare e supera i massi neon.", CYAN, number="2")
    y -= 152
    g.card(M, y, W - 2 * M, 94, "PRIMA DI INIZIARE",
           "Salva callsign, team, numero e colore in Identità del rig. RIFT e il gioco tornano automaticamente alla dashboard quando esci.", LIME)
    g.end()

    # 7 - troubleshooting
    y = g.next_page("06 / Aiuto", "Se qualcosa non funziona",
                    "Prova una sola soluzione alla volta, nell'ordine indicato.")
    problems = [
        ("NESSUNA ACCENSIONE", "Cambia porta USB e cavo. Usa un cavo dati.", RED),
        ("UNO SCHERMO BIANCO", "Spegni. Controlla il relativo connettore. Riaccendi.", AMBER),
        ("RETE NON VISIBILE", "Attendi 5 secondi. Riavvia scollegando USB per 10 secondi.", CYAN),
        ("PAGINA NON SI APRE", "Rimani su ULTRA4-DASH e apri http://192.168.4.1.", LIME),
        ("FLASH BLOCCATO", "BOOT premuto, tocca RESET, rilascia BOOT, ripeti il tool.", RED),
        ("FOTO NON CARICATA", "Leggi il messaggio: formato, rete e memoria sono indicati separatamente. Riprova vicino al dispositivo.", AMBER),
    ]
    for i, (title, text, col) in enumerate(problems):
        row, column = divmod(i, 2)
        g.card(M + column * 258, y - row * 143, 242, 124, title, text, col, number=str(i + 1))
    y -= 442
    g.card(M, y, W - 2 * M, 90, "QUANDO FERMARSI",
           "Se un componente scalda, senti odore o vedi un filo danneggiato: scollega subito USB. Non continuare a tentare. Fai controllare il dispositivo da un tecnico.", RED)
    y -= 112
    g.paragraph("Per dettagli tecnici consulta README.md nel repository GitHub.", M, y, W - 2 * M, g.small)
    g.end()

    # 8 - service card
    y = g.next_page("07 / Scheda tecnica", "Solo per installatore o assistenza",
                    "L'utente finale non deve modificare questi collegamenti.")
    c.setFillColor(RED)
    c.roundRect(M, y - 46, W - 2 * M, 46, 10, fill=1, stroke=0)
    c.setFillColor(WHITE)
    c.setFont("Helvetica-Bold", 10)
    c.drawCentredString(W / 2, y - 29, "LAVORARE SEMPRE A DISPOSITIVO SPENTO")
    y -= 66
    rows = [
        ["SEGNALE", "DISPLAY A", "GPIO", "DISPLAY B", "GPIO"],
        ["MOSI", "SDA", "11", "SDA", "13"], ["SCLK", "SCL", "12", "SCL", "14"],
        ["CS", "CS", "10", "CS", "15 / -1"], ["DC", "A0", "9", "DC", "16"],
        ["RST", "RST", "8", "RST", "17"], ["BACKLIGHT", "BL", "46", "BL", "18"],
        ["POWER", "VCC / GND", "3.3V / GND", "VCC / GND", "3.3V / GND"],
    ]
    y = g.mini_table(M, y, [92, 94, 80, 105, 140], rows, row_h=31)
    y -= 20
    g.card(M, y, 255, 150, "COLLAUDO RAPIDO",
           "1. Entrambi i display si avviano.<br/>2. AP e portale sono visibili.<br/>3. RIFT attraversa i display in ordine.<br/>4. Un upload interrotto conserva la foto valida.<br/>5. Volante e leva controllano il gioco.", LIME)
    c.setFillColor(WHITE)
    c.roundRect(M + 273, y - 150, 238, 150, 12, fill=1, stroke=0)
    g.qr("https://github.com/redhorm/esp32-dual-screen-dashboard-CDX-", M + 290, y - 130, 105)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 9)
    c.drawString(M + 405, y - 52, "CODICE E SUPPORTO")
    c.setFillColor(MUTED)
    c.setFont("Helvetica", 7.5)
    c.drawString(M + 405, y - 72, "Repository GitHub")
    c.drawString(M + 405, y - 89, "Guida rapida")
    c.drawString(M + 405, y - 106, "Aggiornamenti beta")
    y -= 176
    g.card(M, y, W - 2 * M, 92, "CONFIGURAZIONE BASE",
           "ESP32-S3 DevKitC-1 | ST7735S 160x80 | ST7789 240x240 | Seriale 115200 | AP 192.168.4.1 | Firmware 3.0.0-beta.2", CYAN)
    g.end()

    g.save()
    return OUT


if __name__ == "__main__":
    print(build())
