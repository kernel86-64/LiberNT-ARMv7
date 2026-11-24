#!/usr/bin/env python3
"""Execute the SDHCI PIO path with stale and coalesced buffer-ready events."""

import argparse
import importlib.util
from pathlib import Path
import re
import struct
import subprocess

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC


def members(symbols, name):
    listing = subprocess.check_output(['llvm-dwarfdump', '--name=' + name, str(symbols)], text=True)
    address = re.search(r'^(0x[0-9a-f]+): DW_TAG_structure_type', listing, re.M)[1]
    listing = subprocess.check_output(['llvm-dwarfdump', '--debug-info=' + address,
                                     '--show-children', str(symbols)], text=True)
    result = {}
    for item in listing.split('DW_TAG_member')[1:]:
        field = re.search(r'DW_AT_name\s+\("([^"]+)"\)', item)
        location = re.search(r'DW_AT_data_member_location\s+\(DW_OP_plus_uconst (0x[0-9a-f]+)\)', item)
        if field and location:
            result[field[1]] = int(location[1], 16)
    return result


def check(build, load_image, offsets, read, scenario, interrupt, entry='SdBusSendSdhciCommand'):
    machine, symbols = load_image(build, 'drivers/bus/sd/sdbus.sys', 'symbols/sdbus.sys')
    machine.mem_map(0x2000000, 0x10000)
    machine.mem_map(0x2100000, 0x10000)
    machine.mem_map(0x2200000, 0x10000)
    fdo, command, mdl, buffer = 0x2200000, 0x2201000, 0x2202000, 0x2203000
    registers = 0x2208000
    blocks = 2 if scenario == 'coalesced' else 1
    length = blocks * 512
    f, c, m = offsets

    def put(address, value, size=4):
        machine.mem_write(address, value.to_bytes(size, 'little'))

    def get(address, size=4):
        return int.from_bytes(machine.mem_read(address, size), 'little')

    put(fdo + f['RegisterBase'], registers)
    if interrupt:
        put(fdo + f['InterruptObject'], 0x2209000)
    put(command + c['Cmd'], (18 if blocks > 1 else 17) if read else (25 if blocks > 1 else 24))
    put(command + c['CmdClass'], 0)
    put(command + c['TransferDirection'], 1 if read else 2)
    put(command + c['TransferType'], 3 if blocks > 1 else 2)
    put(command + c['ResponseType'], 2)
    put(mdl + m['MdlFlags'], 1, 2)
    put(mdl + m['MappedSystemVa'], buffer)
    put(mdl + m['ByteCount'], length)
    payload = bytes(index & 255 for index in range(length))
    machine.mem_write(buffer, payload if not read else bytes(length))
    ready = 0x20 if read else 0x10
    enable = 0x800 if read else 0x400
    status, issued, polls, transferred, waits = (3 if scenario == 'command_race' else 0), False, 0, 0, 0
    command_ticks = 0
    pending_isr = 0
    written = bytearray()
    hooks = {}
    for index, name in enumerate(sorted(name[6:] for name in symbols if name.startswith('__imp_'))):
        address = 0x2100000 + index * 16
        put(symbols['__imp_' + name], address | 1)
        hooks[address] = name
    hooks[symbols['SdBusResetHost']] = 'reset'
    synchronized = []

    def instruction(instance, pc, size, _):
        nonlocal status, issued, polls, transferred, waits, command_ticks, pending_isr
        if synchronized and pc == synchronized[-1]:
            synchronized.pop()
        if pc not in hooks:
            return
        name = hooks[pc]
        r0, r1 = instance.reg_read(UC_ARM_REG_R0), instance.reg_read(UC_ARM_REG_R1)
        result = 0
        if name.startswith('READ_REGISTER_'):
            offset = r0 - registers
            if offset == 0x30:
                result = status
            elif offset == 0x24:
                if issued:
                    polls += 1
                    if scenario != 'timeout' and (scenario != 'stale' or polls >= 4) and (scenario != 'command_race' or command_ticks >= 4):
                        result = enable
            elif offset == 0x20:
                assert issued and scenario not in ('timeout', 'error') and (scenario != 'stale' or polls >= 4), 'PIO read before Buffer Read Enable'
                result = int.from_bytes(payload[transferred:transferred + 4], 'little')
                transferred += 4
                if transferred == length:
                    status |= 2
            else:
                if offset == 0x10 and scenario == 'command_race':
                    assert command_ticks >= 4, 'response read before the current command completed: stale ISR status survived command reset'
                result = get(r0, 1 if name.endswith('UCHAR') else 2 if name.endswith('USHORT') else 4)
        elif name.startswith('WRITE_REGISTER_'):
            offset = r0 - registers
            if offset == 0x30:
                status &= ~r1
                if issued and scenario == 'error' and r1 & 1:
                    status |= 0x108000
            elif offset in (0xc, 0xe):
                if pending_isr:
                    put(fdo + f['CommandInterruptStatus'], get(fdo + f['CommandInterruptStatus']) | pending_isr)
                    pending_isr = 0
                issued = True
                status = 0 if scenario == 'command_race' else 1 | (ready if scenario == 'coalesced' else 0)
            elif offset == 0x20:
                assert issued and scenario not in ('timeout', 'error') and (scenario != 'stale' or polls >= 4), 'PIO write before Buffer Write Enable'
                written.extend(r1.to_bytes(4, 'little'))
                transferred += 4
                if transferred == length:
                    status |= 2
        elif name == 'KeClearEvent':
            if scenario == 'stale':
                put(fdo + f['CommandInterruptStatus'], ready)
            elif scenario == 'command_race':
                pending_isr = status
                status = 0
        elif name == 'KeWaitForSingleObject':
            waits += 1
            result = 0x102
        elif name == 'KeSynchronizeExecution':
            if pending_isr:
                put(fdo + f['CommandInterruptStatus'], get(fdo + f['CommandInterruptStatus']) | pending_isr)
                pending_isr = 0
            synchronized.append(instance.reg_read(UC_ARM_REG_LR) & ~1)
            instance.reg_write(UC_ARM_REG_R0, instance.reg_read(UC_ARM_REG_R2))
            instance.reg_write(UC_ARM_REG_PC, r1)
            return
        elif name == 'KeStallExecutionProcessor':
            if issued and scenario == 'command_race':
                command_ticks += 1
                if command_ticks == 4:
                    status |= 1
        elif name in ('DbgPrint', 'reset'):
            pass
        elif name in ('memcpy', 'RtlCopyMemory'):
            count = instance.reg_read(UC_ARM_REG_R2)
            instance.mem_write(r0, bytes(instance.mem_read(r1, count)))
            result = r0
        else:
            raise AssertionError('Unexpected SDHCI dependency: ' + name)
        instance.reg_write(UC_ARM_REG_R0, result)
        instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))

    machine.hook_add(UC_HOOK_CODE, instruction)
    stack = 0x200f000
    arguments = (fdo, command, 0, mdl)
    stack_arguments = (length, 512, 0, 0x220a000 if scenario == 'command_race' else 0)
    if entry == 'SdBusSendCommand':
        arguments = (fdo, 13, 0, 0x1a)
        stack_arguments = (0x220a000, 0, 0, 0)
    elif entry == 'SdBusSendDataReadCommand':
        arguments = (fdo, 51, 0, 0x1a)
        stack_arguments = (buffer, length, 0x220a000, 0)
    machine.mem_write(stack, struct.pack('<4I', *stack_arguments))
    machine.reg_write(UC_ARM_REG_SP, stack)
    machine.reg_write(UC_ARM_REG_LR, 0x210f001)
    for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3),
                          arguments):
        machine.reg_write(reg, value)
    machine.emu_start(symbols[entry] | 1, 0x210f000, timeout=5000000, count=1000000)
    result = machine.reg_read(UC_ARM_REG_R0)
    if scenario in ('timeout', 'error'):
        assert result & 0x80000000 and transferred == 0, f'{scenario}: status={result:#x}, bytes={transferred}'
    elif entry == 'SdBusSendCommand':
        assert result == 0 and transferred == 0, f'{entry}: status={result:#x}'
    else:
        assert result == 0 and transferred == length, f'{scenario}: status={result:#x}, bytes={transferred}, waits={waits}'
        actual = bytes(machine.mem_read(buffer, length)) if read else bytes(written)
        assert actual == payload, 'PIO data contents differ'


