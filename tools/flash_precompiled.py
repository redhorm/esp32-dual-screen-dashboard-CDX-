#!/usr/bin/env python3
"""Install the packaged ESP32-S3 binaries without rebuilding the project.

Expected release layout:
  RELEASE/BINARI/flash_manifest.json and the four .bin files
  RELEASE/Progetto_Recoil_Pulse/tools/flash_precompiled.py

Dry-run validates the release and prints the exact command without importing
esptool, discovering ports, spawning a child process, or touching USB.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys

FLASH_BYTES = 0x800000
DEFAULT_BIN_DIR = Path(__file__).resolve().parents[2] / "BINARI"
MANIFEST_NAME = "flash_manifest.json"
# Values are fixed by the delivered ESP32-S3 partition layout, not by arbitrary
# addresses supplied in a manifest. Limits include erase-sector rounding.
REGIONS = (
    ("bootloader.bin", 0x0, 0x8000),
    ("partitions.bin", 0x8000, 0x9000),
    ("boot_app0.bin", 0xE000, 0x10000),
    ("firmware.bin", 0x10000, 0x310000),
)
EXPECTED_PARTITIONS = {
    "nvs": (1, 2, 0x9000, 0x5000),
    "otadata": (1, 0, 0xE000, 0x2000),
    "app0": (0, 16, 0x10000, 0x300000),
    "littlefs": (1, 130, 0x310000, 0x4F0000),
}


class FlashError(Exception):
    pass


def format_command(command):
    return subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)


def dependency_help():
    interpreter = ".flash-venv\\Scripts\\python.exe" if os.name == "nt" else ".flash-venv/bin/python"
    return ("Serve esptool 4.x. Crea un ambiente isolato, poi esegui lo script con quel Python:\n"
            f"  {format_command([sys.executable, '-m', 'venv', '.flash-venv'])}\n"
            f"  {interpreter} -m pip install esptool==4.11.0\n"
            f"  {interpreter} {format_command([str(Path(__file__).resolve())])} --port PORTA")


def require_esptool4():
    try:
        version = importlib.metadata.version("esptool")
    except importlib.metadata.PackageNotFoundError as exc:
        raise FlashError(dependency_help()) from exc
    if not re.match(r"^4\.", version):
        raise FlashError(f"Versione esptool trovata: {version}.\n" + dependency_help())
    return version


def validate_partition_table(data):
    found = {}
    for at in range(0, len(data) - 31, 32):
        entry = data[at:at + 32]
        magic = int.from_bytes(entry[:2], "little")
        if magic in (0xFFFF, 0xEBEB):
            break  # End marker or standard ESP-IDF table checksum.
        if magic != 0x50AA:
            raise FlashError("partitions.bin contiene una tabella non valida.")
        _, kind, subtype, offset, size, raw_label, flags = struct.unpack("<HBBII16sI", entry)
        try:
            label = raw_label.split(b"\0", 1)[0].decode("ascii")
        except UnicodeDecodeError as exc:
            raise FlashError("Etichetta partizione non valida.") from exc
        if label in found or flags != 0 or offset + size > FLASH_BYTES:
            raise FlashError("Tabella partizioni duplicata, protetta o fuori dai limiti di 8 MB.")
        found[label] = (kind, subtype, offset, size)
    if found != EXPECTED_PARTITIONS:
        raise FlashError("La tabella partizioni non corrisponde a RECOIL PULSE (NVS, app0 e LittleFS).")


def validate_release(bin_dir):
    bin_dir = Path(bin_dir).resolve()
    try:
        manifest = json.loads((bin_dir / MANIFEST_NAME).read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise FlashError(f"Manifest mancante o non leggibile: {bin_dir / MANIFEST_NAME}") from exc
    if not isinstance(manifest, dict) or manifest.get("schema") != 1 or manifest.get("chip") != "esp32s3" or manifest.get("flash_size_bytes") != FLASH_BYTES:
        raise FlashError("Manifest non compatibile: richiesti schema 1, ESP32-S3 e layout 8 MB.")
    entries = manifest.get("files")
    if not isinstance(entries, list) or len(entries) != len(REGIONS):
        raise FlashError("Il manifest deve contenere i quattro binari del pacchetto.")
    indexed = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("name"), str) or entry["name"] in indexed:
            raise FlashError("Voci manifest non valide o duplicate.")
        indexed[entry["name"]] = entry
    if set(indexed) != {name for name, _, _ in REGIONS}:
        raise FlashError("Nomi dei binari inattesi: usa il pacchetto completo senza rinominare i file.")

    validated = []
    for name, offset, limit in REGIONS:
        entry = indexed[name]
        raw_offset = entry.get("offset")
        try:
            declared_offset = int(raw_offset, 0) if isinstance(raw_offset, str) else int(raw_offset)
        except (ValueError, TypeError) as exc:
            raise FlashError(f"Indirizzo mancante o non valido per {name}.") from exc
        if declared_offset != offset:
            raise FlashError(f"Indirizzo errato per {name}: deve essere {offset:#x}.")
        size = entry.get("size")
        checksum = entry.get("sha256")
        if type(size) is not int or size <= 0 or not isinstance(checksum, str) or re.fullmatch(r"[a-fA-F0-9]{64}", checksum) is None:
            raise FlashError(f"Dimensione o SHA-256 non validi per {name}.")
        rounded_end = offset + ((size + 4095) // 4096) * 4096
        if rounded_end > limit:
            raise FlashError(f"{name} supera la regione prevista e potrebbe modificare dati conservati.")
        path = bin_dir / name
        try:
            actual_size = path.stat().st_size
            if not path.is_file() or actual_size != size:
                raise FlashError(f"Dimensione errata o file non valido: {name}.")
            data = path.read_bytes()
        except OSError as exc:
            raise FlashError(f"File mancante o non leggibile: {path}") from exc
        if hashlib.sha256(data).hexdigest() != checksum.lower():
            raise FlashError(f"SHA-256 non corrispondente per {name}: ripristina il file dal pacchetto.")
        if name == "partitions.bin":
            validate_partition_table(data)
        if name in ("bootloader.bin", "firmware.bin"):
            # ESP32 image header: magic E9; extended-header chip ID 9 = ESP32-S3.
            if len(data) < 24 or data[0] != 0xE9 or int.from_bytes(data[12:14], "little") != 9:
                raise FlashError(f"{name} non contiene un'immagine ESP32-S3 riconoscibile.")
        validated.append((offset, path))
    return validated


def build_command(port, baud, validated):
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port,
               "--baud", str(baud), "--before", "default_reset", "--after", "hard_reset",
               "write_flash", "--flash_size", "keep", "--flash_mode", "keep", "--flash_freq", "keep"]
    for offset, path in validated:
        command.extend([hex(offset), str(path)])
    return command


def main(argv=None):
    parser = argparse.ArgumentParser(description="Carica i binari RECOIL PULSE verificati senza ricompilare.")
    parser.add_argument("--port", required=True, help="Porta esplicita, per esempio COM5 o /dev/cu.usbmodem101")
    parser.add_argument("--bin-dir", type=Path, default=DEFAULT_BIN_DIR, help="Cartella BINARI del pacchetto")
    parser.add_argument("--baud", type=int, choices=(115200, 460800, 921600), default=921600)
    parser.add_argument("--dry-run", action="store_true", help="Verifica file e comando senza accesso USB")
    args = parser.parse_args(argv)
    try:
        port = args.port.strip()
        if not port:
            raise FlashError("Indica una porta seriale con --port.")
        validated = validate_release(args.bin_dir)
        command = build_command(port, args.baud, validated)
        print("Verificati i quattro binari, SHA-256 e layout delle partizioni.")
        print("Scrittura prevista: bootloader, tabella partizioni, dati di avvio e applicazione.")
        print("Le regioni NVS e LittleFS non sono incluse nella scrittura. Nessuna cancellazione completa.")
        print(format_command(command))
        if args.dry_run:
            print("Simulazione completata: nessun processo esptool avviato e nessun accesso USB.")
            return 0
        version = require_esptool4()
        print(f"Avvio esptool {version} sulla porta {port}.", flush=True)
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            print("Caricamento non completato: controlla il messaggio esptool qui sopra.", file=sys.stderr)
            return result.returncode if result.returncode > 0 else 1
        print("Scrittura completata e verificata da esptool. Riavvio richiesto alla scheda; verifica i display.")
        return 0
    except (FlashError, OSError) as exc:
        print(f"Errore: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("Caricamento interrotto.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
