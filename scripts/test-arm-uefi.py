#!/usr/bin/env python3
"""
PROJECT:     LiberNT boot tests
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Build and test ARMv7 EFI applications under EDK II on QEMU virt
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import struct
import time

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def guest_command(path, command):
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(3)
        connection.connect(str(path))
        stream = connection.makefile('rwb')
        json.loads(stream.readline())
        for command in ['qmp_capabilities', command]:
            stream.write((json.dumps({'execute': command}) + '\n').encode())
            stream.flush()
            while True:
                response = json.loads(stream.readline())
                if 'return' in response:
                    break
                if 'error' in response:
                    raise RuntimeError(response['error'])


def stop_guest(path):
    guest_command(path, 'stop')


def clock_snapshot(qmp):
    script = (
        'python import gdb,json; m=gdb.selected_inferior(); '
        'r=lambda a:int.from_bytes(m.read_memory(a,8).tobytes(),"little"); '
        'print("ARM_CLOCK="+json.dumps({"interrupt_time":r(0xffff9008),'
        '"ticks":r(0xffff9320)}))')
    try:
        result = subprocess.run(['gdb', '-q', '-batch', '-ex',
            'target remote 127.0.0.1:1234', '-ex', script, '-ex', 'detach'],
            check=True, capture_output=True, text=True, timeout=3)
    finally:
        stop_guest(qmp)
    for line in result.stdout.splitlines():
        if line.startswith('ARM_CLOCK='):
            return json.loads(line.split('=', 1)[1])
    raise RuntimeError('could not read the ARM shared clock counters')


def boot(build, firmware, stage, lock, modern=False):
    image = build / 'arm-uefi-tests.img'
    variables = build / 'QEMU_VARS.fd'
    log = build / 'arm-boot.log'
    qmp = build / 'arm-qemu.qmp'
    application = 'arm_uefi_test' if stage == 'abi' else 'uefildr'
    binary = build / 'boot/freeldr/freeldr' / (application + '.efi')
    shutil.copyfile(firmware.with_name('edk2-arm-vars.fd'), variables)
    with image.open('wb') as output:
        output.truncate(128 * 1024 * 1024)
    fat_image = image
    if stage in ['storage', 'user']:
        mbr = bytearray(512)
        struct.pack_into('<I', mbr, 440, 0x4C4E5441)
        struct.pack_into('<BBBBBBBBII', mbr, 446, 0x80, 0xFE, 0xFF, 0xFF,
                         0x0C, 0xFE, 0xFF, 0xFF, 2048, 128 * 2048 - 2048)
        mbr[510:512] = b'\x55\xaa'
        with image.open('r+b') as output:
            output.write(mbr)
        run('mkfs.vfat', '-F', '32', '--offset=2048', '-h', '2048', image,
            stdout=subprocess.DEVNULL)
        fat_image = str(image) + '@@1048576'
    else:
        run('mkfs.vfat', '-F', '32', image, stdout=subprocess.DEVNULL)
    run('mmd', '-i', fat_image, '::/EFI', '::/EFI/BOOT')
    run('mcopy', '-i', fat_image, binary, '::/EFI/BOOT/BOOTARM.EFI')
    config = build / 'freeldr.ini'
    config.write_text(
        '[FREELOADER]\nTimeOut=0\nDefaultOS=LiberNT\nDebug=COM1\n\n'
        '[Operating Systems]\nLiberNT="LiberNT ARMv7"\n\n'
        '[LiberNT]\nBootType=Windows2003\n'
        f'SystemPath=multi(0)disk(0)rdisk(0)partition({1 if stage in ["storage", "user"] else 0})\\reactos\n'
        'Options=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200\n')
    run('mcopy', '-i', fat_image, config, '::/freeldr.ini')
    if stage in ['kernel', 'clock', 'storage', 'user']:
        payload = build / 'arm-kernel-payload'
        payload.mkdir(exist_ok=True)
        run(build / 'host-tools/bin/mkhive', '-h:SYSTEM', f'-d:{payload}',
            ROOT / 'modules/rostests/boot/arm/system.inf',
            *([ROOT / 'modules/rostests/boot/arm/storage.inf'] if stage in ['storage', 'user'] else []),
            stdout=subprocess.DEVNULL)
        run('mmd', '-i', fat_image, '::/reactos', '::/reactos/system32',
            '::/reactos/system32/config')
        for source, name in [
            (build / 'ntoskrnl/ntoskrnl.exe', 'ntoskrnl.exe'),
            (build / 'hal/halarm/hal.dll', 'hal.dll'),
            (build / 'drivers/base/kdrosdbg/kdcom.dll', 'kdcom.dll'),
            (build / 'drivers/base/bootvid/bootvid.dll', 'bootvid.dll'),
            (build / 'drivers/base/bootvid/lfbbvid.dll', 'lfbbvid.dll'),
            (build / 'media/nls/c_1252.nls', 'c_1252.nls'),
            (build / 'media/nls/c_437.nls', 'c_437.nls'),
            (ROOT / 'media/nls/l_intl.nls', 'l_intl.nls'),
        ]:
            run('mcopy', '-i', fat_image, source, f'::/reactos/system32/{name}')
        run('mcopy', '-i', fat_image, payload / 'system', '::/reactos/system32/config/system')
    if stage in ['storage', 'user']:
        for source in ['dll/ntdll/ntdll.dll', 'base/system/smss/smss.exe']:
            run('mcopy', '-i', fat_image, build / source, '::/reactos/system32/' + Path(source).name)
        run('mmd', '-i', fat_image, '::/reactos/system32/drivers')
        for source in ['drivers/storage/virtblk/virtblk.sys',
                       'drivers/filesystems/fastfat/fastfat.sys',
                       'boot/freeldr/freeldr/arm_storage_test.sys']:
            run('mcopy', '-i', fat_image, build / source,
                '::/reactos/system32/drivers/' + Path(source).name)
        data = build / 'arm-kernel-payload/storage-test.bin'
        data.write_bytes(bytes((index * 37 + 11) & 255 for index in range(4096)))
        run('mcopy', '-i', fat_image, data, '::/reactos/storage-test.bin')
    command = [
        'qemu-system-arm', '-machine', 'virt', '-cpu', 'cortex-a15',
        '-m', '512', '-smp', '1', '-display', 'none', '-monitor', 'none',
        '-serial', 'stdio', '-nic', 'none',
        '-drive', f'if=pflash,format=raw,readonly=on,file={firmware}',
        '-drive', f'if=pflash,format=raw,file={variables}',
        '-drive', f'if=none,format=raw,id=os,file={image}',
        '-device', 'virtio-blk-device,drive=os', '-device', 'ramfb',
        '-gdb', 'tcp:127.0.0.1:1234',
        '-qmp', f'unix:{qmp},server=on,wait=off']
    if modern:
        command.extend(['-global', 'virtio-mmio.force-legacy=false'])
    keep = False
    started = last_change = time.monotonic()
    previous_size = 0
    clock = {"interrupt_time": 0, "ticks": 0}
    with log.open('wb') as output:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                   pass_fds=(lock.fileno(),), start_new_session=True)
        try:
            while process.poll() is None:
                time.sleep(0.1)
                now = time.monotonic()
                size = log.stat().st_size
                if size != previous_size:
                    last_change, previous_size = now, size
                text = log.read_text(errors='replace')
                if any(marker in text for marker in [
                        'Exception PC at', 'ASSERT [', 'tests: FAIL', '[ABORT]', '[PREFETCH ABORT]', '[UNDEF]']):
                    stop_guest(qmp)
                    keep = True
                    raise RuntimeError(
                        f'{stage}: guest stopped for debugging (PID {process.pid}); '
                        f'gdb target remote 127.0.0.1:1234; log: {log}')
                if stage == 'storage' and 'ARM storage tests: PASS' in text:
                    stop_guest(qmp)
                    keep = True
                    break
                if stage == 'user' and 'NtProcessStartup(' in text and ') called' in text:
                    stop_guest(qmp)
                    keep = True
                    break
                if stage in ['storage', 'user'] and '*** Fatal System Error:' in text:
                    stop_guest(qmp)
                    keep = True
                    raise RuntimeError('storage: kernel stopped before the FAT I/O regression passed')
                if stage == 'clock' and '*** Fatal System Error:' in text:
                    stop_guest(qmp)
                    keep = True
                    clock = clock_snapshot(qmp)
                    break
                if stage == 'kernel' and 'Boot images: ntoskrnl.exe' in text:
                    stop_guest(qmp)
                    keep = True
                    break
                if stage == 'clock' and now - last_change >= 6 and now - started < 30:
                    stop_guest(qmp)
                    keep = True
                    snapshot = clock_snapshot(qmp)
                    if snapshot['interrupt_time'] <= clock['interrupt_time'] or snapshot['ticks'] <= clock['ticks']:
                        raise RuntimeError('clock: ARM clock counters stopped progressing')
                    clock = snapshot
                    last_change = time.monotonic()
                    guest_command(qmp, 'cont')
                    keep = False
                if now - started >= 30 or now - last_change >= 6:
                    if stage in ['abi', 'kernel', 'clock', 'storage', 'user'] and 'ARMv7 ABI tests: PASS' not in text:
                        stop_guest(qmp)
                        keep = True
                        raise RuntimeError(
                            f'{stage}: guest stopped after timeout/stall (PID {process.pid}); '
                            f'gdb target remote 127.0.0.1:1234; log: {log}')
                    break
            text = log.read_text(errors='replace')
            if stage == 'abi':
                passed = 'ARMv7 ABI tests: PASS' in text and 'tests: FAIL' not in text
            elif stage in ['kernel', 'clock', 'storage', 'user']:
                passed = all(marker in text for marker in [
                    'ARM kernel: KiInitializeSystem', 'Kernel address space:',
                    'Memory:', 'Boot images: ntoskrnl.exe'])
                if stage in ['storage', 'user']:
                    passed = passed and 'boot disk online' in text and 'ARM storage tests: PASS' in text and 'load failed' not in text
                if stage == 'user':
                    passed = passed and 'NtProcessStartup(' in text and ') called' in text
                if stage == 'clock':
                    passed = passed and 'BOOT DRIVERS LOADED' in text and all(
                        marker in text for marker in ['*** Fatal System Error: 0x0000007b',
                                                     '0xC0000034']) and clock['ticks'] > 0
            else:
                passed = all(marker in text for marker in [
                    'FatMount(0) success', 'LoadIdentifier=LiberNT ARMv7',
                    'Options=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200'])
            if not passed:
                raise RuntimeError(f'{stage}: milestone not reached; inspect {log}')
        finally:
            if not keep and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    result = {'stage': stage, 'passed': True, 'seconds': round(time.monotonic() - started, 2)}
    if stage in ['storage', 'user']:
        result['transport'] = 'virtio-mmio-v2' if modern else 'virtio-mmio-v1'
        result['milestone'] = 'native-smss-entry' if stage == 'user' else 'fat-file-io'
    if stage == 'clock':
        result.update(clock)
        result['expected_bugcheck'] = 'INACCESSIBLE_BOOT_DEVICE (empty storage fixture)'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    parser.add_argument('--firmware', type=Path, default=Path('/usr/share/qemu/edk2-arm-code.fd'))
    parser.add_argument('--stage', choices=['abi', 'loader', 'kernel', 'clock', 'storage', 'user', 'both'], default='both')
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--virtio-modern', action='store_true')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    cache = (build / 'CMakeCache.txt').read_text().splitlines()
    if not any(line.startswith('ARCH:') and line.split('=', 1)[-1] == 'arm' for line in cache):
        parser.error('--build-dir must be configured with ARCH=arm')
    with (build / '.arm-qemu.lock').open('w') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error('the ARM test image is in use by another test or a stopped guest')
        if not args.no_build:
            env = dict(os.environ, CCACHE_DISABLE='1')
            run('ninja', '-C', build, 'uefildr', 'arm_uefi_test', '-j8', env=env)
            if args.stage in ['storage', 'user']:
                run('ninja', '-C', build, 'virtblk', 'fastfat', 'arm_storage_test', 'ntdll', 'smss', '-j8', env=env)
            if args.stage in ['kernel', 'clock', 'storage', 'user']:
                run('ninja', '-C', build, 'ntoskrnl', 'hal', 'kdcom', 'bootvid', 'lfbbvid',
                    'media/nls/c_1252.nls', 'media/nls/c_437.nls', '-j8', env=env)
        stages = ['abi', 'loader'] if args.stage == 'both' else [args.stage]
        results = []
        try:
            for stage in stages:
                try:
                    result = boot(build, args.firmware.resolve(), stage, lock, args.virtio_modern)
                except Exception as error:
                    results.append({'stage': stage, 'passed': False, 'error': str(error)})
                    raise
                results.append(result)
                print(f'{stage}: PASS ({result["seconds"]}s)', flush=True)
        finally:
            (build / 'arm-uefi-results.json').write_text(json.dumps(results, indent=2) + '\n')
    if 'loader' in stages:
        print('FreeLoader read the boot configuration. Kernel startup is not covered by this test.')


if __name__ == '__main__':
    main()
