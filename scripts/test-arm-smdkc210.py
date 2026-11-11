#!/usr/bin/env python3
"""Build and exercise Cortex-A9 boot primitives without changing the virt image."""

import argparse
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('arm_uefi_test', ROOT / 'scripts/test-arm-uefi.py')
UEFI = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(UEFI)
DESKTOP_FONTS = ['Marlett.ttf', 'selawk.ttf', 'selawkb.ttf', 'selawkl.ttf',
                 'selawksb.ttf', 'selawksl.ttf', 'segmdl2.ttf']


def physical_read(qmp, address, length):
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(3)
        connection.connect(str(qmp))
        stream = connection.makefile('rwb')
        json.loads(stream.readline())
        for command in [
                {'execute': 'qmp_capabilities'},
                {'execute': 'human-monitor-command', 'arguments': {
                    'command-line': f'xp /{length}bx 0x{address:x}'}}]:
            stream.write((json.dumps(command) + '\n').encode())
            stream.flush()
            while True:
                result = json.loads(stream.readline())
                if 'error' in result:
                    raise RuntimeError(result['error'])
                if 'return' in result:
                    break
        data = bytes(int(value, 16) for line in result['return'].splitlines()
                     for value in re.findall(r'0x([0-9a-fA-F]{2})\b', line.partition(':')[2]))
        if len(data) != length:
            raise RuntimeError('short physical-memory read from QEMU')
        return data


def screen_pixels(qmp, build):
    path = build / 'arm-screen.ppm'
    UEFI.guest_command(qmp, 'screendump', {'filename': str(path)})
    magic, dimensions, maximum, pixels = path.read_bytes().split(b'\n', 3)
    if (magic, dimensions, maximum) != (b'P6', b'800 1280', b'255') or len(pixels) != 800 * 1280 * 3:
        raise RuntimeError('unexpected SMDKC210 input framebuffer')
    return pixels


def screen_region(pixels, x, y, width, height):
    return b''.join(pixels[((y + row) * 800 + x) * 3:((y + row) * 800 + x + width) * 3]
                    for row in range(height))


def drain_os_log(qmp, output, head):
    magic, current = struct.unpack_from('<II', physical_read(qmp, 0x5e900000, 16))
    if magic != 0x474c534f:
        return head
    capacity = 0x80000 - 16
    head = max(head, current - capacity)
    while head < current:
        offset = head % capacity
        count = min(current - head, capacity - offset, 4096)
        output.write(physical_read(qmp, 0x5e900010 + offset, count))
        head += count
    return head


def click_at(qmp, x, y):
    UEFI.guest_command(qmp, 'input-send-event', {'events': [
        {'type': 'abs', 'data': {'axis': 'x', 'value': round(x * 32767 / 800)}},
        {'type': 'abs', 'data': {'axis': 'y', 'value': round(y * 32767 / 1280)}}]})
    for down in [True, False]:
        UEFI.guest_command(qmp, 'input-send-event', {'events': [
            {'type': 'btn', 'data': {'down': down, 'button': 'left'}}]})
        time.sleep(0.1)


def key_chord(qmp, *keys):
    UEFI.guest_command(qmp, 'send-key', {
        'keys': [{'type': 'qcode', 'data': key} for key in keys], 'hold-time': 100})


def usb_key(qmp, key):
    key_chord(qmp, key)
    time.sleep(0.25)


def exercise_usb_input(qmp, build, deadline):
    before = screen_pixels(qmp, build)
    UEFI.guest_command(qmp, 'input-send-event', {'events': [
        {'type': 'abs', 'data': {'axis': 'x', 'value': 24576}},
        {'type': 'abs', 'data': {'axis': 'y', 'value': 8192}}]})
    time.sleep(0.4)
    pointer = screen_pixels(qmp, build)
    if screen_region(before, 590, 310, 40, 40) == screen_region(pointer, 590, 310, 40, 40):
        raise RuntimeError('USB tablet did not move the SMDKC210 cursor')
    usb_key(qmp, 'spc')
    while time.monotonic() < deadline:
        time.sleep(0.2)
        keyboard = screen_pixels(qmp, build)
        if screen_region(pointer, 200, 1190, 400, 60) != screen_region(keyboard, 200, 1190, 400, 60):
            return
    raise RuntimeError('USB keyboard did not dismiss the SMDKC210 lock screen')


