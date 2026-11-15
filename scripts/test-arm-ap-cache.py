#!/usr/bin/env python3
"""Check the built AP trampoline's DCISW operands against the Arm set/way format."""

import argparse
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R7, UC_ARM_REG_PC


def text_section(path):
    data = path.read_bytes()
    _, count, _, _, _, optional, _ = struct.unpack_from('<HHIIIHH', data)
    for index in range(count):
        offset = 20 + optional + index * 40
        if data[offset:offset + 8].rstrip(b'\0') == b'.text':
            size, position = struct.unpack_from('<II', data, offset + 16)
            return data[position:position + size]
    raise RuntimeError('AP trampoline has no .text section')


def check(code, ways, sets, line):
    start = code.index(bytes.fromhex('30ee100f')) + 4
    stop = code.index(bytes.fromhex('bff34f8f'), start)
    address = 0x10000
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    cpu.mem_map(address, 0x1000)
    cpu.mem_write(address, code)
    cpu.reg_write(UC_ARM_REG_R0,
                  ((sets - 1) << 13) | ((ways - 1) << 3) | (line.bit_length() - 5))
    operands = []

    def instruction(machine, pc, size, _):
        if machine.mem_read(pc, size) == bytes.fromhex('07ee567f'):
            operands.append(machine.reg_read(UC_ARM_REG_R7))
            machine.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

    cpu.hook_add(UC_HOOK_CODE, instruction)
    cpu.emu_start((address + start) | 1, address + stop, timeout=1000000, count=1000000)
    shift = 32 - (ways - 1).bit_length()
    expected = {(way << shift) | (entry * line)
                for way in range(ways) for entry in range(sets)}
    if len(operands) != len(expected) or set(operands) != expected:
        raise AssertionError(f'ways={ways} sets={sets} line={line}: '
                             f'{len(set(operands) & expected)}/{len(expected)} valid selectors; '
                             f'unexpected={sorted(set(operands) - expected)[:4]}')


def check_control(code):
    machine = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    machine.mem_map(0x10000, 4096)
    machine.mem_map(0x20000, 8192)
    machine.mem_write(0x10000, code)
    machine.mem_write(0x20010, struct.pack('<I', 0x21000))
    machine.mem_write(0x21028, struct.pack('<I', 0x41))
    machine.reg_write(UC_ARM_REG_R0, 0x20000)
    controls = []

    def instruction(instance, pc, size, _):
        if size != 4:
            return
        first, second = struct.unpack('<HH', instance.mem_read(pc, size))
        if first & 0xff00 == 0xee00 and second & 0xf00 == 0xf00:
            register = UC_ARM_REG_R0 + (second >> 12)
            if first == 0xee11 and second & 0xfff == 0xf30:
                instance.reg_write(register, 0x40)
            elif first == 0xee01 and second & 0xfff == 0xf30:
                controls.append(instance.reg_read(register))
                instance.emu_stop()
            instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

    machine.hook_add(UC_HOOK_CODE, instruction)
    machine.emu_start((0x10000 + code.index(bytes.fromhex('0469'))) | 1,
                      0x10000 + len(code), timeout=1000000, count=10000)
    assert controls == [0x41], f'AP lost the BSP cache/TLB broadcast setting: {controls}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('output-Clang-arm-debug'))
    args = parser.parse_args()
    code = text_section(args.build_dir / 'ntoskrnl/CMakeFiles/ntoskrnl.dir/ke/arm/apboot.S.obj')
    for geometry in [(4, 128, 32), (4, 256, 32), (4, 512, 32), (1, 128, 32), (16, 256, 64)]:
        check(code, *geometry)
    check_control(code)
    print('ARM AP cache setup: PASS (5 cache geometries, inherited ACTLR)')


if __name__ == '__main__':
    main()
