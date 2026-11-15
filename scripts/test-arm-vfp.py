#!/usr/bin/env python3
"""Check ARM context restores against Cortex-A9 erratum 754320 (UAN 0007D)."""

import argparse
from pathlib import Path
import re
import struct
import subprocess

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import (UC_ARM_REG_CP_REG, UC_ARM_REG_D0,
                               UC_ARM_REG_FPEXC, UC_ARM_REG_R0, UC_ARM_REG_SP)


def restore_groups(path):
    output = subprocess.check_output(['llvm-objdump', '-d', str(path)], text=True)
    groups = []
    for line in output.splitlines():
        match = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{4}\s+)+)'
                         r'(vldmia|vpop)\s+(.*)', line)
        if not match:
            continue
        address = int(match[1], 16)
        code = b''.join(struct.pack('<H', int(value, 16)) for value in match[2].split())
        registers = [int(value) for value in re.findall(r'\bd(\d+)\b', match[4])]
        if not groups or groups[-1][-1][0] + len(groups[-1][-1][1]) != address:
            groups.append([])
        groups[-1].append((address, code, registers, match[3]))
    return [group for group in groups if [r for item in group for r in item[2]] == list(range(32))]


def check(group):
    for address, code, registers, mnemonic in group:
        assert len(registers) <= 8, f'{mnemonic} at {address:#x} loads {len(registers)} beats; maximum is 8'
    code = b''.join(item[1] for item in group)
    for offset in (0, 8):
        machine = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
        machine.mem_map(0x10000, 4096)
        machine.mem_map(0x20000, 4096)
        machine.mem_write(0x10000, code)
        source = 0x20000 + offset
        values = [0x7ff0000000000001 ^ (index * 0x1020304050607) for index in range(32)]
        machine.mem_write(source, struct.pack('<32Q', *values))
        machine.reg_write(UC_ARM_REG_CP_REG, (15, 0, 0, 1, 0, 0, 2, 0xf << 20))
        machine.reg_write(UC_ARM_REG_FPEXC, 1 << 30)
        machine.reg_write(UC_ARM_REG_SP, source)
        machine.reg_write(UC_ARM_REG_R0, source)
        machine.emu_start(0x10001, 0x10000 + len(code), timeout=1000000, count=32)
        actual = [machine.reg_read(UC_ARM_REG_D0 + index) for index in range(32)]
        assert actual == values, 'context restore changed VFP register contents'
        register = UC_ARM_REG_SP if group[0][3] == 'vpop' else UC_ARM_REG_R0
        expected = source + (256 if register == UC_ARM_REG_SP else 192)
        assert machine.reg_read(register) == expected, 'context restore changed the source pointer'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('output-Clang-arm-debug'))
    args = parser.parse_args()
    root = args.build_dir / 'ntoskrnl/CMakeFiles/ntoskrnl.dir/ke/arm'
    groups = restore_groups(root / 'trap.s.obj') + restore_groups(root / 'ctxswtch.s.obj')
    assert len(groups) == 3, f'expected exception, context-switch and startup restores, found {len(groups)}'
    for group in groups:
        check(group)
    print('ARM VFP context restore: PASS (3 paths, 32 registers, 8-beat loads, aligned sources)')


if __name__ == '__main__':
    main()
