#!/usr/bin/env python3
"""
PROJECT:     LiberNT API tests
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Validate linked ARM Win32u stubs against the kernel service table
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""
import re
import struct
import subprocess
import sys
from pathlib import Path


def check(binary, readobj, compiler):
    root = Path(__file__).resolve().parents[4]
    cache = (binary.resolve().parents[2] / 'CMakeCache.txt').read_text().splitlines()
    debug = int('CMAKE_BUILD_TYPE:STRING=Debug' in cache)
    table = subprocess.check_output([compiler, '--target=armv7-pc-windows-msvc',
        '-E', '-P', '-x', 'c', f'-DDBG={debug}', str(root / 'win32ss/w32ksvc32.h')], text=True)
    services = re.findall(r'^\s*SVC_\((\w+),\s*\d+\)', table, re.MULTILINE)
    base = int(re.search(r'#define MIN_SYSCALL_NUMBER\s+(0x[\da-fA-F]+)',
                        (root / 'win32ss/napi.h').read_text()).group(1), 16)
    data = binary.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    if machine != 0x1c4:
        raise AssertionError('expected ARMNT image')
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for index in range(count):
        at = pe + 24 + optional_size + index * 40
        size, rva, raw_size, raw = struct.unpack_from('<IIII', data, at + 8)
        sections.append((rva, max(size, raw_size), raw))
    exports = subprocess.check_output([readobj, '--coff-exports', str(binary)], text=True)
    addresses = {name: int(rva, 16) for name, rva in
                 re.findall(r'Name: (\S+)\n  RVA: (0x[\da-fA-F]+)', exports)}
    checked = 0
    for index, service in enumerate(services):
        name = 'Nt' + service
        if name not in addresses:
            continue
        rva = addresses[name]
        if not rva & 1:
            raise AssertionError(f'{name}: missing Thumb entry bit')
        address = rva & ~1
        section = next(item for item in sections if item[0] <= address < item[0] + item[1])
        at = section[2] + address - section[0]
        first, second, svc, ret = struct.unpack_from('<HHHH', data, at)
        if first & 0xfbf0 != 0xf240 or (second >> 8) & 15 != 12 or svc != 0xdf01 or ret != 0x4770:
            raise AssertionError(f'{name}: unexpected ARM syscall sequence')
        actual = ((first & 15) << 12) | (((first >> 10) & 1) << 11) | \
                 (((second >> 12) & 7) << 8) | (second & 255)
        if actual != base + index:
            raise AssertionError(f'{name}: ID {actual:#x}, kernel expects {base + index:#x}')
        checked += 1
    if not checked or 'NtUserGetKeyboardLayoutList' not in addresses:
        raise AssertionError('required Win32u exports missing')
    print(f'ARM Win32u syscall stubs: PASS ({checked} exported services)')


if __name__ == '__main__':
    check(Path(sys.argv[1]), sys.argv[2] if len(sys.argv) > 2 else 'llvm-readobj',
          sys.argv[3] if len(sys.argv) > 3 else 'clang')
