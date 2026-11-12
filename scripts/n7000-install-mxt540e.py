#!/usr/bin/env python3
"""Install the mXT540E driver and root device on an N7000 card.

PROJECT:     LiberNT N7000 touchscreen installer
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Install the mXT540E HID driver and root device on an N7000 card
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
PREFIX = r'HKEY_LOCAL_MACHINE\SYSTEM'

REGISTRY = r"""Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SYSTEM\{control}\Control\Class\{{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}}]
"Class"="HIDClass"
"ClassDesc"="HID devices"

[HKEY_LOCAL_MACHINE\SYSTEM\{control}\Services\mxt540e]
"Type"=dword:00000001
"Start"=dword:00000003
"ErrorControl"=dword:00000001
"Group"="Extended Base"
"ImagePath"="system32\\drivers\\mxt540e.sys"

[HKEY_LOCAL_MACHINE\SYSTEM\{control}\Enum\ROOT\LIBERNTVIRTMXT540E\0000]
"HardwareID"="ROOT\\LIBERNTVIRTMXT540E"
"Service"="mxt540e"
"Class"="HIDClass"
"ClassGUID"="{{745A17A0-74D3-11D0-B6FE-00A0C90F57DA}}"
"ConfigFlags"=dword:00000000
"""


def backup_once(path):
    target = path.with_name(path.name + '.mxt540e-backup')
    if path.exists() and not target.exists():
        shutil.copy2(path, target)
        print(f'backup: {target}')
    return target


def hivex_environment(root):
    environment = os.environ.copy()
    if root is None:
        return environment
    root = root.resolve()
    library = root / 'usr/lib'
    perl = sorted((root / 'usr/lib/perl5').glob('*/vendor_perl'))
    if library.is_dir():
        current = environment.get('LD_LIBRARY_PATH')
        environment['LD_LIBRARY_PATH'] = str(library) + (':' + current if current else '')
    if perl:
        current = environment.get('PERL5LIB')
        environment['PERL5LIB'] = str(perl[-1]) + (':' + current if current else '')
    return environment


def hivex_tools(root):
    if root is None:
        regedit = shutil.which('hivexregedit')
        shell = shutil.which('hivexsh')
    else:
        regedit = str(root / 'usr/bin/hivexregedit')
        shell = str(root / 'usr/bin/hivexsh')
    return regedit, shell


def export_key(regedit, environment, hive, key):
    result = subprocess.run([regedit, '--export', '--prefix', PREFIX, str(hive), key],
                            env=environment, capture_output=True, text=True)
    return result.stdout if result.returncode == 0 and '[' in result.stdout else None


def ensure_key(regedit, shell, environment, hive, path):
    if export_key(regedit, environment, hive, path):
        return
    parent, _, name = path.rpartition('\\')
    if parent:
        ensure_key(regedit, shell, environment, hive, parent)
        script = f'cd \\{parent}\nadd {name}\ncommit\nquit\n'
        subprocess.run([shell, '-w', str(hive)], input=script, env=environment,
                       text=True, capture_output=True)


def control_set(regedit, environment, hive):
    text = export_key(regedit, environment, hive, 'Select')
    match = re.search(r'"Current"=dword:([0-9a-fA-F]+)', text or '')
    return f'ControlSet{int(match.group(1), 16):03d}' if match else 'ControlSet001'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card', type=Path, default=Path('/run/media/kernel64/N7000_EFI'))
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    parser.add_argument('--hivex-root', type=Path)
    parser.add_argument('--restore', action='store_true',
                        help='restore the SYSTEM hive and logs from the previous backup first')
    args = parser.parse_args()

    card = args.card.resolve()
    config = card / 'Windows/System32/config'
    hive = config / 'system'
    drivers = [
        ('drivers/hid/mxt540e/mxt540e.sys', 'mxt540e.sys'),
        ('drivers/hid/hidclass/hidclass.sys', 'hidclass.sys'),
        ('drivers/hid/hidparse/hidparse.sys', 'hidparse.sys'),
    ]
    if not hive.is_file():
        parser.error(f'SYSTEM hive not found: {hive}')
    for source, _ in drivers:
        if not (args.build_dir / source).is_file():
            parser.error(f'driver not built: {args.build_dir / source}')

    regedit, shell = hivex_tools(args.hivex_root)
    if not regedit or not Path(regedit).is_file() or not shell or not Path(shell).is_file():
        parser.error('hivexregedit and hivexsh are required; pass --hivex-root')
    environment = hivex_environment(args.hivex_root)

    system_backup = config / 'system.mxt540e-backup'
    if args.restore:
        if not system_backup.is_file():
            parser.error(f'SYSTEM backup not found: {system_backup}')
        shutil.copy2(system_backup, hive)
        print(f'restored: {hive}')
        for suffix in ('.LOG1', '.LOG2'):
            source = config / ('SYSTEM' + suffix + '.mxt540e-backup')
            target = config / ('SYSTEM' + suffix)
            if source.is_file():
                shutil.copy2(source, target)
                print(f'restored: {target}')
    else:
        backup_once(hive)

    alternate = config / 'SYSTEM.ALT'
    if alternate.exists():
        backup_once(alternate)
        alternate.unlink()

    for suffix in ('.LOG1', '.LOG2'):
        log = config / ('SYSTEM' + suffix)
        if log.exists():
            backup_once(log)
            log.unlink()

    control = control_set(regedit, environment, hive)
    for parent in [f'{control}\\Control', f'{control}\\Control\\Class',
                   f'{control}\\Enum', f'{control}\\Enum\\ROOT',
                   f'{control}\\Enum\\ROOT\\LIBERNTVIRTMXT540E']:
        ensure_key(regedit, shell, environment, hive, parent)

    with tempfile.NamedTemporaryFile('w', suffix='.reg', delete=False) as stream:
        stream.write(REGISTRY.format(control=control))
        registry_file = stream.name
    try:
        subprocess.run([regedit, '--merge', '--prefix', PREFIX, str(hive), registry_file],
                       env=environment, check=True, capture_output=True, text=True)
    finally:
        os.unlink(registry_file)

    if export_key(regedit, environment, hive, f'{control}\\Services\\mxt540e') is None:
        parser.error('registry merge did not create the mxt540e service')

    destination = card / 'Windows/System32/drivers'
    destination.mkdir(parents=True, exist_ok=True)
    for source, name in drivers:
        shutil.copy2(args.build_dir / source, destination / name)
        print(f'driver: {destination / name}')

    shutil.copy2(hive, alternate)
    print(f'mirror: {alternate}')
    subprocess.run(['sync'], check=True)
    print('installed; reboot the phone and look for "mxt540e: started" in the OS log')


if __name__ == '__main__':
    main()
