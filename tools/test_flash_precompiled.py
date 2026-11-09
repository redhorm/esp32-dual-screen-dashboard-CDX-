#!/usr/bin/env python3
"""Offline checks: python3 -m unittest discover -s tools -p test_flash_precompiled.py"""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

import flash_precompiled as flash


class PrecompiledFlashTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="recoil-flash-test-")
        self.addCleanup(self.directory.cleanup)
        self.bins = Path(self.directory.name)
        self.manifest = {"schema": 1, "chip": "esp32s3", "flash_size_bytes": 0x800000, "files": []}
        for name, offset, _ in flash.REGIONS:
            if name == "partitions.bin":
                data = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, start, size,
                                             label.encode(), 0)
                                for label, (kind, subtype, start, size) in flash.EXPECTED_PARTITIONS.items())
                data += b"\xff" * (3072 - len(data))
            elif name == "boot_app0.bin":
                data = b"\xff" * 8192
            else:
                header = bytearray(24)
                header[0] = 0xE9
                header[12] = 9
                data = bytes(header) + b"\0" * 100
            (self.bins / name).write_bytes(data)
            self.manifest["files"].append({"name": name, "offset": hex(offset), "size": len(data),
                                          "sha256": hashlib.sha256(data).hexdigest()})
        self.save_manifest()

    def save_manifest(self):
        (self.bins / flash.MANIFEST_NAME).write_text(json.dumps(self.manifest))

    def test_missing_binary_fails_before_any_device_access(self):
        (self.bins / "firmware.bin").unlink()
        with self.assertRaisesRegex(flash.FlashError, "mancante"):
            flash.validate_release(self.bins)

    def test_changed_byte_fails_checksum(self):
        binary = self.bins / "firmware.bin"
        data = bytearray(binary.read_bytes()); data[-1] ^= 1; binary.write_bytes(data)
        with self.assertRaisesRegex(flash.FlashError, "SHA-256 non corrispondente"):
            flash.validate_release(self.bins)

    def test_changed_address_and_erase_boundary_are_rejected(self):
        self.manifest["files"][0]["offset"] = "0x1000"
        self.save_manifest()
        with self.assertRaisesRegex(flash.FlashError, "Indirizzo errato"):
            flash.validate_release(self.bins)
        self.manifest["files"][0]["offset"] = "0x0"
        self.manifest["files"][0]["size"] = 0x8001
        self.save_manifest()
        with self.assertRaisesRegex(flash.FlashError, "supera la regione"):
            flash.validate_release(self.bins)

    def test_other_partition_layout_is_rejected_even_with_matching_checksum(self):
        entry = next(e for e in self.manifest["files"] if e["name"] == "partitions.bin")
        binary = self.bins / "partitions.bin"
        data = bytearray(binary.read_bytes())
        struct.pack_into("<I", data, 4, 0xA000)  # NVS moved.
        binary.write_bytes(data)
        entry["sha256"] = hashlib.sha256(data).hexdigest()
        self.save_manifest()
        with self.assertRaisesRegex(flash.FlashError, "non corrisponde a RECOIL"):
            flash.validate_release(self.bins)

    def test_invalid_manifest_and_duplicate_entries_are_rejected(self):
        (self.bins / flash.MANIFEST_NAME).write_text("{")
        with self.assertRaisesRegex(flash.FlashError, "Manifest mancante"):
            flash.validate_release(self.bins)
        self.manifest["files"][1] = self.manifest["files"][0]
        self.save_manifest()
        with self.assertRaisesRegex(flash.FlashError, "duplicate"):
            flash.validate_release(self.bins)

    def test_dry_run_validates_without_dependency_check_or_subprocess(self):
        with mock.patch.object(flash, "require_esptool4") as dependency, \
                mock.patch.object(flash.subprocess, "run") as run, \
                contextlib.redirect_stdout(io.StringIO()) as out:
            result = flash.main(["--port", "PORTA_TEST", "--bin-dir", str(self.bins), "--dry-run"])
        self.assertEqual(result, 0)
        dependency.assert_not_called()
        run.assert_not_called()
        self.assertIn("nessun accesso USB", out.getvalue())
        self.assertNotIn("erase_all", out.getvalue())

    def test_write_command_contains_only_fixed_regions_and_keep_flags(self):
        command = flash.build_command("COM5", 460800, flash.validate_release(self.bins))
        self.assertEqual(command[command.index("--chip") + 1], "esp32s3")
        self.assertEqual(command[command.index("--port") + 1], "COM5")
        for flag in ("--flash_size", "--flash_mode", "--flash_freq"):
            self.assertEqual(command[command.index(flag) + 1], "keep")
        self.assertNotIn("--erase-all", command)
        self.assertEqual(command[-8::2], ["0x0", "0x8000", "0xe000", "0x10000"])

    def test_esptool5_is_rejected_with_isolated_dependency_instructions(self):
        with mock.patch.object(flash.importlib.metadata, "version", return_value="5.0.0"):
            with self.assertRaisesRegex(flash.FlashError, "esptool==4.11.0"):
                flash.require_esptool4()


if __name__ == "__main__":
    unittest.main()
