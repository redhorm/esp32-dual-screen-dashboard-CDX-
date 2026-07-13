#!/usr/bin/env python3
"""Assistente semplice e multipiattaforma per compilare e flashare la dashboard.

Dipendenze del programma: solo libreria standard Python. PlatformIO viene cercato
nel progetto, nel sistema e nell'installazione standard dell'utente. Se manca,
puo essere installato in un ambiente virtuale isolato dentro ``.tools``.
"""

from __future__ import annotations

import argparse
import configparser
import importlib.util
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import textwrap
import venv
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Optional, Sequence


SCRIPT_VERSION = "1.0.0"
PROJECT_ROOT = Path(__file__).resolve().parents[1]
PLATFORMIO_INI = PROJECT_ROOT / "platformio.ini"
LOCAL_PIO_VENV = PROJECT_ROOT / ".tools" / "platformio-venv"


class AssistantError(Exception):
    """Errore previsto, gia formulato per chi usa il programma."""

    def __init__(self, message: str, exit_code: int = 2) -> None:
        super().__init__(message)
        self.exit_code = exit_code


class Ui:
    """Output terminale leggibile, con colori solo quando supportati."""

    def __init__(self, use_color: bool) -> None:
        self.use_color = use_color

    def _paint(self, value: str, code: str) -> str:
        if not self.use_color:
            return value
        return f"\033[{code}m{value}\033[0m"

    def title(self, value: str) -> None:
        print()
        print(self._paint(value, "1;36"))

    def ok(self, value: str) -> None:
        print(self._paint(f"[OK] {value}", "1;32"))

    def info(self, value: str) -> None:
        print(self._paint(f"[i]  {value}", "36"))

    def warn(self, value: str) -> None:
        print(self._paint(f"[!]  {value}", "1;33"))

    def error(self, value: str) -> None:
        sys.stdout.flush()
        print(self._paint(f"[ERRORE] {value}", "1;31"), file=sys.stderr)

    def command(self, argv: Sequence[str]) -> None:
        print(self._paint(f"      {format_command(argv)}", "2"))


@dataclass(frozen=True)
class ProjectConfig:
    environment: str
    board: str
    monitor_speed: int


@dataclass(frozen=True)
class SerialDevice:
    port: str
    description: str
    hwid: str

    @property
    def score(self) -> int:
        """Probabilita euristica che la porta appartenga a una scheda ESP32."""
        haystack = f"{self.port} {self.description} {self.hwid}".upper()
        score = 0
        weights = (
            ("ESP32-S3", 140),
            ("ESP32", 120),
            ("ESPRESSIF", 100),
            ("VID:PID=303A", 90),
            ("USB JTAG", 80),
            ("USB SERIAL", 50),
            ("CP210", 50),
            ("CH340", 50),
            ("CH341", 50),
            ("VID:PID=10C4", 45),
            ("VID:PID=1A86", 45),
            ("USBCDC", 45),
            ("USBMODEM", 40),
            ("TTYACM", 40),
            ("USBSERIAL", 35),
        )
        for marker, points in weights:
            if marker in haystack:
                score += points
        if self.port.upper().startswith("COM"):
            score += 10
        if "/DEV/CU." in haystack:
            score += 10
        if "BLUETOOTH" in haystack:
            score -= 300
        if "DEBUG-CONSOLE" in haystack:
            score -= 300
        return score


def format_command(argv: Sequence[str]) -> str:
    if os.name == "nt":
        return subprocess.list2cmdline(list(argv))
    return shlex.join(argv)


