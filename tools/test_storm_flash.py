#!/usr/bin/env python3
"""Host-only checks of destructive-operation guards; never opens a serial port."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import storm_flash as flash


def table(entries):
    raw = b''.join(struct.pack('<HBBII16sI', 0x50AA, t, st, at, size, name.encode(), flags)
                   for t, st, at, size, name, flags in entries)
    raw += b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(raw).digest()
    return raw.ljust(0x1000, b'\xff')


def layout(entries=flash.PARTITIONS[:4]):
    data = bytearray(b'\xff' * 0x8000)
    data[:0x1000] = table(entries)
    return data


class FlashGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name)
        image = bytearray(64); image[0] = 0xe9; image[12] = 9
        files = []
        for name, (offset, _) in flash.REGIONS.items():
            data = table(flash.PARTITIONS) if name == 'partitions.bin' else bytes(image)
            (self.folder / name).write_bytes(data)
            files.append(dict(file=name, offset=offset, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
        self.manifest = dict(product='stormDASH', chip='esp32s3', flash_bytes=flash.FLASH_BYTES, files=files)
        self.save()

    def tearDown(self):
        self.temp.cleanup()

    def save(self):
        (self.folder / 'manifest.json').write_text(json.dumps(self.manifest))

    def test_valid_package(self):
        self.assertEqual(flash.verified(self.folder)['product'], 'stormDASH')

    def test_modified_binary_refused(self):
        (self.folder / 'firmware.bin').write_bytes(b'corrupted')
        with self.assertRaises(flash.FlashError): flash.verified(self.folder)

    def test_duplicate_and_outside_paths_refused(self):
        for name in ['bootloader.bin', '../firmware.bin']:
            self.manifest['files'][-1]['file'] = name; self.save()
            with self.assertRaises(flash.FlashError): flash.verified(self.folder)

    def test_address_and_oversize_refused(self):
        self.manifest['files'][-1]['offset'] = 0; self.save()
        with self.assertRaises(flash.FlashError): flash.verified(self.folder)
        self.manifest['files'][-1]['offset'] = 0x10000
        self.manifest['files'][-1]['size'] = 0x300001; self.save()
        with self.assertRaises(flash.FlashError): flash.verified(self.folder)

    def test_legacy_and_storm_layout_accepted(self):
        for entries in (flash.PARTITIONS[:4], flash.PARTITIONS):
            self.assertEqual(flash.check_device_layout(layout(entries)), entries)

    def test_unknown_upper_partition_refused(self):
        entries = flash.PARTITIONS[:4] + ((1, 130, 0x800000, 0x800000, 'userdata', 0),)
        with self.assertRaises(flash.FlashError): flash.check_device_layout(layout(entries))

    def test_any_ota_metadata_refused(self):
        for at in [0x6000, 0x6018, 0x7000, 0x7fff]:
            data = layout(); data[at] = 0
            with self.assertRaises(flash.FlashError): flash.check_device_layout(data)

    def test_table_checksum_and_extra_data_refused(self):
        for at in [12, 6 * 32 + 16, 0xfff]:
            data = layout(flash.PARTITIONS); data[at] ^= 1
            with self.assertRaises(flash.FlashError): flash.check_device_layout(data)

    def test_backup_upper_data_and_size_guard(self):
        data = bytearray(b'\xff' * flash.FLASH_BYTES); checked = layout()
        data[0x8000:0x10000] = checked; flash.check_backup(data, checked)
        data[-1] = 0
        with self.assertRaises(flash.FlashError): flash.check_backup(data, checked)
        with self.assertRaises(flash.FlashError): flash.check_backup(data[:-1], checked)

    def test_backup_layout_must_match_checked_snapshot(self):
        data = bytearray(b'\xff' * flash.FLASH_BYTES); checked = layout()
        data[0x8000:0x10000] = checked; data[0x9000] = 0
        with self.assertRaises(flash.FlashError): flash.check_backup(data, checked)

    def test_verification_never_invokes_esptool(self):
        with patch.object(flash, 'esptool', side_effect=AssertionError('USB forbidden')):
            self.assertEqual(flash.main(['--verify', '--binaries', str(self.folder)]), 0)

    def test_checks_remain_enabled_under_python_optimized(self):
        (self.folder / 'firmware.bin').write_bytes(b'corrupted')
        result = subprocess.run([sys.executable, '-O', str(Path(flash.__file__)), '--verify', '--binaries', str(self.folder)], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'mismatch', result.stderr)


if __name__ == '__main__': unittest.main(verbosity=2)
