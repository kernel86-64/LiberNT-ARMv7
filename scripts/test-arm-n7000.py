#!/usr/bin/env python3
"""Boot the prepared N7000 microSD and recover its RAM log after a watchdog reset."""

import argparse
import fcntl
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]


def wait_for(predicate, timeout=None):
    start = time.monotonic()
    while not predicate():
        if timeout is not None and time.monotonic() - start > timeout:
            raise RuntimeError('N7000 diagnostic wait timed out')
        time.sleep(0.02)


def sd_volume(usb, device):
    listing = device.call(usb.VOLUMES, expect=usb.MAX_READ).decode().lower()
    found = [line for line in listing.splitlines()
             if '223344556602' in line and '/hd(1,mbr,' in line]
    if len(found) == 1 and '223344556601' in listing:
        return int(found[0].split(':', 1)[0])
    return None


def cycle(build, usb, startup):
    log = build / 'arm-boot.log'
    hold = build / 'n7000-hold.nsh'
    hold.write_text('@echo -off\necho N7000 waiting for early-debug payload\n')
    with (build / '.arm-qemu.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        log.write_bytes(b'')
    wait_for(lambda: usb.present(usb.VID, usb.PID))
    device = usb.Dbg()
    try:
        volume = None
        def ready():
            nonlocal volume
            volume = sd_volume(usb, device)
            return volume is not None
        wait_for(ready, 30)
        usb.oslog(device, True)
        usb.push(device, str(startup), r'\startup.nsh', volume)
        device.call(usb.RESET, 0)
    finally:
        device.close()
    wait_for(lambda: not usb.present(usb.VID, usb.PID), 10)
    capture = subprocess.Popen([
        sys.executable, str(ROOT / 'scripts/n7000-usblog.py'), '--build-dir', str(build),
        '--until', 'Attempting to exit bootsevices'], stdout=subprocess.DEVNULL)
    try:
        def marker_ready(marker):
            if capture.poll() is not None:
                raise RuntimeError('firmware capture ended before the watchdog was armed')
            return marker in log.read_bytes()
        wait_for(lambda: marker_ready(b'Starting LiberNT FreeLoader'), 90)
        device = usb.Dbg()
        try:
            for address, value in [(0x10060000, 0), (0x10060004, 65535),
                                   (0x10060008, 65535), (0x10060000, 0xff39)]:
                device.call(usb.WRITE32, address, value)
            usb.push(device, str(hold), r'\startup.nsh', volume)
        finally:
            device.close()
        print('Watchdog armed; waiting for EBS and automatic restart.', flush=True)
        if capture.wait(timeout=25):
            print('Firmware USB stream ended; recovering the RAM log after reset.', flush=True)
    finally:
        if capture.poll() is None:
            capture.terminate()
            capture.wait(timeout=5)
    wait_for(lambda: not usb.present(usb.VID, usb.PID), 10)
    wait_for(lambda: usb.present(usb.VID, usb.PID), 90)
    device = usb.Dbg()
    try:
        print('Reset reason:', device.read(0x10020404, 4).hex(), flush=True)
        magic, head = struct.unpack('<II', device.read(usb.OS_LOG_BASE, 8))
        volume = None
        wait_for(ready, 30)
        usb.push(device, str(hold), r'\startup.nsh', volume)
        if magic != usb.OS_LOG_MAGIC:
            raise RuntimeError(f'OS log header was not preserved: {magic:08x}, head={head}')
        size = usb.OS_LOG_SIZE - 16
        count = min(head, size)
        start = (head - count) % size
        data = bytearray()
        while len(data) < count:
            position = (start + len(data)) % size
            length = min(usb.MAX_READ, count - len(data), size - position)
            data += device.read(usb.OS_LOG_BASE + 16 + position, length)
        with (build / '.arm-qemu.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            log.write_bytes(data)
        print(data.decode('ascii', 'replace'), flush=True)
    finally:
        device.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    parser.add_argument('--usbdbg', type=Path, default=ROOT.parent / 'N7000PXEDebug/usbdbg.py')
    parser.add_argument('--startup', type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    startup = args.startup or build / 'n7000-startup-original.nsh'
    if not args.usbdbg.is_file() or not startup.is_file():
        parser.error('the USB debugger module and prepared startup script are required')
    spec = importlib.util.spec_from_file_location('n7000_usbdbg', args.usbdbg)
    usb = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(usb)
    try:
        cycle(build, usb, startup)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