def executable_in_venv(venv_root: Path) -> Optional[Path]:
    candidates = (
        venv_root / "Scripts" / "pio.exe",
        venv_root / "Scripts" / "platformio.exe",
        venv_root / "bin" / "pio",
        venv_root / "bin" / "platformio",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def python_in_venv(venv_root: Path) -> Path:
    if os.name == "nt":
        return venv_root / "Scripts" / "python.exe"
    return venv_root / "bin" / "python"


def find_platformio() -> Optional[list[str]]:
    """Trova PlatformIO senza dipendere da Homebrew o da un package manager."""
    override = os.environ.get("PLATFORMIO_CLI", "").strip()
    if override:
        command = shlex.split(override, posix=os.name != "nt")
        if command:
            return command

    local_roots = (
        LOCAL_PIO_VENV,
        PROJECT_ROOT / ".venv",
        Path.home() / ".platformio" / "penv",
    )
    for root in local_roots:
        executable = executable_in_venv(root)
        if executable:
            return [str(executable)]

    for name in ("pio", "platformio"):
        executable = shutil.which(name)
        if executable:
            return [executable]

    try:
        module = importlib.util.find_spec("platformio")
    except (ImportError, ModuleNotFoundError, ValueError):
        module = None
    if module is not None:
        return [sys.executable, "-m", "platformio"]
    return None


def read_project_config(environment_override: Optional[str]) -> ProjectConfig:
    if not PLATFORMIO_INI.is_file():
        raise AssistantError(
            f"Non trovo {PLATFORMIO_INI.name}. Avvia questo file dalla cartella del progetto."
        )

    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        parser.read(PLATFORMIO_INI, encoding="utf-8")
    except (configparser.Error, OSError) as exc:
        raise AssistantError(f"Non riesco a leggere platformio.ini: {exc}") from exc

    default_value = parser.get("platformio", "default_envs", fallback="").strip()
    default_envs = [item.strip() for item in default_value.replace("\n", ",").split(",")]
    default_envs = [item for item in default_envs if item]
    environment = environment_override or (default_envs[0] if default_envs else "")
    if not environment:
        raise AssistantError(
            "platformio.ini non indica un ambiente predefinito. Usa --env NOME."
        )

    section = f"env:{environment}"
    if not parser.has_section(section):
        raise AssistantError(
            f"L'ambiente '{environment}' non esiste in platformio.ini."
        )
    board = parser.get(section, "board", fallback="scheda non specificata").strip()
    speed_text = parser.get(section, "monitor_speed", fallback="115200").strip()
    try:
        monitor_speed = int(speed_text)
    except ValueError:
        monitor_speed = 115200
    return ProjectConfig(environment, board, monitor_speed)


def run_capture(
    argv: Sequence[str], timeout: int = 30
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            list(argv),
            cwd=PROJECT_ROOT,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            env=command_environment(),
        )
    except FileNotFoundError as exc:
        raise AssistantError(
            "PlatformIO risulta configurato, ma il suo eseguibile non e piu disponibile."
        ) from exc
    except subprocess.TimeoutExpired as exc:
        raise AssistantError("PlatformIO non ha risposto entro il tempo previsto.") from exc


def command_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment.setdefault("PLATFORMIO_NO_ANSI", "true")
    environment.setdefault("PLATFORMIO_SETTING_ENABLE_TELEMETRY", "no")
    environment.setdefault("PYTHONUNBUFFERED", "1")
    return environment


def install_local_platformio(ui: Ui, dry_run: bool) -> list[str]:
    """Installa PlatformIO in un venv del progetto, senza modificare il sistema."""
    ui.title("INSTALLAZIONE PLATFORMIO")
    ui.info("L'installazione resta isolata in .tools/platformio-venv.")
    create_command = [sys.executable, "-m", "venv", str(LOCAL_PIO_VENV)]
    pip_command = [
        str(python_in_venv(LOCAL_PIO_VENV)),
        "-m",
        "pip",
        "install",
        "--disable-pip-version-check",
        "platformio",
    ]

    if dry_run:
        ui.command(create_command)
        ui.command(pip_command)
        ui.ok("Simulazione: nessun file e stato modificato.")
        return [str(executable_in_venv(LOCAL_PIO_VENV) or "pio")]

    try:
        if not python_in_venv(LOCAL_PIO_VENV).is_file():
            ui.info("Creo l'ambiente Python isolato...")
            venv.EnvBuilder(with_pip=True, clear=False).create(LOCAL_PIO_VENV)
        ui.info("Scarico e installo PlatformIO...")
        result = stream_command(pip_command)
    except (OSError, subprocess.SubprocessError) as exc:
        raise AssistantError(f"Installazione PlatformIO non riuscita: {exc}") from exc

    if result.returncode != 0:
        advice = diagnose_failure("install", result.output)
        raise AssistantError(
            "Installazione PlatformIO non riuscita.\n" + "\n".join(f"- {x}" for x in advice)
        )
    executable = executable_in_venv(LOCAL_PIO_VENV)
    if not executable:
        raise AssistantError(
            "Installazione terminata, ma l'eseguibile PlatformIO non e stato trovato."
        )
    ui.ok("PlatformIO e pronto.")
    return [str(executable)]


@dataclass(frozen=True)
class StreamResult:
    returncode: int
    output: str


def stream_command(argv: Sequence[str]) -> StreamResult:
    """Mostra l'output in tempo reale e conserva la parte utile per la diagnosi."""
    try:
        process = subprocess.Popen(
            list(argv),
            cwd=PROJECT_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
            env=command_environment(),
        )
    except FileNotFoundError as exc:
        raise AssistantError("Impossibile avviare PlatformIO.") from exc

    output_lines: list[str] = []
    assert process.stdout is not None
    try:
        for line in process.stdout:
            print(line, end="")
            output_lines.append(line)
            if len(output_lines) > 500:
                del output_lines[:100]
        returncode = process.wait()
    except KeyboardInterrupt:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
        raise
    return StreamResult(returncode, "".join(output_lines))


def json_value_from_text(raw: str) -> Any:
    """Estrae il primo array/oggetto JSON, ignorando eventuali avvisi iniziali."""
    decoder = json.JSONDecoder()
    for start, character in enumerate(raw):
        if character not in "[{":
            continue
        try:
            value, _ = decoder.raw_decode(raw[start:])
        except json.JSONDecodeError:
            continue
        if isinstance(value, (list, dict)):
            return value
    raise ValueError("nessun JSON presente")


def parse_devices(payload: Any) -> list[SerialDevice]:
    if isinstance(payload, dict):
        for key in ("devices", "ports", "result"):
            if isinstance(payload.get(key), list):
                payload = payload[key]
                break
    if not isinstance(payload, list):
        raise ValueError("l'elenco delle porte non e un array")

    devices: list[SerialDevice] = []
    for entry in payload:
        if not isinstance(entry, dict):
            continue
        port = str(entry.get("port", "")).strip()
        if not port:
            continue
        devices.append(
            SerialDevice(
                port=port,
                description=str(entry.get("description", "Porta seriale")).strip()
                or "Porta seriale",
                hwid=str(entry.get("hwid", "")).strip(),
            )
        )
    return devices


def detect_devices(
    pio: Sequence[str], fixture: Optional[Path], ui: Ui, dry_run: bool
) -> list[SerialDevice]:
    if fixture is not None:
        try:
            raw = fixture.read_text(encoding="utf-8")
            return parse_devices(json_value_from_text(raw))
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            raise AssistantError(f"File porte JSON non valido: {exc}") from exc

    argv = [*pio, "device", "list", "--json-output"]
    if dry_run:
        ui.command(argv)
        return []
    result = run_capture(argv)
    if result.returncode != 0:
        raise AssistantError(
            "Non riesco a leggere le porte USB. Scollega e ricollega la scheda, poi riprova."
        )
    try:
        return parse_devices(json_value_from_text(result.stdout))
    except (ValueError, json.JSONDecodeError) as exc:
        raise AssistantError(
            "PlatformIO ha risposto, ma l'elenco delle porte non era leggibile."
        ) from exc


def display_devices(devices: Sequence[SerialDevice], ui: Ui) -> None:
    if not devices:
        ui.warn("Nessuna porta seriale rilevata.")
        return
    for index, device in enumerate(devices, start=1):
        likely = " - ESP32 probabile" if device.score >= 80 else ""
        print(f"  {index}. {device.port} - {device.description}{likely}")


def choose_port(
    devices: Sequence[SerialDevice],
    requested: Optional[str],
    interactive: bool,
    assume_yes: bool,
    dry_run: bool,
    ui: Ui,
) -> str:
    if requested:
        match = next((item for item in devices if item.port == requested), None)
        if devices and match is None:
            ui.warn(f"La porta richiesta ({requested}) non compare tra quelle rilevate.")
        ui.ok(f"Porta indicata: {requested}")
        return requested

    ranked = sorted(devices, key=lambda item: (-item.score, item.port.lower()))
    usable = [
        item
        for item in ranked
        if item.score >= 20 or item.port.upper().startswith("COM")
    ]
    if len(usable) == 1:
        ui.ok(f"Scheda rilevata automaticamente: {usable[0].port}")
        return usable[0].port

    if not usable:
        if dry_run:
            ui.info("Simulazione: al momento del flash verra scelta la porta rilevata.")
            return "<PORTA_RILEVATA>"
        raise AssistantError(
            "Nessuna scheda collegata.\n"
            "1. Collega ESP32-S3 con un cavo USB che trasmette dati.\n"
            "2. Attendi qualche secondo.\n"
            "3. Riprova."
        )

    best = usable[0]
    runner_up_score = usable[1].score if len(usable) > 1 else -1000
    if best.score >= 80 and best.score - runner_up_score >= 40:
        ui.ok(f"ESP32 riconosciuta automaticamente: {best.port}")
        return best.port
    if assume_yes and best.score >= 80 and best.score > runner_up_score:
        ui.ok(f"Scheda selezionata automaticamente: {best.port}")
        return best.port

    ui.title("SCEGLI LA SCHEDA")
    display_devices(usable, ui)
    if not interactive:
        raise AssistantError(
            "Sono presenti piu porte. Ripeti il comando aggiungendo --port PORTA."
        )

    while True:
        answer = input("Numero della porta: ").strip()
        try:
            index = int(answer) - 1
        except ValueError:
            index = -1
        if 0 <= index < len(usable):
            return usable[index].port
        ui.warn("Inserisci uno dei numeri mostrati.")


def verify_platformio(pio: Sequence[str], dry_run: bool, ui: Ui) -> str:
    argv = [*pio, "--version"]
    if dry_run:
        ui.command(argv)
        return "PlatformIO (simulazione)"
    result = run_capture(argv)
    if result.returncode != 0:
        raise AssistantError(
            "PlatformIO e presente ma non parte correttamente. Usa --install-platformio "
            "per creare una copia isolata."
        )
    version = result.stdout.strip().splitlines()[-1] if result.stdout.strip() else "PlatformIO"
    return version


def perform_check(
    pio: Optional[Sequence[str]],
    config: ProjectConfig,
    fixture: Optional[Path],
    dry_run: bool,
    ui: Ui,
) -> bool:
    ui.title("CONTROLLO RAPIDO")
    ui.ok(f"Progetto: {PROJECT_ROOT.name}")
    ui.ok(f"Scheda configurata: {config.board}")
    ui.ok(f"Ambiente firmware: {config.environment}")
    ui.ok(f"Python: {platform.python_version()} ({platform.system()})")
    if pio is None:
        ui.error("PlatformIO non trovato.")
        ui.info("Soluzione: python3 tools/flash_dashboard.py --install-platformio")
        return False
    try:
        ui.ok(verify_platformio(pio, dry_run, ui))
        devices = detect_devices(pio, fixture, ui, dry_run)
    except AssistantError as exc:
        ui.error(str(exc))
        return False
    candidates = [
        item
        for item in devices
        if item.score >= 20 or item.port.upper().startswith("COM")
    ]
    if candidates:
        ui.ok(f"Porte USB utilizzabili: {len(candidates)}")
        display_devices(sorted(candidates, key=lambda item: -item.score), ui)
        if not any(item.score >= 80 for item in candidates):
            ui.warn("ESP32 non identificata con certezza; al flash potrai scegliere la porta.")
    elif dry_run:
        ui.info("Rilevamento porte simulato.")
    else:
        ui.warn("Nessuna scheda collegata; la compilazione resta disponibile.")
    return True


def diagnose_failure(phase: str, output: str) -> list[str]:
    text = output.lower()
    advice: list[str] = []
    if any(marker in text for marker in ("permission denied", "access is denied", "operation not permitted")):
        advice.append("Chiudi altri monitor seriali; su Linux verifica i permessi della porta USB.")
    if any(marker in text for marker in ("resource busy", "device or resource busy", "could not open port")):
        advice.append("La porta e occupata: chiudi Arduino IDE, monitor seriali e altri programmi.")
    if any(marker in text for marker in ("failed to connect", "no serial data received", "wrong boot mode")):
        advice.append("Tieni premuto BOOT, premi una volta RESET, poi rilascia BOOT e riprova.")
    if any(marker in text for marker in ("no such file or directory", "device disconnected")):
        advice.append("La scheda si e scollegata: controlla il cavo USB e seleziona di nuovo la porta.")
    if any(marker in text for marker in ("timed out", "timeout", "packet content transfer stopped")):
        advice.append("Prova un altro cavo USB dati o una porta USB diretta, senza hub.")
    if any(marker in text for marker in ("temporary failure in name resolution", "name or service not known", "connectionerror", "sslerror")):
        advice.append("Controlla la connessione Internet: PlatformIO deve scaricare i componenti al primo uso.")
    if any(marker in text for marker in ("no space left", "disk full")):
        advice.append("Libera spazio sul disco e riprova.")
    if "could not find a version that satisfies" in text:
        advice.append("La rete o l'installazione Python non riesce a recuperare una dipendenza richiesta.")
    if not advice:
        if phase == "build":
            advice.append("Controlla le ultime righe sopra: indicano il file e il motivo della compilazione fallita.")
        elif phase == "flash":
            advice.append("Ricollega la scheda con un cavo USB dati e ripeti il flash.")
        else:
            advice.append("Controlla la connessione Internet e riprova.")
    return advice


def run_phase(
    name: str, argv: Sequence[str], phase: str, dry_run: bool, ui: Ui
) -> None:
    ui.title(name)
    ui.command(argv)
    if dry_run:
        ui.ok("Simulazione completata; comando non eseguito.")
        return
    result = stream_command(argv)
    if result.returncode != 0:
        advice = diagnose_failure(phase, result.output)
        message = "\n".join(f"- {item}" for item in advice)
        raise AssistantError(f"Operazione non riuscita.\n{message}", 3 if phase == "build" else 4)


def build_firmware(
    pio: Sequence[str],
    config: ProjectConfig,
    dry_run: bool,
    for_flash: bool,
    ui: Ui,
) -> None:
    argv = [*pio, "run", "--environment", config.environment]
    title = "1/2 - PREPARAZIONE FIRMWARE" if for_flash else "PREPARAZIONE FIRMWARE"
    run_phase(title, argv, "build", dry_run, ui)
    if dry_run:
        ui.ok("Simulazione: nessun firmware e stato compilato.")
    else:
        ui.ok("Firmware preparato correttamente.")


def flash_firmware(
    pio: Sequence[str], config: ProjectConfig, port: str, dry_run: bool, ui: Ui
) -> None:
    argv = [
        *pio,
        "run",
        "--environment",
        config.environment,
        "--target",
        "upload",
        "--upload-port",
        port,
    ]
    run_phase("2/2 - INSTALLAZIONE SULLA SCHEDA", argv, "flash", dry_run, ui)
    if dry_run:
        ui.ok("Simulazione: nessun dato e stato scritto sulla scheda.")
    else:
        ui.ok("FLASH COMPLETATO: la dashboard si riavviera automaticamente.")


def open_monitor(
    pio: Sequence[str], config: ProjectConfig, port: str, dry_run: bool, ui: Ui
) -> None:
    argv = [
        *pio,
        "device",
        "monitor",
        "--environment",
        config.environment,
        "--port",
        port,
        "--baud",
        str(config.monitor_speed),
    ]
    ui.title("MONITOR DIAGNOSTICO")
    ui.info("Per uscire premi Ctrl+C.")
    ui.command(argv)
    if dry_run:
        ui.ok("Simulazione completata; monitor non aperto.")
        return
    try:
        result = subprocess.run(argv, cwd=PROJECT_ROOT, check=False, env=command_environment())
    except FileNotFoundError as exc:
        raise AssistantError("Impossibile aprire il monitor PlatformIO.") from exc
    if result.returncode not in (0, 130):
        raise AssistantError(
            "Monitor non avviato. Verifica che la porta non sia usata da un altro programma."
        )


def ask_yes_no(question: str, default: bool = False) -> bool:
    suffix = "[S/n]" if default else "[s/N]"
    while True:
        answer = input(f"{question} {suffix}: ").strip().lower()
        if not answer:
            return default
        if answer in ("s", "si", "sì", "y", "yes"):
            return True
        if answer in ("n", "no"):
            return False
        print("Rispondi s oppure n.")


def interactive_menu(ui: Ui) -> str:
    ui.title("COSA VUOI FARE?")
    print("  1. Prepara e installa sulla scheda  (consigliato)")
    print("  2. Prepara soltanto il firmware")
    print("  3. Controlla PC, progetto e collegamento USB")
    print("  4. Apri il monitor diagnostico")
    print("  0. Esci")
    while True:
        answer = input("Scelta: ").strip()
        choices = {"1": "flash", "2": "build", "3": "check", "4": "monitor", "0": "exit"}
        if answer in choices:
            return choices[answer]
        ui.warn("Scegli 0, 1, 2, 3 oppure 4.")


def parser_for_cli() -> argparse.ArgumentParser:
    description = """\
Assistente di installazione per ESP32 Dual Screen Dashboard.

Senza opzioni apre un menu semplice. Al primo flash, se PlatformIO manca,
propone di installarlo in modo isolato. Non richiede Homebrew e non modifica
l'installazione Python del computer.
"""
    examples = """\
Esempi:
  python3 tools/flash_dashboard.py
  python3 tools/flash_dashboard.py --check
  python3 tools/flash_dashboard.py --flash
  python3 tools/flash_dashboard.py --flash --port /dev/cu.usbmodem101
  py tools\\flash_dashboard.py --flash --port COM5
  python3 tools/flash_dashboard.py --flash --monitor
  python3 tools/flash_dashboard.py --dry-run --flash --port PORTA_TEST
"""
    parser = argparse.ArgumentParser(
        description=textwrap.dedent(description),
        epilog=textwrap.dedent(examples),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    actions = parser.add_argument_group("operazioni")
    actions.add_argument("--check", action="store_true", help="controlla requisiti e porte USB")
    actions.add_argument("--build", action="store_true", help="compila il firmware senza installarlo")
    actions.add_argument("--flash", action="store_true", help="compila e installa il firmware sulla scheda")
    actions.add_argument("--monitor", action="store_true", help="apre il monitor seriale (anche dopo --flash)")
    actions.add_argument(
        "--install-platformio",
        action="store_true",
        help="installa PlatformIO localmente in .tools/platformio-venv",
    )
    options = parser.add_argument_group("opzioni")
    options.add_argument("--port", metavar="PORTA", help="porta USB, per esempio COM5 o /dev/cu.usbmodem101")
    options.add_argument("--env", metavar="AMBIENTE", help="ambiente PlatformIO (normalmente rilevato da solo)")
    options.add_argument("--yes", action="store_true", help="accetta la selezione automatica non ambigua")
    options.add_argument("--dry-run", action="store_true", help="mostra i comandi senza eseguirli")
    options.add_argument("--no-color", action="store_true", help="disattiva i colori del terminale")
    options.add_argument(
        "--ports-json",
        type=Path,
        metavar="FILE",
        help=argparse.SUPPRESS,
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {SCRIPT_VERSION}")
    return parser


def print_banner(ui: Ui, config: ProjectConfig) -> None:
    print()
    print(ui._paint("=" * 62, "1;36"))
    print(ui._paint("  ESP32 DUAL SCREEN - INSTALLAZIONE GUIDATA", "1;36"))
    print(ui._paint("=" * 62, "1;36"))
    print(f"  Scheda: {config.board}")
    print(f"  Profilo: {config.environment}")
    print("  Il firmware originale non viene modificato.")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = parser_for_cli()
    args = parser.parse_args(argv)
    use_color = not args.no_color and not os.environ.get("NO_COLOR") and sys.stdout.isatty()
    ui = Ui(use_color)

    try:
        config = read_project_config(args.env)
        print_banner(ui, config)
        pio = find_platformio()

        has_explicit_action = any(
            (args.check, args.build, args.flash, args.monitor, args.install_platformio)
        )
        interactive = not has_explicit_action
        if interactive and not sys.stdin.isatty():
            raise AssistantError(
                "Il menu interattivo richiede un terminale. Usa --flash, --build oppure --check."
            )

        if args.install_platformio:
            pio = install_local_platformio(ui, args.dry_run)
            if not any((args.check, args.build, args.flash, args.monitor)):
                return 0

        if pio is None and args.check and not any((args.build, args.flash, args.monitor)):
            return 0 if perform_check(None, config, args.ports_json, args.dry_run, ui) else 1

        if pio is None and interactive:
            ui.warn("PlatformIO non e ancora installato.")
            if ask_yes_no("Vuoi installarlo ora in modo isolato?", default=True):
                pio = install_local_platformio(ui, args.dry_run)
            else:
                raise AssistantError("PlatformIO e necessario per preparare il firmware.")

        if (
            pio is None
            and not args.dry_run
            and any((args.build, args.flash, args.monitor))
            and sys.stdin.isatty()
        ):
            ui.warn("PlatformIO non e ancora installato.")
            if args.yes or ask_yes_no(
                "Vuoi installarlo ora in modo isolato?", default=True
            ):
                pio = install_local_platformio(ui, dry_run=False)
            else:
                raise AssistantError("PlatformIO e necessario per preparare il firmware.")

        if pio is None and args.dry_run:
            pio = ["pio"]
            ui.info("Simulazione: uso il nome comando 'pio'.")
        if pio is None:
            raise AssistantError(
                "PlatformIO non trovato. Esegui:\n"
                "  python3 tools/flash_dashboard.py --install-platformio\n"
                "Non serve Homebrew e l'installazione resta dentro il progetto."
            )

        action = interactive_menu(ui) if interactive else ""
        if action == "exit":
            ui.info("Nessuna modifica effettuata.")
            return 0

        do_check = args.check or action == "check"
        do_build = args.build or args.flash or action in ("build", "flash")
        do_flash = args.flash or action == "flash"
        do_monitor = args.monitor or action == "monitor"

        if do_check:
            check_ok = perform_check(pio, config, args.ports_json, args.dry_run, ui)
            if not check_ok:
                return 1
            if not any((do_build, do_flash, do_monitor)):
                return 0
        else:
            version = verify_platformio(pio, args.dry_run, ui)
            ui.ok(version)

        port: Optional[str] = None
        if do_flash or do_monitor:
            ui.title("RICERCA SCHEDA USB")
            devices = (
                []
                if args.port
                else detect_devices(pio, args.ports_json, ui, args.dry_run)
            )
            port = choose_port(
                devices,
                args.port,
                interactive=interactive,
                assume_yes=args.yes,
                dry_run=args.dry_run,
                ui=ui,
            )

        if do_build:
            build_firmware(pio, config, args.dry_run, do_flash, ui)
        if do_flash:
            assert port is not None
            flash_firmware(pio, config, port, args.dry_run, ui)
            if interactive and not do_monitor:
                do_monitor = ask_yes_no("Vuoi aprire il monitor diagnostico?", default=False)
        if do_monitor:
            assert port is not None
            open_monitor(pio, config, port, args.dry_run, ui)

        ui.title("OPERAZIONE TERMINATA")
        ui.ok("Tutto completato.")
        return 0
    except AssistantError as exc:
        ui.error(str(exc))
        return exc.exit_code
    except KeyboardInterrupt:
        print()
        ui.warn("Operazione interrotta dall'utente.")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
