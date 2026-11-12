#!/usr/bin/env python3
"""Merge the Themes service and svchost group into an N7000 card.

PROJECT:     LiberNT N7000 theme service installer
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Add the missing Themes service and Svchost netsvcs group
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def export(regedit, environment, hive, prefix, key):
    result = subprocess.run([regedit, '--export', '--prefix', prefix, str(hive), key],
                            env=environment, capture_output=True, text=True)
    if result.returncode or '[' not in result.stdout:
        raise RuntimeError(f'cannot export {key} from {hive}')
    return result.stdout


def backup(path):
    target = path.with_name(path.name + '.theme-service-backup')
    if path.exists() and not target.exists():
        shutil.copy2(path, target)
        print(f'backup: {target}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card', type=Path, default=Path('/run/media/kernel64/N7000_EFI'))
    parser.add_argument('--hivex-root', type=Path, default=Path('/tmp/hivex-root'))
    parser.add_argument('--source', type=Path, default=Path('/tmp/bootcd-full/Windows'))
    args = parser.parse_args()

    card = args.card.resolve()
    source = args.source.resolve()
    regedit = args.hivex_root / 'usr/bin/hivexregedit'
    environment = os.environ.copy()
    environment['LD_LIBRARY_PATH'] = str(args.hivex_root / 'usr/lib')
    perl = sorted((args.hivex_root / 'usr/lib/perl5').glob('*/vendor_perl'))
    if perl:
        environment['PERL5LIB'] = str(perl[-1])
    if not regedit.is_file():
        parser.error(f'hivexregedit not found: {regedit}')

    system = card / 'Windows/System32/config/system'
    alternate = card / 'Windows/System32/config/SYSTEM.ALT'
    software = card / 'Windows/System32/config/software'
    for path in (system, software):
        if not path.is_file():
            parser.error(f'hive not found: {path}')
        backup(path)
    backup(alternate)

    themes = export(regedit, environment, source / 'system32/config/system',
                    r'HKEY_LOCAL_MACHINE\SYSTEM', r'ControlSet001\Services\Themes')
    svchost = export(regedit, environment, source / 'system32/config/software',
                     r'HKEY_LOCAL_MACHINE\SOFTWARE',
                     r'Microsoft\Windows NT\CurrentVersion\Svchost')

    with tempfile.TemporaryDirectory() as temporary:
        themes_path = Path(temporary) / 'themes.reg'
        svchost_path = Path(temporary) / 'svchost.reg'
        themes_path.write_text(themes)
        svchost_path.write_text(svchost)
        subprocess.run([str(regedit), '--merge', '--prefix', r'HKEY_LOCAL_MACHINE\SYSTEM',
                        str(system), str(themes_path)], env=environment, check=True)
        subprocess.run([str(regedit), '--merge', '--prefix', r'HKEY_LOCAL_MACHINE\SOFTWARE',
                        str(software), str(svchost_path)], env=environment, check=True)

    merged = export(regedit, environment, system, r'HKEY_LOCAL_MACHINE\SYSTEM',
                    r'ControlSet001\Services\Themes')
    if 'Themes' not in merged:
        parser.error('Themes service merge failed')
    shutil.copy2(system, alternate)
    for suffix in ('.LOG1', '.LOG2'):
        for name in ('SYSTEM', 'SOFTWARE'):
            log = card / 'Windows/System32/config' / (name + suffix)
            if log.exists():
                backup(log)
                log.unlink()
    subprocess.run(['sync'], check=True)
    print('Themes service merged; reboot and check the Start button/theme')


if __name__ == '__main__':
    main()