def exercise_login(qmp, build, output, head, manual_login):
    started = changed = time.monotonic()
    deadline = started + 30
    attempted = False
    previous_clock = UEFI.clock_snapshot(qmp)
    UEFI.guest_command(qmp, 'cont')
    while time.monotonic() < deadline:
        new_head = drain_os_log(qmp, output, head)
        if new_head != head:
            head, changed = new_head, time.monotonic()
        text = (build / 'arm-boot.log').read_text(errors='replace')
        if any(marker in text for marker in ['*** STOP:', '*** Fatal System Error:',
                'tests: FAIL', 'Kill explorer.exe', 'Kill winlogon.exe', 'ARM account setup: NetUserAdd failed']):
            raise RuntimeError('SMDKC210 login failed; inspect the stopped guest')
        if manual_login and not attempted and 'ARM account setup: PASS' in text and screen_pixels(qmp, build)[:3] != bytes(3):
            exercise_usb_input(qmp, build, deadline)
            time.sleep(0.7)
            click_at(qmp, 100, 1220)
            time.sleep(0.3)
            enter_credentials(qmp)
            attempted, changed = True, time.monotonic()
        if 'ARM user session: PASS (tester, profile, Explorer desktop and taskbar)' in text:
            return head, round(time.monotonic() - started, 2)
        if time.monotonic() - changed >= 6:
            clock = UEFI.clock_snapshot(qmp)
            UEFI.guest_command(qmp, 'cont')
            if any(clock[key] <= previous_clock[key] for key in ['interrupt_time', 'ticks']):
                raise RuntimeError('SMDKC210 clock stalled during interactive login')
            previous_clock, changed = clock, time.monotonic()
        time.sleep(0.1)
    raise RuntimeError('SMDKC210 interactive login exceeded its 30s phase limit')


def enter_credentials(qmp):
    for y, keys in [(690, ['t', 'e', 's', 't', 'e', 'r']), (750, ['a', 'r', 'm', 'v', '7'])]:
        click_at(qmp, 300, y)
        time.sleep(0.3)
        usb_key(qmp, 'home')
        key_chord(qmp, 'shift', 'end')
        time.sleep(0.3)
        usb_key(qmp, 'backspace')
        for key in keys:
            usb_key(qmp, key)
    usb_key(qmp, 'ret')


def exercise_desktop(qmp, build, output, head):
    started = time.monotonic()
    deadline, checked_clock = started + 30, started
    previous_clock = UEFI.clock_snapshot(qmp)
    UEFI.guest_command(qmp, 'cont')
    baseline = screen_pixels(qmp, build)
    for keys, region, label in [
            (('ctrl', 'esc'), (0, 500, 600, 740), 'Start menu'),
            (('meta_l', 'e'), (100, 100, 600, 1000), 'Explorer window')]:
        key_chord(qmp, *keys)
        changed = False
        while time.monotonic() < deadline:
            head = drain_os_log(qmp, output, head)
            time.sleep(0.2)
            pixels = screen_pixels(qmp, build)
            previous = screen_region(baseline, *region)
            current = screen_region(pixels, *region)
            text = (build / 'arm-boot.log').read_text(errors='replace')
            if (f'ARM UI: {label} responsive' in text and
                    sum(left != right for left, right in zip(previous, current)) > 10000):
                changed = True
                break
            if time.monotonic() - checked_clock >= 6:
                clock = UEFI.clock_snapshot(qmp)
                UEFI.guest_command(qmp, 'cont')
                if any(clock[key] <= previous_clock[key] for key in ['interrupt_time', 'ticks']):
                    raise RuntimeError('SMDKC210 clock stalled during the desktop test')
                previous_clock, checked_clock = clock, time.monotonic()
        if not changed:
            raise RuntimeError(f'SMDKC210 {label} did not appear within the 30s desktop phase')
        if label == 'Start menu':
            UEFI.input_key(qmp, 'esc')
            time.sleep(0.4)
            baseline = screen_pixels(qmp, build)
    return head, round(time.monotonic() - started, 2)


