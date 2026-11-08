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
import time

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def stop_guest(path):
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(3)
        connection.connect(str(path))
        stream = connection.makefile('rwb')
        json.loads(stream.readline())
        for command in ['qmp_capabilities', 'stop']:
            stream.write((json.dumps({'execute': command}) + '\n').encode())
            stream.flush()
            while True:
                response = json.loads(stream.readline())
                if 'return' in response:
                    break
                if 'error' in response:
                    raise RuntimeError(response['error'])


def boot(build, firmware, stage, lock):
    image = build / 'arm-uefi-tests.img'
    variables = build / 'QEMU_VARS.fd'
    log = build / 'arm-boot.log'
    qmp = build / 'arm-qemu.qmp'
    application = 'arm_uefi_test' if stage == 'abi' else 'uefildr'
    binary = build / 'boot/freeldr/freeldr' / (application + '.efi')
    if not variables.exists():
        shutil.copyfile(firmware.with_name('edk2-arm-vars.fd'), variables)
    with image.open('wb') as output:
        output.truncate(128 * 1024 * 1024)
    run('mkfs.vfat', '-F', '32', image, stdout=subprocess.DEVNULL)
    run('mmd', '-i', image, '::/EFI', '::/EFI/BOOT')
    run('mcopy', '-i', image, binary, '::/EFI/BOOT/BOOTARM.EFI')
    config = build / 'freeldr.ini'
    config.write_text(
        '[FREELOADER]\nTimeOut=0\nDefaultOS=LiberNT\nDebug=COM1\n\n'
        '[Operating Systems]\nLiberNT="LiberNT ARMv7"\n\n'
        '[LiberNT]\nBootType=Windows2003\n'
        'SystemPath=multi(0)disk(0)rdisk(0)partition(0)\\reactos\n'
        'Options=/DEBUG /DEBUGPORT=COM1 /BAUDRATE=115200\n')
    run('mcopy', '-i', image, config, '::/freeldr.ini')
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
    keep = False
    started = last_change = time.monotonic()
    previous_size = 0
    with log.open('wb') as output:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                   pass_fds=(lock.fileno(),))
        try:
            while process.poll() is None:
                time.sleep(0.1)
                now = time.monotonic()
                size = log.stat().st_size
                if size != previous_size:
                    last_change, previous_size = now, size
                text = log.read_text(errors='replace')
                if 'Exception PC at' in text or 'ASSERT [' in text or 'tests: FAIL' in text:
                    stop_guest(qmp)
                    keep = True
                    raise RuntimeError(
                        f'{stage}: guest stopped for debugging (PID {process.pid}); '
                        f'gdb target remote 127.0.0.1:1234; log: {log}')
                if now - started >= 30 or now - last_change >= 6:
                    break
            text = log.read_text(errors='replace')
            if stage == 'abi':
                passed = 'ARMv7 ABI tests: PASS' in text and 'tests: FAIL' not in text
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
    return {'stage': stage, 'passed': True, 'seconds': round(time.monotonic() - started, 2)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    parser.add_argument('--firmware', type=Path, default=Path('/usr/share/qemu/edk2-arm-code.fd'))
    parser.add_argument('--stage', choices=['abi', 'loader', 'both'], default='both')
    parser.add_argument('--no-build', action='store_true')
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
        stages = ['abi', 'loader'] if args.stage == 'both' else [args.stage]
        results = []
        try:
            for stage in stages:
                try:
                    result = boot(build, args.firmware.resolve(), stage, lock)
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
