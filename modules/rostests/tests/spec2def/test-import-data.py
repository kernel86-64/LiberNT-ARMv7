#!/usr/bin/env python3
"""
PROJECT:     LiberNT build tests
LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
PURPOSE:     Check Thumb function exports with pointer-only import libraries
COPYRIGHT:   Copyright 2026 LiberNT contributors
"""
import pathlib
import re
import subprocess
import sys
import tempfile

spec2def, clang, readobj, dlltool = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    spec = root / 'provider.spec'
    definition = root / 'provider.def'
    source = root / 'provider.c'
    image = root / 'provider.dll'
    library = root / 'provider.a'
    source.write_text('void *Local(void *p) { return p; }\n')
    spec.write_text('@ cdecl -import-data Local(ptr)\n')
    subprocess.run([spec2def, '-a=arm', '-n=provider.dll', '-d=' + str(definition), str(spec)], check=True)
    assert ' DATA' not in definition.read_text()
    subprocess.run([clang, '--target=arm-w64-mingw32', '-fuse-ld=lld', '-nostdlib', '-shared',
                    '-Wl,--entry,Local,--exclude-all-symbols', str(source), str(definition),
                    '-o', str(image)], check=True)
    exports = subprocess.check_output([readobj, '--coff-exports', str(image)], text=True)
    local = re.search(r'Name: Local\s+RVA: (0x[0-9A-Fa-f]+)', exports)
    assert local and int(local[1], 16) & 1, exports
    subprocess.run([spec2def, '-a=arm', '--implib', '-n=provider.dll',
                    '-d=' + str(definition), str(spec)], check=True)
    assert 'Local DATA' in definition.read_text()
    subprocess.run([dlltool, '-m', 'arm', '-d', str(definition), '-l', str(library)], check=True)
    source.write_text('typedef void *(*FN)(void *);\nextern FN __imp_Local;\n'
                      'void *Local(void *p) { return p; }\n'
                      'void *Probe(void *p) { return __imp_Local(p); }\n')
    subprocess.run([clang, '--target=arm-w64-mingw32', '-fuse-ld=lld', '-nostdlib',
                    '-Wl,--entry,Probe,--subsystem,native', str(source), str(library),
                    '-o', str(root / 'consumer.exe')], check=True)
    imports = subprocess.check_output([readobj, '--coff-imports', str(root / 'consumer.exe')], text=True)
    assert 'Name: provider.dll' in imports and 'Symbol: Local ' in imports, imports
    for arch in ['i386', 'x86_64', 'arm64']:
        subprocess.run([spec2def, '-a=' + arch, '-n=provider.dll',
                        '-d=' + str(definition), str(spec)], check=True)
        assert ' DATA' not in definition.read_text(), (arch, definition.read_text())
        subprocess.run([spec2def, '-a=' + arch, '--implib', '-n=provider.dll',
                        '-d=' + str(definition), str(spec)], check=True)
        assert ' DATA' in definition.read_text(), (arch, definition.read_text())
print('Function export and pointer-only import regression passed')
