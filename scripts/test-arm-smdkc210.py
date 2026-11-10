#!/usr/bin/env python3
"""Build and exercise Cortex-A9 boot primitives without changing the virt image."""

import argparse
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('arm_uefi_test', ROOT / 'scripts/test-arm-uefi.py')
UEFI = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(UEFI)


def test(build, lock, no_build):
    env = dict(os.environ, CCACHE_DISABLE='1')
    host_tests = {
        'arm-shortdesc-host': ['modules/rostests/boot/arm/shortdesc-host.c',
                               'boot/freeldr/freeldr/arch/arm/shortdesc.c'],
        'arm-processor-host': ['modules/rostests/boot/arm/processor-host.c']}
    if not no_build:
        subprocess.run(['ninja', '-C', str(build), 'arm_smdkc210_test', '-j8'], env=env, check=True)
        compiler = shutil.which('clang')
        if not compiler:
            raise RuntimeError('host clang is required for sanitized table-construction checks')
        for name, sources in host_tests.items():
            subprocess.run([
                compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
                '-fsanitize=address,undefined', '-I', str(ROOT / 'boot/freeldr/freeldr/arch/arm'),
                '-I', str(ROOT / 'ntoskrnl/include/internal/arm'),
                *(str(ROOT / source) for source in sources),
                '-o', str(build / name)], env=env, check=True)
    for name in host_tests:
        subprocess.run([str(build / name)], env=env, check=True)
    log, qmp = build / 'arm-boot.log', build / 'arm-qemu.qmp'
    command = [
        'qemu-system-arm', '-machine', 'smdkc210', '-m', '1G', '-smp', '2',
        '-kernel', str(build / 'arm-smdkc210.elf'), '-display', 'none',
        '-serial', 'stdio', '-monitor', 'none', '-nic', 'none',
        '-semihosting-config', 'enable=on,target=native',
        '-gdb', 'tcp:127.0.0.1:1234', '-qmp', f'unix:{qmp},server=on,wait=off']
    keep = False
    started = changed = time.monotonic()
    previous_size = 0
    with log.open('wb') as output:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output,
                                   stderr=subprocess.STDOUT, pass_fds=(lock.fileno(),),
                                   start_new_session=True)
        try:
            while process.poll() is None:
                time.sleep(0.1)
                now = time.monotonic()
                size = log.stat().st_size
                if size != previous_size:
                    changed, previous_size = now, size
                text = log.read_text(errors='replace')
                if 'tests: FAIL' in text or now - started >= 30 or now - changed >= 6:
                    UEFI.stop_guest(qmp)
                    keep = True
                    raise RuntimeError(f'SMDKC210 guest stopped for debugging (PID {process.pid}); '
                                       f'gdb: target remote 127.0.0.1:1234; log: {log}')
            text = log.read_text(errors='replace')
            markers = [
                'CPU name: ARM Cortex-A9',
                'section identity and 4K high aliases PASS',
                'read-only and user access permissions PASS',
                'translation faults and execute-never PASS',
                'break-before-make and TLB invalidation PASS',
                'self-directed SGI and IRQ return PASS',
                'counter and five comparator interrupts PASS',
                'SMDKC210 tests: PASS (processor/MMU only; no kernel startup)']
            if process.returncode or not all(marker in text for marker in markers):
                raise RuntimeError(f'SMDKC210 milestone not reached; inspect {log}')
        finally:
            if not keep and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    return {'stage': 'smdkc210-short-descriptors', 'passed': True,
            'seconds': round(time.monotonic() - started, 2),
            'milestone': 'Cortex-A9 MMU, GIC IRQ and MCT comparator; no kernel startup'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    parser.add_argument('--no-build', action='store_true')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    cache = (build / 'CMakeCache.txt').read_text().splitlines()
    if not any(line.startswith('ARCH:') and line.split('=', 1)[-1] == 'arm' for line in cache):
        parser.error('--build-dir must be configured with ARCH=arm')
    with (build / '.arm-qemu.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error('the ARM test log/debugger is in use by another test')
        try:
            result = test(build, lock, args.no_build)
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            (build / 'arm-uefi-results.json').write_text(json.dumps(
                {'stage': 'smdkc210-short-descriptors', 'passed': False, 'error': str(error)}, indent=2) + '\n')
            parser.exit(1, f'{error}\n')
        (build / 'arm-uefi-results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(f'SMDKC210: PASS ({result["seconds"]}s); processor/MMU only, no kernel startup')


if __name__ == '__main__':
    main()