def desktop_registry(build):
    path = build / 'arm-kernel-payload/desktop-fonts.inf'
    lines = ['[Version]', 'Signature="$Windows NT$"', '', '[AddReg]']
    key = 'SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts'
    lines.extend(f'HKLM,"{key}","{Path(name).stem} (TrueType)",0x00000000,"{name}"'
                 for name in DESKTOP_FONTS)
    lines.extend(line for line in (ROOT / 'boot/bootdata/hivesft.inf').read_text().splitlines()
                 if '\\FontSubstitutes","Segoe UI' in line and 'Selawik' in line)
    lines.append('HKLM,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon","Userinit",'
                 '0x00020000,"%SystemRoot%\\system32\\arm_account_setup.exe --verify-interface,'
                 '%SystemRoot%\\system32\\userinit.exe"')
    path.write_text('\n'.join(lines) + '\n')
    return path


def prepare_desktop(build, image, no_build):
    if not no_build:
        UEFI.run('ninja', '-C', build, 'win8.msstyles', 'comctl32_v6', '-j8',
                 env=dict(os.environ, CCACHE_DISABLE='1'))
    directories = ['::/reactos/Resources', '::/reactos/Resources/Themes',
                   '::/reactos/Resources/Themes/Aero', '::/reactos/Web',
                   '::/reactos/Web/Wallpaper', '::/reactos/winsxs', '::/reactos/winsxs/manifests']
    UEFI.run('mmd', '-i', image, *directories)
    UEFI.run('mcopy', '-o', '-i', image,
             build / 'media/themes/Win8/win8.msstyles/win8.msstyles',
             '::/reactos/Resources/Themes/Aero/aero.msstyles')
    UEFI.run('mcopy', '-o', '-i', image, ROOT / 'modules/wallpapers/BlueHorizon.png',
             '::/reactos/Web/Wallpaper/BlueHorizon.png')
    for name in DESKTOP_FONTS:
        UEFI.run('mcopy', '-o', '-i', image, ROOT / 'media/fonts' / name, '::/reactos/Fonts/' + name)
    for version, module in [('5.82.2600.2982', 'comctl32'), ('6.0.2600.2982', 'comctl32_v6')]:
        assembly = f'arm_microsoft.windows.common-controls_6595b64144ccf1df_{version}_none_deadbeef'
        UEFI.run('mmd', '-i', image, '::/reactos/winsxs/' + assembly)
        UEFI.run('mcopy', '-o', '-i', image, build / 'dll/win32' / module / 'comctl32.dll',
                 '::/reactos/winsxs/' + assembly + '/comctl32.dll')
        UEFI.run('mcopy', '-o', '-i', image, ROOT / 'dll/win32/comctl32' / (assembly + '.manifest'),
                 '::/reactos/winsxs/manifests/' + assembly + '.manifest')


