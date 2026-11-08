#!/usr/bin/env python3
"""
PROJECT:     LiberNT build tests
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Validate ARM EFI machine conversion and rejection of other image types
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('arm_uefi', Path(__file__).resolve().parents[1] / 'arm_uefi.py')
arm_uefi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(arm_uefi)


class ArmUefiTest(unittest.TestCase):
    def image(self, machine=0x1c4, magic=0x10b, subsystem=10):
        data = bytearray(512)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 60, 128)
        data[128:132] = b'PE\0\0'
        struct.pack_into('<H', data, 132, machine)
        struct.pack_into('<H', data, 152, magic)
        struct.pack_into('<H', data, 220, subsystem)
        return data

    def test_only_machine_changes(self):
        original = self.image()
        converted = arm_uefi.convert(original)
        self.assertEqual(original[132:134], b'\xc4\x01')
        original[132:134] = b'\xc2\x01'
        self.assertEqual(converted, original)
        self.assertEqual(arm_uefi.convert(converted), converted)

    def test_rejects_wrong_architecture_or_subsystem(self):
        for machine, magic, subsystem in [(0xaa64, 0x20b, 10), (0x14c, 0x10b, 10), (0x1c4, 0x10b, 1)]:
            with self.subTest(machine=machine, magic=magic, subsystem=subsystem):
                with self.assertRaises(ValueError):
                    arm_uefi.convert(self.image(machine, magic, subsystem))

    def test_rejects_truncated_or_invalid_headers(self):
        for data in [b'', b'MZ', self.image()[:150], bytes(512)]:
            with self.subTest(length=len(data)):
                with self.assertRaises(ValueError):
                    arm_uefi.convert(data)
        data = self.image()
        struct.pack_into('<I', data, 60, 0xffffffff)
        with self.assertRaises(ValueError):
            arm_uefi.convert(data)


if __name__ == '__main__':
    unittest.main()