def check_signals(build, load_image, fields):
    for backend in ('exynos', 'generic', 'no_extension'):
        for offset in (0x34, 0x38):
            for value in (0, 0x30, 0x07ff00fb, 0xffffffff):
                machine, symbols = load_image(build, 'drivers/bus/sd/sdbus.sys', 'symbols/sdbus.sys')
                machine.mem_map(0x2000000, 0x10000)
                machine.mem_map(0x2100000, 0x10000)
                machine.mem_map(0x2200000, 0x10000)
                fdo, registers = 0x2200000, 0x2208000
                machine.mem_write(fdo + fields['RegisterBase'], struct.pack('<I', registers))
                if backend != 'no_extension':
                    machine.mem_write(fdo + fields['HardwareExtension'], struct.pack('<I', 0x2201000))
                    machine.mem_write(0x2201000, struct.pack('<I', symbols['SdBusExynosOps'] if backend == 'exynos' else 0x2202000))
                machine.mem_write(symbols['__imp_WRITE_REGISTER_ULONG'], struct.pack('<I', 0x2100001))
                writes = []

                def instruction(instance, pc, size, _):
                    if pc == 0x2100000:
                        writes.append((instance.reg_read(UC_ARM_REG_R0), instance.reg_read(UC_ARM_REG_R1)))
                        instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))

                machine.hook_add(UC_HOOK_CODE, instruction)
                machine.reg_write(UC_ARM_REG_SP, 0x200f000)
                machine.reg_write(UC_ARM_REG_LR, 0x2101001)
                for register, argument in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2), (fdo, offset, value)):
                    machine.reg_write(register, argument)
                machine.emu_start(symbols['SdBusWriteReg32'] | 1, 0x2101000, timeout=1000000, count=10000)
                expected = value & 1 if backend == 'exynos' and offset == 0x38 else value
                assert writes == [(registers + offset, expected)], f'{backend}: offset={offset:#x}, value={value:#x}, writes={writes}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('output-Clang-arm-debug'))
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('gic_test', Path(__file__).with_name('test-arm-gic.py'))
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    symbols = args.build_dir / 'symbols/sdbus.sys'
    offsets = [members(symbols, name) for name in ('_FDO_EXTENSION', '_SDCMD_DESCRIPTOR', '_MDL')]
    for scenario in ('stale', 'coalesced', 'timeout', 'error'):
        for read in (True, False):
            for interrupt in (False, True):
                check(args.build_dir, helper.load_image, offsets, read, scenario, interrupt)
    for read in (True, False):
        check(args.build_dir, helper.load_image, offsets, read, 'command_race', True)
    for entry in ('SdBusSendCommand', 'SdBusSendDataReadCommand'):
        check(args.build_dir, helper.load_image, offsets, True, 'command_race', True, entry)
    check_signals(args.build_dir, helper.load_image, offsets[0])
    print('ARM SDHCI PIO: PASS (polled/interrupt, read/write, stale/coalesced events, command-reset race, timeout/error, Exynos signal policy)')


if __name__ == '__main__':
    main()
