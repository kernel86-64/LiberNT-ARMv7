#!/usr/bin/env python3
"""Install the full default user hive and Aero visual style on an N7000 card.

PROJECT:     LiberNT N7000 theme installer
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Enable the Aero user theme on the N7000 microSD image
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""

import argparse
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def backup(path):
    target = path.with_name(path.name + '.theme-backup')
    if path.exists() and not target.exists():
        shutil.copy2(path, target)
        print(f'backup: {target}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--card', type=Path, default=Path('/run/media/kernel64/N7000_EFI'))
    parser.add_argument('--source', type=Path,
                        default=Path('/tmp/bootcd-full/Windows'))
    args = parser.parse_args()

    card = args.card.resolve()
    source = args.source.resolve()
    default_hive = source / 'system32/config/default'
    msstyles = source / 'Resources/Themes/Aero/aero.msstyles'
    if not default_hive.is_file():
        parser.error(f'full default user hive not found: {default_hive}')
    if not msstyles.is_file():
        parser.error(f'Aero style not found: {msstyles}')

    destination_style = card / 'Windows/Resources/Themes/Aero/aero.msstyles'
    destination_style.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(msstyles, destination_style)
    print(f'style: {destination_style}')

    for profile in ('Default', 'Administrator', 'Administrator.000'):
        destination = card / 'Users' / profile / 'NTUSER.DAT'
        destination.parent.mkdir(parents=True, exist_ok=True)
        backup(destination)
        shutil.copy2(default_hive, destination)
        print(f'profile: {destination}')

    subprocess.run(['sync'], check=True)
    print('theme installed; reboot and log on again')


if __name__ == '__main__':
    main()
