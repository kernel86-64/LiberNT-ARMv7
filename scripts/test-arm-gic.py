#!/usr/bin/env python3
"""Execute built ARM HAL/kernel code to check SGIs, IPI masks and SMP locking."""

import argparse
from pathlib import Path
import struct
import subprocess
import re

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_CPSR


def load_image(build, path, symbol_file):
    data = (build / path).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    _, count, _, _, _, optional, _ = struct.unpack_from('<HHIIIHH', data, pe + 4)
    header = pe + 24
    base = struct.unpack_from('<I', data, header + 28)[0]
    length = struct.unpack_from('<I', data, header + 56)[0]
    machine = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    machine.mem_map(base, (length + 4095) & ~4095)
    for index in range(count):
        offset = header + optional + index * 40
        _, rva, size, position = struct.unpack_from('<IIII', data, offset + 8)
        if size:
            machine.mem_write(base + rva, data[position:position + size])
    symbols = {}
    output = subprocess.check_output(['llvm-nm', str(build / symbol_file)], text=True)
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    return machine, symbols


def check(build, exynos, cpu):
    machine, symbols = load_image(build, 'hal/halarm/hal.dll', 'symbols/hal.dll')
    machine.mem_map(0x200000, 0x10000)
    machine.mem_map(0x300000, 4096)
    machine.mem_map(0x400000, 0x8000)
    machine.mem_map(0xfff10000, 0x2000)
    machine.mem_map(0xfff20000, 0x2000)
    machine.mem_write(symbols['HalpExynos'], bytes([exynos]))
    offsets = (build / 'sdk/include/asm/ksarm.h').read_text()
    prcb = int(re.search(r'^PcPrcb = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)
    machine.mem_write(0x400000 + prcb, bytes([cpu]))
    pointer = int(re.search(r'^PcCurrentPrcb = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)
    machine.mem_write(0x400000 + pointer, struct.pack('<I', 0x400000 + prcb))
    current = int(re.search(r'^PcCurrentThread = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)
    machine.mem_write(0x400000 + current, struct.pack('<I', 0x500000))
    writes = []
    secondary_init = False
    actlr = []
    diagnostic = 0x10000
    migrate = False
    migrate_after_pcr_read = False
    foreign_irql = 5
    migrated = False

    def instruction(instance, pc, size, _):
        nonlocal migrated, diagnostic
        if migrate and instance.mem_read(pc, size) == bytes.fromhex('72b6'):
            migrated = True
        if secondary_init and pc == symbols['HalpExynosInitializeCacheRanges']:
            raise AssertionError('AP reinitialized global HAL cache and timer state')
        if instance.mem_read(pc, size) == bytes.fromhex('10bf'):
            instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
        if size == 4:
            first, second = struct.unpack('<HH', instance.mem_read(pc, size))
            if first == 0xee1d and second & 0xfff == 0xf90:
                instance.reg_write(UC_ARM_REG_R0 + (second >> 12), 0x404000 if migrated else 0x400000)
                if migrate_after_pcr_read and not migrated and not instance.reg_read(UC_ARM_REG_CPSR) & 0x80:
                    instance.mem_write(0x400000 + 0x1c, bytes([foreign_irql]))
                    migrated = True
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
            elif first == 0xee10 and second & 0xfff == 0xf10:
                instance.reg_write(UC_ARM_REG_R0 + (second >> 12), 0x412fc091 if exynos else 0x412fc0f0)
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
            elif first == 0xee1f and second & 0xfff == 0xf30:
                instance.reg_write(UC_ARM_REG_R0 + (second >> 12), diagnostic)
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
            elif first == 0xee0f and second & 0xfff == 0xf30:
                diagnostic = instance.reg_read(UC_ARM_REG_R0 + (second >> 12))
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
            elif first == 0xee11 and second & 0xfff == 0xf30:
                instance.reg_write(UC_ARM_REG_R0 + (second >> 12), 0x40)
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)
            elif first == 0xee01 and second & 0xfff == 0xf30:
                actlr.append(instance.reg_read(UC_ARM_REG_R0 + (second >> 12)))
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

    def write(instance, access, address, size, value, _):
        if address >= 0xfff00000:
            if address == 0xfff20010 + (cpu * 4096 if exynos else 0):
                assert instance.reg_read(UC_ARM_REG_CPSR) & 0x80, 'EOI token popped with IRQs enabled'
            writes.append((address, value))

    machine.hook_add(UC_HOOK_CODE, instruction)
    machine.hook_add(UC_HOOK_MEM_WRITE, write)

    def call(name, first, second=0):
        writes.clear()
        machine.reg_write(UC_ARM_REG_SP, 0x210000)
        machine.reg_write(UC_ARM_REG_LR, 0x300001)
        machine.reg_write(UC_ARM_REG_R0, first)
        machine.reg_write(UC_ARM_REG_R1, second)
        machine.emu_start(symbols[name] | 1, 0x300000, timeout=1000000, count=100000)

    dist = 0xfff10000 + (cpu * 4096 if exynos else 0)
    interface = 0xfff20000 + (cpu * 4096 if exynos else 0)
    call('HalpClockVector', 0)
    call('HalpArmGetInterruptIrql', machine.reg_read(UC_ARM_REG_R0))
    clock_irql = machine.reg_read(UC_ARM_REG_R0)
    call('HalpArmGetInterruptIrql', 3)
    assert machine.reg_read(UC_ARM_REG_R0) == clock_irql, 'Clock SGI uses a different IRQL than the hardware clock'
    call('HalpArmInitializeProcessor', cpu)
    assert diagnostic == (0x410810 if exynos else 0x10000)
    priorities = ((15 - clock_irql) << 28) | 0x0010e0d0
    assert (dist + 0x400, priorities) in writes, f'CPU{cpu} SGI priorities written to {writes}'
    assert (dist + 0x100, 15) in writes
    assert (interface + 4, 255) in writes
    call('HalpArmSendSgi', 3, 2)
    assert writes == [(dist + 0xf00, 0x30002)], f'CPU{cpu} SGI sender bank: {writes}'
    call('HalpArmEnableSmp', 0)
    assert actlr == [0x41 if exynos else 0x40], f'CPU{cpu} cache/TLB broadcast state: {actlr}'
    for version in [1, 2]:
        machine.mem_write(dist + 0xfe8, struct.pack('<I', version << 4))
        for irql, sgi in [(1, 1), (2, 0), (clock_irql, 3)]:
            call('HalClearSoftwareInterrupt', irql)
            expected = [] if version == 1 else [(dist + 0xf10, 255 << (sgi * 8))]
            assert writes == expected, f'CPU{cpu} GICv{version} SGI{sgi} clear: {writes}, expected {expected}'

    for token in [89, 0x402, 1023]:
        machine.mem_write(interface + 0xc, struct.pack('<I', token))
        call('HalGetInterruptSource', 0)
        assert machine.reg_read(UC_ARM_REG_R0) == token & 0x3ff
    for token in [0x402, 89]:
        machine.reg_write(UC_ARM_REG_CPSR, machine.reg_read(UC_ARM_REG_CPSR) & ~0x80)
        call('HalEndSystemInterrupt', 0)
        assert not machine.reg_read(UC_ARM_REG_CPSR) & 0x80, 'EOI changed the caller IRQ mask'
        actual = [value for address, value in writes if address == interface + 0x10]
        assert actual == [token], f'CPU{cpu} nested IRQ EOI: {actual}, expected {[token]}'
    machine.mem_write(interface + 0xc, struct.pack('<I', 1023))
    call('HalGetInterruptSource', 0)
    call('HalEndSystemInterrupt', 0)
    assert not [value for address, value in writes if address == interface + 0x10]

    lock = 0x400800
    reads = []

    def read(instance, access, address, size, value, _):
        if address == lock:
            reads.append(address)
            if len(reads) == 100:
                instance.mem_write(lock, bytes(4))

    machine.hook_add(UC_HOOK_MEM_READ, read)
    machine.mem_write(lock, struct.pack('<I', 0x600001))
    call('KfAcquireSpinLock', lock)
    assert len(reads) >= 100, f'CPU{cpu} acquired a spinlock held by another thread without waiting'
    assert struct.unpack('<I', machine.mem_read(lock, 4))[0] == 0x500001
    call('KfReleaseSpinLock', lock, 0)
    assert machine.mem_read(lock, 4) == bytes(4)
    synch = int(re.findall(r'^SYNCH_LEVEL = (0x[0-9a-f]+)$', offsets, re.M)[-1], 16)
    call('KeRaiseIrqlToSynchLevel', 0)
    assert machine.reg_read(UC_ARM_REG_R0) == 0
    call('KeGetCurrentIrql', 0)
    assert machine.reg_read(UC_ARM_REG_R0) == synch, 'C synchronization IRQL disagrees with ARM assembler'
    assert struct.unpack('<I', machine.mem_read(interface + 4, 4))[0] == (15 - synch) << 4
    call('KfLowerIrql', 0)
    call('HalpAcquireSpinLockRaiseToSynch', lock)
    assert machine.reg_read(UC_ARM_REG_R0) == 0
    call('KeGetCurrentIrql', 0)
    assert machine.reg_read(UC_ARM_REG_R0) == synch
    call('KfReleaseSpinLock', lock, 0)
    call('KeAcquireInStackQueuedSpinLockRaiseToSynch', lock, 0x208000)
    call('KeGetCurrentIrql', 0)
    assert machine.reg_read(UC_ARM_REG_R0) == synch
    call('HalpReleaseInStackQueuedSpinLock', 0x208000)
    call('KeGetCurrentIrql', 0)
    assert machine.reg_read(UC_ARM_REG_R0) == 0
    assert machine.mem_read(lock, 4) == bytes(4)
    if exynos and cpu == 1:
        secondary_init = True
        call('HalInitSystem', 1)
        assert machine.reg_read(UC_ARM_REG_R0) == 1

    secondary_init = False
    machine.mem_write(0x404000 + prcb, bytes([1 - cpu]))
    machine.mem_write(0x404000 + pointer, struct.pack('<I', 0x404000 + prcb))
    machine.mem_write(0x400000 + 0x1c, bytes(1))
    machine.mem_write(0x404000 + 0x1c, bytes(1))
    migrate = True
    call('KfRaiseIrql', 2)
    expected_interface = 0xfff20000 + ((1 - cpu) * 4096 if exynos else 0)
    assert migrated, 'migration injection did not reach CPSID'
    assert writes == [(expected_interface + 4, 0xd0)], f'IRQL write after CPU migration: {writes}'

    migrate = False
    migrated = False
    migrate_after_pcr_read = True
    machine.mem_write(0x400000 + 0x1c, bytes(1))
    machine.mem_write(0x404000 + 0x1c, bytes(1))
    call('KfRaiseIrql', 2)
    old_irql = machine.reg_read(UC_ARM_REG_R0)
    assert old_irql == 0, f'IRQL raise read the previous CPU after migration: {old_irql}'
    assert writes == [(interface + 4, 0xd0)], f'IRQL raise did not program the local priority mask: {writes}'
    assert not machine.reg_read(UC_ARM_REG_CPSR) & 0x80, 'IRQL raise left IRQs masked'

    foreign_irql = 0
    machine.mem_write(0x400000 + 0x1c, bytes([1]))
    machine.mem_write(0x404000 + 0x1c, bytes([1]))
    call('KfLowerIrql', 1)
    assert writes == [(interface + 4, 0xe0)], f'IRQL lower did not program the local priority mask: {writes}'
    assert not machine.reg_read(UC_ARM_REG_CPSR) & 0x80, 'IRQL lower left IRQs masked'
    machine.reg_write(UC_ARM_REG_CPSR, machine.reg_read(UC_ARM_REG_CPSR) | 0x80)
    call('KfLowerIrql', 0)
    assert machine.reg_read(UC_ARM_REG_CPSR) & 0x80, 'IRQL lower unmasked an IRQ-disabled caller'


def check_ipi(build):
    machine, symbols = load_image(build, 'ntoskrnl/ntoskrnl.exe', 'symbols/ntoskrnl.exe')
    machine.mem_map(0x2000000, 0x10000)
    machine.mem_map(0x2100000, 4096)
    machine.mem_map(0x2200000, 0x8000)
    machine.mem_write(symbols['KeNumberProcessors'], bytes([8]))
    machine.mem_write(symbols['__imp_HalRequestIpi'], struct.pack('<I', 0x2100001))
    offsets = (build / 'sdk/include/asm/ksarm.h').read_text()
    frozen = int(re.search(r'^PbIpiFrozen = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)
    requested = []

    def instruction(instance, pc, size, _):
        if pc == 0x2100000:
            requested.append(instance.reg_read(UC_ARM_REG_R0))
            instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))

    machine.hook_add(UC_HOOK_CODE, instruction)
    for cpu in range(8):
        machine.mem_write(symbols['KiProcessorBlock'] + cpu * 4, struct.pack('<I', 0x2200000 + cpu * 4096))
    for mask in [0, 1, 2, 3, 5, 0x80, 0xa5, 0xff]:
        requested.clear()
        for cpu in range(8):
            machine.mem_write(0x2200000 + cpu * 4096 + frozen, bytes(4))
        machine.reg_write(UC_ARM_REG_SP, 0x2010000)
        machine.reg_write(UC_ARM_REG_LR, 0x2100101)
        machine.reg_write(UC_ARM_REG_R0, mask)
        machine.reg_write(UC_ARM_REG_R1, 1)
        machine.emu_start(symbols['KiIpiSend'] | 1, 0x2100100, timeout=1000000, count=100000)
        actual = [struct.unpack('<I', machine.mem_read(0x2200000 + cpu * 4096 + frozen, 4))[0]
                  for cpu in range(8)]
        expected = [2 if mask & (1 << cpu) else 0 for cpu in range(8)]
        assert actual == expected, f'IPI mask {mask:#x}: {actual}, expected {expected}'
        assert requested == [mask]


def check_irq(build):
    machine, symbols = load_image(build, 'ntoskrnl/ntoskrnl.exe', 'symbols/ntoskrnl.exe')
    for address, size in [(0x2000000, 0x10000), (0x2100000, 4096),
                          (0x2200000, 0x4000), (0x2300000, 4096), (0x2400000, 4096)]:
        machine.mem_map(address, size)
    offsets = (build / 'sdk/include/asm/ksarm.h').read_text()

    def offset(name):
        return int(re.search(r'^' + name + r' = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)

    machine.mem_write(0x2200000 + offset('PcCurrentPrcb'), struct.pack('<I', 0x2200000 + offset('PcPrcb')))
    machine.mem_write(0x2200000 + offset('PcCurrentThread'), struct.pack('<I', 0x2300000))
    machine.mem_write(0x2300000 + offset('ThTrapFrame'), struct.pack('<I', 0x2400800))
    calls = []
    hooks = {}
    for index, (name, result) in enumerate([
            ('HalGetInterruptSource', 102), ('KeGetCurrentIrql', 0),
            ('KfRaiseIrql', 0), ('HalpArmDispatchInterrupt', 0),
            ('HalEndSystemInterrupt', 0), ('KfLowerIrql', 0)]):
        address = 0x2100000 + index * 16
        machine.mem_write(symbols['__imp_' + name], struct.pack('<I', address | 1))
        hooks[address] = (name, result)
    for name, result in [('KiArmGetInterruptIrql', 5), ('KeDispatchSecondaryInterrupt', 0),
                         ('DbgPrintEarly', 0)]:
        hooks[symbols[name]] = (name, result)

    def instruction(instance, pc, size, _):
        if pc == symbols['KeBugCheckEx']:
            raise AssertionError('An ISR returning FALSE caused a kernel bugcheck')
        if pc in hooks:
            name, result = hooks[pc]
            calls.append(name)
            instance.reg_write(UC_ARM_REG_R0, result)
            instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))
        elif size == 4:
            first, second = struct.unpack('<HH', instance.mem_read(pc, size))
            if first == 0xee1d and second & 0xfff == 0xf90:
                instance.reg_write(UC_ARM_REG_R0 + (second >> 12), 0x2200000)
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

    machine.hook_add(UC_HOOK_CODE, instruction)
    machine.reg_write(UC_ARM_REG_SP, 0x2010000)
    machine.reg_write(UC_ARM_REG_LR, 0x2100101)
    machine.reg_write(UC_ARM_REG_R0, 0x2400000)
    machine.reg_write(UC_ARM_REG_R1, 0)
    machine.emu_start(symbols['KiInterruptHandler'] | 1, 0x2100100, timeout=1000000, count=100000)
    assert 'KeDispatchSecondaryInterrupt' in calls and 'HalEndSystemInterrupt' in calls
    assert calls[-1] == 'KfLowerIrql'
    assert struct.unpack('<I', machine.mem_read(0x2300000 + offset('ThTrapFrame'), 4))[0] == 0x2400800


def check_dispatch(build):
    for replacement in [0, 0x2300000]:
        machine, symbols = load_image(build, 'ntoskrnl/ntoskrnl.exe', 'symbols/ntoskrnl.exe')
        for address, size in [(0x2000000, 0x10000), (0x2100000, 4096),
                              (0x2200000, 0x4000), (0x2300000, 0x2000)]:
            machine.mem_map(address, size)
        offsets = (build / 'sdk/include/asm/ksarm.h').read_text()

        def offset(name):
            return int(re.search(r'^' + name + r' = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)

        machine.mem_write(0x2200000 + offset('PcCurrentPrcb'), struct.pack('<I', 0x2200000 + offset('PcPrcb')))
        machine.mem_write(0x2200000 + offset('PcCurrentThread'), struct.pack('<I', 0x2300000))
        machine.mem_write(0x2200000 + offset('PcNextThread'), struct.pack('<I', 0x2301000))
        machine.mem_write(0x220001c, bytes([2]))
        machine.mem_write(0x2300000 + offset('ThState'), bytes([offset('Standby') if replacement else offset('Running')]))
        machine.mem_write(symbols['__imp_KeGetCurrentIrql'], struct.pack('<I', 0x2100001))
        changed = []

        def instruction(instance, pc, size, _):
            if pc == 0x2100000:
                instance.reg_write(UC_ARM_REG_R0, 2)
                instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))
            elif pc == symbols['KiSwapContext']:
                raise AssertionError('Dispatcher switched after NextThread was withdrawn or selected itself')
            elif size == 4:
                first, second = struct.unpack('<HH', instance.mem_read(pc, size))
                if first == 0xee1d and second & 0xfff == 0xf90:
                    instance.reg_write(UC_ARM_REG_R0 + (second >> 12), 0x2200000)
                    instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

        def write(instance, access, address, size, value, _):
            if address == 0x2200000 + offset('PcPrcbLock') and value == 1 and not changed:
                instance.mem_write(0x2200000 + offset('PcNextThread'), struct.pack('<I', replacement))
                changed.append(True)

        machine.hook_add(UC_HOOK_CODE, instruction)
        machine.hook_add(UC_HOOK_MEM_WRITE, write)
        machine.reg_write(UC_ARM_REG_SP, 0x2010000)
        machine.reg_write(UC_ARM_REG_LR, 0x2100101)
        machine.emu_start(symbols['KiDispatchInterrupt'] | 1, 0x2100100, timeout=1000000, count=100000)
        assert changed
        for field, expected in [('PcCurrentThread', 0x2300000), ('PcNextThread', 0), ('PcPrcbLock', 0)]:
            actual = struct.unpack('<I', machine.mem_read(0x2200000 + offset(field), 4))[0]
            assert actual == expected, f'{field}={actual:#x}, expected {expected:#x}'
        assert machine.mem_read(0x2300000 + offset('ThState'), 1) == bytes([offset('Running')])


def check_errata(build):
    cases = [(0x410fc090, 0x400800), (0x411fc090, 0x400810),
             (0x411fc093, 0x400810), (0x411fc094, 0x400800),
             (0x412fc090, 0x400810), (0x412fc091, 0x400810),
             (0x412fc092, 0x400810), (0x412fc093, 0x400800),
             (0x412fc095, 0x400800), (0x413fc090, 0x400000),
             (0x414fc090, 0x400000), (0x412fc0f1, 0), (0x512fc091, 0)]
    for midr, mask in cases:
        modes = [(0, True), (mask, True), (mask, False)]
        if midr == 0x412fc091:
            modes.append((0, False))
        for preset, writable in modes:
            machine, symbols = load_image(build, 'hal/halarm/hal.dll', 'symbols/hal.dll')
            machine.mem_map(0x200000, 0x10000)
            machine.mem_map(0x300000, 4096)
            machine.mem_map(0xfff10000, 4096)
            machine.mem_map(0xfff20000, 4096)
            diagnostic = 0x10000 | preset
            changes = []
            failures = []
            machine.mem_write(symbols['__imp_KeBugCheckEx'], struct.pack('<I', 0x300101))

            def instruction(instance, pc, size, _):
                nonlocal diagnostic
                if pc == 0x300100:
                    failures.append(tuple(instance.reg_read(UC_ARM_REG_R0 + index) for index in [1, 2, 3]))
                    instance.emu_stop()
                    return
                if size != 4:
                    return
                first, second = struct.unpack('<HH', instance.mem_read(pc, size))
                register = UC_ARM_REG_R0 + (second >> 12)
                if first == 0xee10 and second & 0xfff == 0xf10:
                    instance.reg_write(register, midr)
                elif first == 0xee1f and second & 0xfff == 0xf30:
                    instance.reg_write(register, diagnostic)
                elif first == 0xee0f and second & 0xfff == 0xf30:
                    value = instance.reg_read(register)
                    if writable:
                        diagnostic = value
                    changes.append(value)
                else:
                    return
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

            def write(instance, access, address, size, value, _):
                if address == 0xfff20000 and value == 1:
                    assert diagnostic & mask == mask, 'IRQs enabled without required errata controls'

            machine.hook_add(UC_HOOK_CODE, instruction)
            machine.hook_add(UC_HOOK_MEM_WRITE, write)
            machine.reg_write(UC_ARM_REG_SP, 0x210000)
            machine.reg_write(UC_ARM_REG_LR, 0x300001)
            machine.reg_write(UC_ARM_REG_R0, 0)
            machine.emu_start(symbols['HalpArmInitializeProcessor'] | 1, 0x300000,
                              timeout=1000000, count=100000)
            if not writable and diagnostic & mask != mask:
                assert failures == [(midr, mask, 0x10000)], f'Unwritable diagnostic control was not reported: {failures}'
                continue
            assert not failures
            assert diagnostic == 0x10000 | mask, f'MIDR {midr:#x}: diagnostic {diagnostic:#x}, expected {0x10000 | mask:#x}'
            expected = [0x10000 | mask] if mask and not preset else []
            assert changes == expected, f'MIDR {midr:#x}: unnecessary diagnostic writes {changes}'


def check_ap_process_mask(build):
    machine, symbols = load_image(build, 'ntoskrnl/ntoskrnl.exe', 'symbols/ntoskrnl.exe')
    for address in (0x2000000, 0x2100000, 0x2200000, 0x2300000, 0x2400000):
        machine.mem_map(address, 0x10000)
    offsets = (build / 'sdk/include/asm/ksarm.h').read_text()
    active = int(re.search(r'^PrActiveProcessors = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)
    prcb = int(re.search(r'^PcPrcb = (0x[0-9a-f]+)$', offsets, re.M)[1], 16)
    machine.mem_write(0x2400000 + active, struct.pack('<I', 1))
    machine.mem_write(symbols['__imp_KfLowerIrql'], struct.pack('<I', 0x2100001))
    skips = {symbols[name] for name in ('KiInitSpinLocks', 'PoInitializePrcb', 'KeInitializeThread')}
    reached = []

    def instruction(instance, pc, size, _):
        if pc == symbols['ExpInitializeExecutive']:
            reached.append(pc)
            instance.emu_stop()
        elif pc in skips or pc == 0x2100000:
            instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))
        elif size == 4:
            first, second = struct.unpack('<HH', instance.mem_read(pc, size))
            if first == 0xee1d and second & 0xfff == 0xf90:
                instance.reg_write(UC_ARM_REG_R0 + (second >> 12), 0x2200000)
                instance.reg_write(UC_ARM_REG_PC, (pc + size) | 1)

    machine.hook_add(UC_HOOK_CODE, instruction)
    machine.reg_write(UC_ARM_REG_SP, 0x200f000)
    machine.mem_write(0x200f000, struct.pack('<II', 1, 0))
    machine.reg_write(UC_ARM_REG_LR, 0x2100101)
    for register, value in enumerate((0x2400000, 0x2300000, 0x230f000, 0x2200000 + prcb)):
        machine.reg_write(UC_ARM_REG_R0 + register, value)
    machine.emu_start(symbols['KiInitializeKernel'] | 1, 0x2100100, timeout=1000000, count=100000)
    assert reached, 'AP initialization did not reach the executive boundary'
    result = struct.unpack('<I', machine.mem_read(0x2400000 + active, 4))[0]
    assert result == 3, f'AP replaced the BSP active-process bit: {result:#x}'


def check_ap_boot_alignment(build):
    for displacement in (4, 12, 0x88, 0xffc):
        machine, symbols = load_image(build, 'ntoskrnl/ntoskrnl.exe', 'symbols/ntoskrnl.exe')
        for address in (0x2000000, 0x2100000, 0x2200000):
            machine.mem_map(address, 0x10000)
        machine.mem_write(symbols['KeMaximumProcessors'], struct.pack('<I', 2))
        for name in ('KeNumprocSpecified', 'KeBootprocSpecified'):
            machine.mem_write(symbols[name], struct.pack('<I', 0))
        allocations = []
        reached = []

        def instruction(instance, pc, size, _):
            if pc == symbols['KiInitializePcr']:
                pcr = instance.reg_read(UC_ARM_REG_R1)
                thread = instance.reg_read(UC_ARM_REG_R0 + 2)
                assert pcr % 4096 == 0, f'AP PCR is not page aligned: {pcr:#x}'
                assert thread % 16 == 0, f'AP thread is not 16-byte aligned: {thread:#x}'
                assert 0x2200000 + displacement <= pcr < thread < 0x2200000 + displacement + allocations[0]
                reached.append(True)
                instance.emu_stop()
                return
            if pc == symbols['KiArmPrepareApBoot']:
                result = 1
            elif pc == symbols['ExAllocatePoolWithTag']:
                allocations.append(instance.reg_read(UC_ARM_REG_R1))
                result = 0x2200000 + displacement if len(allocations) == 1 else 0x2208000
            elif pc == symbols['memset']:
                result = instance.reg_read(UC_ARM_REG_R0)
            elif pc == symbols['MmCreateKernelStack']:
                result = 0x220f000
            else:
                return
            instance.reg_write(UC_ARM_REG_R0, result)
            instance.reg_write(UC_ARM_REG_PC, instance.reg_read(UC_ARM_REG_LR))

        machine.hook_add(UC_HOOK_CODE, instruction)
        machine.reg_write(UC_ARM_REG_SP, 0x200f000)
        machine.reg_write(UC_ARM_REG_LR, 0x2100001)
        machine.emu_start(symbols['KeStartAllProcessors'] | 1, 0x2100000, timeout=1000000, count=100000)
        assert reached, 'AP startup did not initialize the PCR'



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('output-Clang-arm-debug'))
    args = parser.parse_args()
    check_errata(args.build_dir)
    for exynos, cpu in [(False, 0), (True, 0), (True, 1)]:
        check(args.build_dir, exynos, cpu)
    check_ipi(args.build_dir)
    check_irq(args.build_dir)
    check_dispatch(args.build_dir)
    check_ap_process_mask(args.build_dir)
    check_ap_boot_alignment(args.build_dir)
    print('ARM SMP primitives: PASS (A9 errata revisions/readback, synch IRQL, SGI banks/clear, IPI masks, locks, AP init/alignment, unclaimed IRQ, dispatch races, IRQL migration, nested IRQ EOI)')


if __name__ == '__main__':
    main()
