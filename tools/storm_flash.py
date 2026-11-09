#!/usr/bin/env python3
"""Verified, data-preserving initial stormDASH USB installation.
Use portal OTA for subsequent updates after a device has switched OTA slots.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_BYTES = 0x1000000
REGIONS = {"bootloader.bin": (0, 0x8000),
           "partitions.bin": (0x8000, 0x1000),
           "firmware.bin": (0x10000, 0x300000)}
# Exact order, labels, flags and extents: matching only the first four entries
# would silently replace unknown partitions in the upper half of the flash.
PARTITIONS = (
    (1, 2, 0x9000, 0x5000, "nvs", 0),
    (1, 0, 0xE000, 0x2000, "otadata", 0),
    (0, 16, 0x10000, 0x300000, "app0", 0),
    (1, 130, 0x310000, 0x4F0000, "littlefs", 0),
    (0, 17, 0x800000, 0x300000, "app1", 0),
    (1, 130, 0xB00000, 0x500000, "stormdata", 0),
)


class FlashError(RuntimeError):
    pass


def partition_entries(data):
    """Parse the complete ESP-IDF table, including its optional MD5 record."""
    if len(data) < 32 or len(data) > 0x1000 or len(data) % 32:
        raise FlashError("Invalid partition-table length.")
    found = []
    for at in range(0, len(data), 32):
        entry = data[at:at + 32]
        magic = int.from_bytes(entry[:2], "little")
        if magic == 0xFFFF:
            if any(byte != 0xFF for byte in data[at:]):
                raise FlashError("Unexpected data after partition-table terminator.")
            return tuple(found)
        if magic == 0xEBEB:
            if entry[:16] != b"\xeb\xeb" + b"\xff" * 14:
                raise FlashError("Invalid partition checksum record.")
            if entry[16:] != hashlib.md5(data[:at]).digest():
                raise FlashError("Partition-table MD5 mismatch.")
            if any(byte != 0xFF for byte in data[at + 32:]):
                raise FlashError("Unexpected data after partition checksum.")
            return tuple(found)
        if magic != 0x50AA:
            raise FlashError("Invalid partition-table entry.")
        _, kind, subtype, offset, size, raw_label, flags = struct.unpack(
            "<HBBII16sI", entry)
        try:
            label = raw_label.split(b"\0", 1)[0].decode("ascii")
        except UnicodeDecodeError as exc:
            raise FlashError("Invalid partition label.") from exc
        if raw_label != label.encode().ljust(16, b"\0"):
            raise FlashError("Invalid partition-label padding.")
        found.append((kind, subtype, offset, size, label, flags))
    raise FlashError("Partition-table terminator is missing.")


def verified(folder):
    folder = Path(folder)
    try:
        manifest = json.loads((folder / "manifest.json").read_text())
    except (OSError, ValueError) as exc:
        raise FlashError("Manifest is missing or invalid.") from exc
    if (not isinstance(manifest, dict) or manifest.get("product") != "stormDASH"
            or manifest.get("chip") != "esp32s3"
            or type(manifest.get("flash_bytes")) is not int
            or manifest["flash_bytes"] != FLASH_BYTES):
        raise FlashError("Expected stormDASH ESP32-S3 / 16 MB manifest.")
    files = manifest.get("files")
    if not isinstance(files, list) or len(files) != len(REGIONS):
        raise FlashError("Manifest must contain exactly the three release binaries.")
    seen = set()
    for item in files:
        if not isinstance(item, dict):
            raise FlashError("Invalid manifest file record.")
        name = item.get("file")
        if not isinstance(name, str) or name not in REGIONS or name in seen:
            raise FlashError("Unexpected or duplicate binary name.")
        seen.add(name)
        offset, limit = REGIONS[name]
        if type(item.get("offset")) is not int or item["offset"] != offset:
            raise FlashError("Unexpected write address: " + name)
        size, checksum = item.get("size"), item.get("sha256")
        if type(size) is not int or not 0 < size <= limit:
            raise FlashError("Region overflow or invalid binary size: " + name)
        if not isinstance(checksum, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", checksum):
            raise FlashError("Invalid SHA-256: " + name)
        try:
            data = (folder / name).read_bytes()
        except OSError as exc:
            raise FlashError("Binary is missing or unreadable: " + name) from exc
        if len(data) != size or hashlib.sha256(data).hexdigest() != checksum.lower():
            raise FlashError("Hash or length mismatch: " + name)
        if name == "partitions.bin":
            if partition_entries(data) != PARTITIONS:
                raise FlashError("Release partition layout does not match stormDASH.")
        elif (len(data) < 24 or data[0] != 0xE9
              or int.from_bytes(data[12:14], "little") != 9):
            raise FlashError("Binary is not an ESP32-S3 image: " + name)
    return manifest


def check_device_layout(data):
    """data is the full 0x8000..0x10000 read, before any write is allowed."""
    if len(data) != 0x8000:
        raise FlashError("Incomplete device-layout read; no write performed.")
    entries = partition_entries(data[:0x1000])
    if entries not in (PARTITIONS[:4], PARTITIONS):
        raise FlashError("Existing partitions differ; no migration or erase performed.")
    # Refuse both valid selections and partially written/corrupt metadata.
    if data[0x6000:0x8000] != b"\xff" * 0x2000:
        raise FlashError("OTA selection or metadata is present. Use local portal OTA; "
                         "USB installation does not reset OTA metadata.")
    return entries


def check_backup(data, checked_layout):
    if len(data) != FLASH_BYTES:
        raise FlashError("Incomplete backup; no write performed.")
    entries = check_device_layout(data[0x8000:0x10000])
    if data[0x8000:0x10000] != checked_layout:
        raise FlashError("Device layout or settings changed during backup; no write performed.")
    if entries == PARTITIONS[:4] and data[0x800000:] != b"\xff" * 0x800000:
        raise FlashError("Upper 8 MB contain data; manual migration is required. No write performed.")


def esptool(port, *args, capture=False, first=False, reboot=False):
    # Keep the CPU in ROM from identification through backup and validation.
    # Letting it boot between reads could change NVS or OTA selection mid-check.
    return subprocess.run(
        [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port,
         "--baud", "921600", "--before", "default_reset" if first else "no_reset",
         "--after", "hard_reset" if reboot else "no_reset", *map(str, args)],
        check=True, capture_output=capture, text=capture)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--verify", action="store_true")
    mode.add_argument("--install", action="store_true")
    ap.add_argument("--port")
    ap.add_argument("--binaries", type=Path, default=ROOT / "output/stormDASH/BINARI")
    ap.add_argument("--backup-dir", type=Path, default=ROOT / "private-recovery")
    args = ap.parse_args(argv)
    manifest = verified(args.binaries)
    print("stormDASH: hashes, image headers, partition layout and write regions verified.")
    if not args.install:
        return 0
    if not args.port:
        ap.error("--install requires --port")
    backup = args.backup_dir / "before-stormDASH-16MB.bin"
    checksum_path = backup.with_suffix(".sha256")
    for path in (backup, checksum_path):
        if path.exists() or path.is_symlink():
            raise FlashError("Backup already exists. Choose a fresh --backup-dir; "
                             "existing backups are never overwritten.")
    with tempfile.TemporaryDirectory(prefix="stormdash-install-") as directory:
        staging = Path(directory)
        # Write only these private snapshots, not files that could be edited
        # in the extracted release while a lengthy device backup is running.
        for item in manifest["files"]:
            (staging / item["file"]).write_bytes((args.binaries / item["file"]).read_bytes())
        (staging / "manifest.json").write_text(json.dumps(manifest))
        verified(staging)
        info = esptool(args.port, "flash_id", capture=True, first=True).stdout
        if ("Detected flash size: 16MB" not in info
                or "Embedded PSRAM 8MB (AP_3v3)" not in info):
            raise FlashError("Expected N16R8 hardware not identified; no write performed.")
        layout_path = staging / "device-layout.bin"
        esptool(args.port, "read_flash", "0x8000", "0x8000", layout_path)
        checked_layout = layout_path.read_bytes()
        check_device_layout(checked_layout)
        args.backup_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
        # Reserve the path exclusively; private flash dumps contain credentials.
        with backup.open("xb"):
            pass
        backup.chmod(0o600)
        esptool(args.port, "read_flash", "0", "0x1000000", backup)
        data = backup.read_bytes()
        check_backup(data, checked_layout)
        with checksum_path.open("x") as output:
            output.write(hashlib.sha256(data).hexdigest() + "\n")
        write_args = ["write_flash", "--flash_size", "16MB",
                      "--flash_mode", "keep", "--flash_freq", "keep"]
        for name, (offset, _) in REGIONS.items():
            write_args += [hex(offset), str(staging / name)]
        esptool(args.port, *write_args, reboot=True)
    print("Installed; NVS, OTA data and LittleFS were not written. Keep the private backup.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FlashError, OSError, subprocess.CalledProcessError) as exc:
        print("Installation stopped: " + str(exc), file=sys.stderr)
        print("No automatic erase/recovery was attempted. If USB was accessed, "
              "reset the board to leave ROM download mode.", file=sys.stderr)
        sys.exit(1)
