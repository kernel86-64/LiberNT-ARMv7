#!/usr/bin/env python3
"""Execute the ARM exception return with the stack used by a user callback."""

import argparse
import importlib.util
from pathlib import Path
import re
import struct
import sys

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP,
                               UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_CPSR)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('output-Clang-arm-debug'))
    args = parser.parse_args()
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location('arm_image', Path(__file__).with_name('test-arm-gic.py'))
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    machine, symbols = helper.load_image(args.build_dir, 'ntoskrnl/ntoskrnl.exe', 'symbols/ntoskrnl.exe')
    offsets = dict((name, int(value, 16)) for name, value in re.findall(
        r'^(\w+) = (0x[0-9a-f]+)$', (args.build_dir / 'sdk/include/asm/ksarm.h').read_text(), re.M))
    stack, pcr, thread, frames = 0x200f000, 0x2100000, 0x2200000, 0x2300000
    trap, exception = frames + 0x1000, frames
    for base in (0x2000000, pcr, thread, frames):
        machine.mem_map(base, 0x10000)

    def put(address, value):
        machine.mem_write(address, struct.pack('<I', value))

    put(pcr + offsets['PcCurrentThread'], thread)
    put(thread + offsets['ThInitialStack'], stack - 64)
    values = [0x10203000 + index for index in range(13)]
    for index in range(4):
        put(trap + offsets['TrR0'] + 4 * index, values[index])
    for index in range(8):
        put(exception + offsets['ExR4'] + 4 * index, values[index + 4])
    for name, value in [('TrR12', values[12]), ('TrSp', 0x700ff000),
                        ('TrLr', 0x12345001), ('TrPc', 0x12346001),
                        ('TrCpsr', 0x30), ('TrVfpState', 0)]:
        put(trap + offsets[name], value)
    reached = []

    def instruction(instance, pc, size, _):
        code = bytes(instance.mem_read(pc, size))
        if pc == symbols['KiArmDeliverUserApc'] or pc == symbols.get('KiArmMarkExceptionExit'):
            sp = instance.reg_read(UC_ARM_REG_SP)
            instance.mem_write(sp - 160, b'\xcc' * 160)
            instance.reg_write(UC_ARM_REG_R0, 0)
            instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))
        elif size == 4 and struct.unpack('<HH', code) == (0xee1d, 0x0f90):
            instance.reg_write(UC_ARM_REG_R0, pcr)
            instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
        elif code == bytes.fromhex('bde900c0'):
            for index, expected in enumerate(values):
                assert instance.reg_read(UC_ARM_REG_R0 + index) == expected, f'callback return corrupted r{index}'
            assert instance.reg_read(UC_ARM_REG_LR) == 0x12345001
            sp = instance.reg_read(UC_ARM_REG_SP)
            assert sp == stack - 72
            assert struct.unpack('<II', instance.mem_read(sp, 8)) == (0x12346001, 0x30), 'callback return frame was overwritten'
            reached.append(True)
            instance.emu_stop()

    machine.hook_add(UC_HOOK_CODE, instruction)
    machine.reg_write(UC_ARM_REG_CPSR, 0xd3)
    machine.reg_write(UC_ARM_REG_SP, stack)
    machine.reg_write(UC_ARM_REG_R0, trap)
    machine.reg_write(UC_ARM_REG_R1, exception)
    machine.emu_start(symbols['KiExceptionExit'] | 1, 0, timeout=1000000, count=10000)
    assert reached, 'exception return did not reach RFE'
    print('ARM callback exception return: PASS (callee stack use, integer registers, PC/CPSR)')


if __name__ == '__main__':
    main()
