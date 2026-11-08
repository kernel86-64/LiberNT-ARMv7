#!/usr/bin/env python3
"""
PROJECT:     LiberNT Build
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Set the UEFI ARM machine identifier on a linked Thumb PE32 application
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""
import argparse
from pathlib import Path
import struct


def convert(data):
    image = bytearray(data)
    if len(image) < 64 or image[:2] != b'MZ':
        raise ValueError('Expected a DOS header')
    offset = struct.unpack_from('<I', image, 0x3c)[0]
    if offset + 96 > len(image) or image[offset:offset + 4] != b'PE\0\0':
        raise ValueError('Expected a complete PE32 header')
    machine = struct.unpack_from('<H', image, offset + 4)[0]
    magic = struct.unpack_from('<H', image, offset + 24)[0]
    subsystem = struct.unpack_from('<H', image, offset + 92)[0]
    if machine not in (0x1c4, 0x1c2) or magic != 0x10b or subsystem != 10:
        raise ValueError('Expected an ARM Thumb PE32 EFI application')
    struct.pack_into('<H', image, offset + 4, 0x1c2)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        image = convert(args.image.read_bytes())
    except (OSError, ValueError) as error:
        parser.error(str(error))
    args.image.write_bytes(image)


if __name__ == '__main__':
    main()
