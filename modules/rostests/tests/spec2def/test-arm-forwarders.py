#!/usr/bin/env python3
"""
PROJECT:     LiberNT build tests
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Validate ARM forwarder strings and Thumb exports in a linked PE
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""
import pathlib
import re
import subprocess
import sys
import tempfile

spec2def, clang, readobj = sys.argv[1:]
targets = ['NTOSKRNL.RtlUnwind', 'NTOSKRNL.KeBugCheckEx',
           'NTOSKRNL.PsGetCurrentProcessId', 'NTOSKRNL.RtlUnwindEx']
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    spec = root / 'forward.spec'
    definition = root / 'forward.def'
    source = root / 'forward.c'
    image = root / 'forward.dll'
    spec.write_text('@ stdcall Local()\n' + ''.join(
        f'@ stdcall Forward{index}() {target}\n' for index, target in enumerate(targets)))
    source.write_text('void Local(void) {}\n')
    subprocess.run([spec2def, '-a=arm', '-n=forward.dll', '-d=' + str(definition), str(spec)], check=True)
    subprocess.run([clang, '--target=arm-w64-mingw32', '-fuse-ld=lld', '-nostdlib', '-shared',
                    '-Wl,--entry,Local,--exclude-all-symbols', str(source), str(definition),
                    '-o', str(image)], check=True)
    exports = subprocess.check_output([readobj, '--coff-exports', str(image)], text=True)
    actual = re.findall(r'ForwardedTo: (\S+)', exports)
    assert actual == targets, (actual, targets)
    local = re.search(r'Name: Local\s+RVA: (0x[0-9A-Fa-f]+)', exports)
    assert local and int(local[1], 16) & 1, exports
    subprocess.run([spec2def, '-a=arm', '--implib', '-n=forward.dll',
                    '-d=' + str(definition), str(spec)], check=True)
    assert ' DATA' not in definition.read_text()
    subprocess.run([spec2def, '-a=x86_64', '-n=forward.dll',
                    '-d=' + str(definition), str(spec)], check=True)
    assert ' DATA' not in definition.read_text()
print('ARM PE forwarder regression passed (strings, Thumb code, function imports)')