def prepare_kernel(build, stage, no_build, manual_login=False):
    startup = stage in ['startup', 'input', 'login']
    input_devices = stage in ['input', 'login']
    login = stage == 'login'
    modules = UEFI.session_modules(build, dict(os.environ, CCACHE_DISABLE='1'), no_build,
                                   startup=True, login=login,
                                   extra_modules=['imm32.dll'] if login else []) if startup else []
    UEFI.boot(build, Path('/usr/share/qemu/edk2-arm-code.fd'), 'csr' if login else 'session', None,
              modules=modules, login=login, pagefile=True, prepare_only=True)
    payload = build / 'arm-kernel-payload'
    sources = [ROOT / name for name in [
        'modules/rostests/boot/arm/n7000-storage.inf',
        'drivers/bus/sd/sdbus_reg.inf',
        'drivers/storage/class/sffdisk/sffdisk_reg.inf',
        'drivers/storage/partmgr/partmgr_reg.inf']]
    infs = payload / 'n7000-infs.txt'
    infs.write_text(''.join(str(ROOT / name) + '\n' for name in [
        'drivers/bus/acpi/acpi.inf', 'drivers/bus/sd/sdbus.inf',
        'drivers/storage/class/sffdisk/sffdisk.inf', 'media/inf/hal.inf']))
    if input_devices:
        infs.write_text(infs.read_text() + ''.join(str(ROOT / 'media/inf' / name) + '\n'
                        for name in ['usbport.inf', 'usb.inf', 'input.inf', 'keyboard.inf', 'msmouse.inf']))
    registry = payload / 'n7000-reg-infs.txt'
    registry.write_text(''.join(str(path) + '\n' for path in sources))
    UEFI.run(build / 'host-tools/bin/mkdrvdb', 'arm', payload / 'n7000-drivers.inf',
             infs, registry)
    UEFI.run(build / 'host-tools/bin/mkhive', '-h:SYSTEM,SOFTWARE,SAM,SECURITY,DEFAULT' if startup else '-h:SYSTEM', f'-d:{payload}',
             ROOT / 'modules/rostests/boot/arm/system.inf',
             ROOT / 'modules/rostests/boot/arm/session.inf', *sources,
             *([ROOT / 'modules/rostests/boot/arm/pagefile.inf'] if stage in ['session', 'csr', 'startup', 'input', 'login'] else []),
             payload / 'n7000-drivers.inf',
             *([ROOT / name for name in [
                 'drivers/directx/dxgkrnl/wddm_reg.inf',
                 'drivers/directx/softgpu/softgpu_root_reg.inf',
                 'drivers/filesystems/npfs/npfs_reg.inf',
                 'drivers/crypto/ksecdd/ksecdd_reg.inf',
                 'modules/rostests/boot/arm/graphics.inf',
                 'modules/rostests/boot/arm/startup.inf']] if startup else []),
             *([ROOT / name for name in ['drivers/input/kbdclass/kbdclass_reg.inf',
                 'drivers/input/mouclass/mouclass_reg.inf',
                 'modules/rostests/boot/arm/smdkc210-input.inf']] if input_devices else []),
             *([ROOT / 'boot/bootdata/hivecls.inf', ROOT / 'boot/bootdata/hivedef.inf',
                 UEFI.session_registry(build, modules),
                 ROOT / 'modules/rostests/boot/arm/account.inf', desktop_registry(build)] if login else []),
             *([ROOT / 'modules/rostests/boot/arm/autologon.inf'] if login and not manual_login else []))
    image = str(build / 'arm-uefi-tests.img') + '@@1048576'
    UEFI.run('mcopy', '-o', '-i', image, build / 'boot/freeldr/freeldr/uefildr.efi',
             '::/EFI/BOOT/BOOTARM.EFI')
    for source, name in [('ntoskrnl/ntoskrnl.exe', 'ntoskrnl.exe'),
                         ('hal/halarm/hal.dll', 'hal.dll')]:
        UEFI.run('mcopy', '-o', '-i', image, build / source, '::/reactos/system32/' + name)
    for source in ['drivers/bus/acpi/acpi.sys', 'drivers/bus/sd/sdbus.sys',
                   'drivers/storage/class/sffdisk/sffdisk.sys',
                   'drivers/storage/partmgr/partmgr.sys',
                   'drivers/storage/mountmgr/mountmgr.sys',
                   'drivers/filesystems/fastfat/fastfat.sys',
                   'boot/freeldr/freeldr/arm_n7000_test.sys']:
        UEFI.run('mcopy', '-o', '-i', image, build / source,
                 '::/reactos/system32/drivers/' + Path(source).name)
    UEFI.run('mcopy', '-o', '-i', image, payload / 'system',
             '::/reactos/system32/config/system')
    if startup:
        for source in ['drivers/directx/dxgmms2/dxgmms2.sys',
                       'drivers/directx/dxgkrnl/dxgkrnl.sys',
                       'drivers/directx/softgpu/BasicDisplay.sys',
                       'drivers/filesystems/npfs/npfs.sys',
                       'drivers/crypto/ksecdd/ksecdd.sys']:
            UEFI.run('mcopy', '-o', '-i', image, build / source,
                     '::/reactos/system32/drivers/' + Path(source).name)
        UEFI.run('mcopy', '-o', '-i', image, build / 'win32ss/drivers/displays/framebuf/framebuf.dll',
                 '::/reactos/system32/framebuf.dll')
        for hive in ['software', 'sam', 'security', 'default']:
            UEFI.run('mcopy', '-o', '-i', image, payload / hive, '::/reactos/system32/config/' + hive)
        if subprocess.run(['mdir', '-i', image, '::/reactos/Fonts'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
            UEFI.run('mmd', '-i', image, '::/reactos/Fonts')
        for name in ['FreeSans.ttf', 'tahoma.ttf', 'tahomabd.ttf']:
            UEFI.run('mcopy', '-o', '-i', image, ROOT / 'media/fonts' / name, '::/reactos/Fonts/' + name)
        for module in modules:
            destination = ('::/reactos/SystemResources/' + module.name.replace('_mun.dll', '.dll.mun')
                           if module.name.endswith('_mun.dll') else
                           ('::/reactos/' if module.name == 'explorer.exe' else '::/reactos/system32/') + module.name)
            UEFI.run('mcopy', '-o', '-i', image, module, destination)
        if login:
            UEFI.run('mcopy', '-o', '-i', image, payload / 'default', '::/Users/Default/NTUSER.DAT')
            prepare_desktop(build, image, no_build)
    usb_drivers = ['usbport', 'usbd', 'usbhub']
    if input_devices:
        usb_drivers.extend(['usbehci', 'hidusb', 'hidclass', 'hidparse',
                            'kbdhid', 'mouhid', 'kbdclass', 'mouclass'])
    for name in usb_drivers:
        source = next((build / 'drivers').rglob(name + '.sys'))
        UEFI.run('mcopy', '-o', '-i', image, source, '::/reactos/system32/drivers/' + name + '.sys')
    startup = build / 'n7000-qemu-startup.nsh'
    startup.write_text('@echo -off\nfs0:\n\\EFI\\BOOT\\BOOTARM.EFI\n')
    UEFI.run('mcopy', '-o', '-i', image, startup, '::/startup.nsh')


def firmware_test(build, lock, firmware, no_build, stage, manual_login):
    if firmware.with_suffix('.target').read_text().strip() != 'smdkc210':
        raise RuntimeError('the firmware must be built with --smdkc210')
    image = build / 'arm-uefi-tests.img'
    if not no_build:
        targets = ['uefildr', 'ntoskrnl', 'hal', 'acpi', 'sdbus', 'sffdisk', 'partmgr',
                   'mountmgr', 'fastfat', 'arm_n7000_test', 'arm_smdkc210_boot',
                   'ntdll', 'smss', 'autochk', 'csrss', 'win32k', 'ftfd', 'hidparse', 'csrsrv']
        if stage in ['startup', 'input', 'login']:
            targets.extend(['dxgmms2', 'dxgkrnl', 'softgpu', 'npfs', 'ksecdd', 'framebuf'])
        if stage in ['input', 'login']:
            targets.extend(['usbport', 'usbd', 'usbehci', 'usbhub', 'hidusb', 'hidclass',
                            'kbdhid', 'mouhid', 'kbdclass', 'mouclass'])
        UEFI.run('ninja', '-C', build, *targets, '-j8', env=dict(os.environ, CCACHE_DISABLE='1'))
    prepare_kernel(build, stage, no_build, manual_login)
    with image.open('rb') as source:
        mbr = source.read(512)
    if (mbr[510:512] != b'\x55\xaa' or
            struct.unpack_from('<II', mbr, 454) != (2048, image.stat().st_size // 512 - 2048)):
        raise RuntimeError('the reusable image must contain the ARM user fixture in MBR partition 1')
    log, qmp = build / 'arm-boot.log', build / 'arm-qemu.qmp'
    command = [
        'qemu-system-arm', '-machine', 'smdkc210', '-m', '1G', '-smp', '2',
        '-display', 'none', '-serial', 'stdio', '-monitor', 'none', '-nic', 'none',
        '-gdb', 'tcp:127.0.0.1:1234', '-qmp', f'unix:{qmp},server=on,wait=off',
        '-device', f'loader,file={firmware},addr=0x40010000,force-raw=on',
        '-device', f'loader,file={build / "arm-smdkc210-boot.elf"},cpu-num=0',
        '-drive', f'if=sd,index=2,format=raw,file={build / "arm-uefi-tests.img"}']
    if stage in ['input', 'login']:
        command.extend(['-device', 'usb-kbd,bus=usb-bus.0,usb_version=2',
                        '-device', 'usb-tablet,bus=usb-bus.0,usb_version=2'])
    log.write_bytes(b'')
    head, keep = 0, False
    startup_ready = None
    startup_clock = None
    started = changed = time.monotonic()
    previous_size = 0
    with log.open('ab', buffering=0) as output:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output,
                                   stderr=subprocess.STDOUT, pass_fds=(lock.fileno(),),
                                   start_new_session=True)
        try:
            while process.poll() is None:
                time.sleep(0.1)
                text = log.read_text(errors='replace')
                if qmp.exists() and 'Attempting to exit bootsevices' in text:
                    head = drain_os_log(qmp, output, head)
                now, size = time.monotonic(), log.stat().st_size
                if size != previous_size:
                    changed, previous_size = now, size
                text = log.read_text(errors='replace')
                passed = all(marker in text for marker in [
                    'ARM N7000 ACPI cache tests: PASS', 'ARM N7000 clock tests: PASS',
                    'ARM N7000 RTC tests: PASS',
                    'ARM N7000 storage tests: PASS', 'ARM N7000 overwrite tests: PASS', 'NtProcessStartup('])
                if stage in ['session', 'csr', 'startup', 'input', 'login']:
                    passed = passed and all(marker in text for marker in [
                        'Paging file \\??\\C:\\pagefile.sys created, 16 MB, maximum 32 MB',
                        'SMSS: Boot environment initialized'])
                if stage in ['csr', 'startup', 'input', 'login']:
                    passed = passed and 'CSRSS: Server initialized' in text
                if stage in ['startup', 'input', 'login']:
                    passed = passed and all(marker in text for marker in [
                        'WINLOGON: Session initialized', 'LSASS: Security services initialized',
                        'desktop ready 800x1280 (scanout)', 'UEFI GOP Framebuffer'])
                if stage in ['startup', 'input', 'login'] and passed:
                    if startup_ready is None:
                        startup_clock = UEFI.clock_snapshot(qmp)
                        UEFI.guest_command(qmp, 'cont')
                        startup_ready = time.monotonic()
                    passed = now - startup_ready >= (0 if stage == 'login' else 3)
                failed = any(marker in text for marker in [
                    'tests: FAIL', '*** STOP:', 'Exception PC at', 'ASSERT ['])
                if passed or failed or now - started >= 30 or now - changed >= 6:
                    UEFI.stop_guest(qmp)
                    keep = True
                    if not passed:
                        reason = 'guest failure' if failed else '30s limit' if now - started >= 30 else '6s stall'
                        raise RuntimeError(f'SMDKC210 kernel stopped: {reason} (PID {process.pid}); '
                                           f'gdb: target remote 127.0.0.1:1234; log: {log}')
                    if stage == 'input':
                        UEFI.guest_command(qmp, 'cont')
                        try:
                            exercise_usb_input(qmp, build, started + 30)
                        finally:
                            UEFI.stop_guest(qmp)
                    login_seconds = None
                    if stage == 'login':
                        try:
                            head, login_seconds = exercise_login(qmp, build, output, head, manual_login)
                            UEFI.stop_guest(qmp)
                            head, desktop_seconds = exercise_desktop(qmp, build, output, head)
                        finally:
                            UEFI.stop_guest(qmp)
                    if stage in ['startup', 'input', 'login']:
                        clock = UEFI.clock_snapshot(qmp)
                        if any(clock[key] <= startup_clock[key] for key in ['interrupt_time', 'ticks']):
                            raise RuntimeError('Cortex-A9 clock stopped after Winlogon initialization')
                        UEFI.guest_command(qmp, 'screendump', {
                            'filename': str(build / 'arm-screen.png'), 'format': 'png'})
                        screen = (build / 'arm-screen.png').read_bytes()
                        if screen[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack_from('>II', screen, 16) != (800, 1280):
                            raise RuntimeError('unexpected SMDKC210 scanout dimensions')
                    data = subprocess.check_output([
                        'mtype', '-i', str(image) + '@@1048576', '::/arm-n7000-test.bin'])
                    if data != bytes(index ^ 0xa9 for index in range(256)):
                        raise RuntimeError('the SD write/flush test did not persist the expected bytes')
                    return {'stage': 'smdkc210-' + stage, 'passed': True,
                            'seconds': round(now - started, 2),
                            **({'login_seconds': login_seconds} if login_seconds is not None else {}),
                            **({'desktop_seconds': desktop_seconds, 'autologon': not manual_login} if stage == 'login' else {}),
                            'milestone': {
                                'startup': 'Winlogon, LSASS and 800x1280 FIMD scanout on Cortex-A9',
                                'csr': 'CSRSS server initialization on Cortex-A9',
                                'session': 'paging-file creation and SMSS boot environment on Cortex-A9',
                                'input': 'USB HID and graphical startup on Cortex-A9',
                                'login': 'Explorer desktop on Cortex-A9',
                                'user': 'EDK II, FreeLoader, native SMSS on Cortex-A9'}[stage]}
            raise RuntimeError(f'QEMU exited with status {process.returncode}; inspect {log}')
        finally:
            if not keep and process.poll() is None:
                process.terminate()
                process.wait(timeout=3)


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
                'Cortex-A9 Normal memory unaligned read and alignment trap PASS',
                'translation faults and execute-never PASS',
                'break-before-make and TLB invalidation PASS',
                'self-directed SGI and IRQ return PASS',
                'counter and five comparator interrupts PASS',
                'periodic comparator and monotonic counter PASS',
                'RTC: BCD write, leap-day rollover and restore PASS',
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
    parser.add_argument('--firmware', type=Path,
                        help='boot a --smdkc210 EDK II payload using the existing user test image')
    parser.add_argument('--stage', choices=['user', 'session', 'csr', 'startup', 'input', 'login'], default='user')
    parser.add_argument('--manual-login', action='store_true', help='exercise USB credential entry instead of automatic logon')
    args = parser.parse_args()
    if args.stage != 'user' and not args.firmware:
        parser.error('--stage requires --firmware')
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
            result = (firmware_test(build, lock, args.firmware.resolve(), args.no_build, args.stage, args.manual_login) if args.firmware else
                      test(build, lock, args.no_build))
        except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
            (build / 'arm-uefi-results.json').write_text(json.dumps(
                {'stage': 'smdkc210-kernel' if args.firmware else 'smdkc210-short-descriptors',
                 'passed': False, 'error': str(error)}, indent=2) + '\n')
            parser.exit(1, f'{error}\n')
        (build / 'arm-uefi-results.json').write_text(json.dumps(result, indent=2) + '\n')
        print(f'SMDKC210: PASS ({result["seconds"]}s); {result["milestone"]}')


if __name__ == '__main__':
    main()
