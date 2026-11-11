#!/usr/bin/env python3
"""Check NVS root-table lifecycle and slot accounting with 32/64-bit PTE words."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    'modules/rostests/boot/arm/nvs-root-host.c',
    'ntoskrnl/nvs/core/mmsystem.c', 'ntoskrnl/nvs/core/pfndb.c',
    'ntoskrnl/nvs/core/adopt.c', 'ntoskrnl/nvs/core/pagetable.c',
    'ntoskrnl/nvs/vad/vadtree.c']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    compiler = shutil.which('clang')
    if not compiler:
        parser.error('host clang is required for ASan/UBSan checks')
    if not (build / 'CMakeCache.txt').is_file():
        parser.error('--build-dir must be an existing build directory')
    env = dict(os.environ, CCACHE_DISABLE='1')
    for size, compact in [(4, 32), (8, 64), (8, 32)]:
        output = build / f'arm-nvs-root-host{size * 8}'
        subprocess.run([
            compiler, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
            '-Wl,--gc-sections', '-DMM_HOST_TEST', f'-DMI_ARCH_PTE_BYTES={size}', f'-DMI_ARCH_SOFTWARE_PTE_BITS={compact}',
            '-I', str(ROOT / 'ntoskrnl'), '-I', str(ROOT / 'submodules/host-tests'),
            *(str(ROOT / source) for source in SOURCES), '-o', str(output)], env=env, check=True)
        subprocess.run([str(output)], env=env, check=True)

    short = ROOT / 'boot/freeldr/freeldr/arch/arm'
    output = build / 'arm-nvs-shortpte-host'
    subprocess.run([
        compiler, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
        '-Wl,--gc-sections', '-DMM_HOST_TEST', '-DMI_ARCH_PTE_BYTES=8',
        '-DMI_ARCH_SOFTWARE_PTE_BITS=32', '-I', str(ROOT / 'ntoskrnl'),
        '-I', str(ROOT / 'submodules/host-tests'), '-I', str(short),
        str(ROOT / 'modules/rostests/boot/arm/shortpte-host.c'),
        str(ROOT / 'ntoskrnl/nvs/arch/arm/pte.c'),
        str(ROOT / 'ntoskrnl/nvs/arch/arm/shorttable.c'), str(short / 'shortdesc.c'),
        '-o', str(output)], env=env, check=True)
    subprocess.run([str(output)], env=env, check=True)

    output = build / 'arm-nvs-shorttable-host'
    subprocess.run([
        compiler, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections',
        '-Wl,--gc-sections', '-DMM_HOST_TEST', '-DMI_ARCH_PTE_BYTES=8',
        '-DMI_ARCH_SOFTWARE_PTE_BITS=32', '-I', str(ROOT / 'ntoskrnl'),
        '-I', str(ROOT / 'submodules/host-tests'),
        str(ROOT / 'modules/rostests/boot/arm/shorttable-host.c'),
        str(ROOT / 'ntoskrnl/nvs/arch/arm/pte.c'),
        str(ROOT / 'ntoskrnl/nvs/arch/arm/shorttable.c'),
        '-o', str(output)], env=env, check=True)
    subprocess.run([str(output)], env=env, check=True)


if __name__ == '__main__':
    main()
