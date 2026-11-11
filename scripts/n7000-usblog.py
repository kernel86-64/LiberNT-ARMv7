#!/usr/bin/env python3
"""Capture the Exynos441xPkg UsbLogDxe stream without debug commands."""

import argparse
import ctypes
import fcntl
from pathlib import Path
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
COMPLETE = b'ARM platform probe: COMPLETE'


class DeviceDescriptor(ctypes.Structure):
    _fields_ = [
        ('bLength', ctypes.c_uint8), ('bDescriptorType', ctypes.c_uint8),
        ('bcdUSB', ctypes.c_uint16), ('bDeviceClass', ctypes.c_uint8),
        ('bDeviceSubClass', ctypes.c_uint8), ('bDeviceProtocol', ctypes.c_uint8),
        ('bMaxPacketSize0', ctypes.c_uint8), ('idVendor', ctypes.c_uint16),
        ('idProduct', ctypes.c_uint16), ('bcdDevice', ctypes.c_uint16),
        ('iManufacturer', ctypes.c_uint8), ('iProduct', ctypes.c_uint8),
        ('iSerialNumber', ctypes.c_uint8), ('bNumConfigurations', ctypes.c_uint8)]


def usb_library():
    lib = ctypes.CDLL('libusb-1.0.so.0')
    lib.libusb_init.argtypes = [ctypes.c_void_p]
    lib.libusb_exit.argtypes = [ctypes.c_void_p]
    lib.libusb_exit.restype = None
    lib.libusb_get_device_list.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))]
    lib.libusb_get_device_list.restype = ctypes.c_ssize_t
    lib.libusb_free_device_list.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int]
    lib.libusb_free_device_list.restype = None
    lib.libusb_get_device_descriptor.argtypes = [ctypes.c_void_p, ctypes.POINTER(DeviceDescriptor)]
    lib.libusb_open.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    lib.libusb_get_bus_number.argtypes = [ctypes.c_void_p]
    lib.libusb_get_bus_number.restype = ctypes.c_uint8
    lib.libusb_get_device_address.argtypes = [ctypes.c_void_p]
    lib.libusb_get_device_address.restype = ctypes.c_uint8
    lib.libusb_error_name.argtypes = [ctypes.c_int]
    lib.libusb_error_name.restype = ctypes.c_char_p
    lib.libusb_claim_interface.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.libusb_release_interface.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.libusb_close.argtypes = [ctypes.c_void_p]
    lib.libusb_close.restype = None
    lib.libusb_bulk_transfer.argtypes = [ctypes.c_void_p, ctypes.c_ubyte, ctypes.c_void_p,
                                       ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.c_uint]
    return lib


def open_device(lib):
    devices = ctypes.POINTER(ctypes.c_void_p)()
    count = lib.libusb_get_device_list(None, ctypes.byref(devices))
    if count < 0:
        raise RuntimeError(f'cannot enumerate USB devices: {lib.libusb_error_name(count).decode()}')
    try:
        for index in range(count):
            descriptor = DeviceDescriptor()
            result = lib.libusb_get_device_descriptor(devices[index], ctypes.byref(descriptor))
            if result or (descriptor.idVendor, descriptor.idProduct) != (0x1209, 0x0001):
                continue
            handle = ctypes.c_void_p()
            result = lib.libusb_open(devices[index], ctypes.byref(handle))
            if result == -4:
                continue
            if result:
                path = (f'/dev/bus/usb/{lib.libusb_get_bus_number(devices[index]):03}/'
                        f'{lib.libusb_get_device_address(devices[index]):03}')
                detail = ('; device exists but USB access is denied; grant this user read/write '
                          'access to the device or run the receiver with sudo') if result == -3 else ''
                raise RuntimeError(f'cannot open UsbLogDxe 1209:0001 at {path}: '
                                   f'{lib.libusb_error_name(result).decode()}{detail}')
            return handle.value
    finally:
        lib.libusb_free_device_list(devices, 1)
    return None


def capture(build, complete=COMPLETE):
    lib = usb_library()
    with (build / '.arm-qemu.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('the ARM boot log is in use by another test')
        if lib.libusb_init(None):
            raise RuntimeError('libusb initialization failed')
        device = None
        claimed = False
        received = b''
        buffer = ctypes.create_string_buffer(16384)
        log = build / 'arm-boot.log'
        print(f'Waiting for UsbLogDxe 1209:0001, interface 0; log: {log}', file=sys.stderr)
        try:
            with log.open('wb', buffering=0) as output:
                while True:
                    if not device:
                        device = open_device(lib)
                        if not device:
                            time.sleep(0.1)
                            continue
                        result = lib.libusb_claim_interface(device, 0)
                        if result:
                            raise RuntimeError('cannot claim UsbLogDxe interface 0: '
                                               f'{lib.libusb_error_name(result).decode()}')
                        claimed = True
                        print('Connected to UsbLogDxe 1209:0001, interface 0.', file=sys.stderr)
                    transferred = ctypes.c_int()
                    result = lib.libusb_bulk_transfer(device, 0x81, buffer, len(buffer),
                                                      ctypes.byref(transferred), 100)
                    if not 0 <= transferred.value <= len(buffer):
                        raise RuntimeError('invalid USB transfer length')
                    if transferred.value:
                        data = buffer.raw[:transferred.value]
                        output.write(data)
                        sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                        received = (received + data)[-len(complete) - len(buffer):]
                    if result not in (0, -7):
                        raise RuntimeError(f'UsbLogDxe stream ended: libusb status {result}')
                    if complete in received:
                        print('\nFirmware capture complete; kernel startup was not tested.', file=sys.stderr)
                        return
        finally:
            if device:
                if claimed:
                    lib.libusb_release_interface(device, 0)
                lib.libusb_close(device)
            lib.libusb_exit(None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'output-Clang-arm-debug')
    parser.add_argument('--until', default=COMPLETE.decode(), help='stop after this text appears in the log')
    args = parser.parse_args()
    if not args.until:
        parser.error('--until must not be empty')
    try:
        capture(args.build_dir.resolve(), args.until.encode())
    except KeyboardInterrupt:
        parser.exit(130, '\nStopped.\n')
    except (OSError, RuntimeError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
