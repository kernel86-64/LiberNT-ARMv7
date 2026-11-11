#!/usr/bin/env python3
"""Host tests for the N7000 firmware log receiver."""

import contextlib
import ctypes
import importlib.util
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[4]
SPEC = importlib.util.spec_from_file_location('n7000_usblog', ROOT / 'scripts/n7000-usblog.py')
RECEIVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RECEIVER)


class ReceiverTests(unittest.TestCase):
    def library(self, transfers, claim=0):
        def transfer(device, endpoint, buffer, capacity, transferred, timeout):
            self.assertEqual(endpoint, 0x81)
            data, status = transfers.pop(0)
            self.assertLessEqual(len(data), capacity)
            ctypes.memmove(buffer, data, len(data))
            transferred._obj.value = len(data)
            return status

        return SimpleNamespace(
            libusb_init=Mock(return_value=0), libusb_exit=Mock(),
            open_result=Mock(return_value=1),
            libusb_error_name=Mock(return_value=b'LIBUSB_ERROR_BUSY'),
            libusb_claim_interface=Mock(return_value=claim),
            libusb_release_interface=Mock(), libusb_close=Mock(),
            libusb_bulk_transfer=Mock(side_effect=transfer))

    def receive(self, lib, build, complete=RECEIVER.COMPLETE):
        with patch.object(RECEIVER, 'usb_library', return_value=lib), \
                patch.object(RECEIVER, 'open_device', side_effect=lib.open_result), \
                patch.object(RECEIVER.sys, 'stdout', SimpleNamespace(buffer=io.BytesIO())), \
                contextlib.redirect_stderr(io.StringIO()):
            RECEIVER.capture(build, complete)

    def test_custom_fragmented_completion_marker(self):
        lib = self.library([(b'Firmware performance: COM', 0), (b'PLETE', 0)])
        with tempfile.TemporaryDirectory() as directory:
            self.receive(lib, Path(directory), b'Firmware performance: COMPLETE')
        lib.libusb_close.assert_called_once_with(1)

    def test_device_appears_after_more_than_a_minute(self):
        lib = self.library([(RECEIVER.COMPLETE, 0)])
        lib.open_result.side_effect = [None, 1]
        clock = [0]

        def sleep(delay):
            clock[0] += 90

        with tempfile.TemporaryDirectory() as directory, \
                patch.object(RECEIVER.time, 'monotonic', side_effect=lambda: clock[0]), \
                patch.object(RECEIVER.time, 'sleep', side_effect=sleep):
            self.receive(lib, Path(directory))
        self.assertEqual(clock[0], 90)
        self.assertEqual(lib.open_result.call_count, 2)
        lib.libusb_close.assert_called_once_with(1)

    def test_keyboard_interrupt_releases_resources(self):
        lib = self.library([])
        lib.libusb_bulk_transfer.side_effect = KeyboardInterrupt
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(KeyboardInterrupt):
                self.receive(lib, Path(directory))
        lib.libusb_release_interface.assert_called_once_with(1, 0)
        lib.libusb_close.assert_called_once_with(1)
        lib.libusb_exit.assert_called_once_with(None)

    def test_fragmented_marker_and_partial_timeout_transfer(self):
        chunks = [(b'boot\x00\r\nARM platform pro', 0),
                  (b'be: COM', -7), (b'PLETE\r\n', 0)]
        expected = b''.join(data for data, status in chunks)
        lib = self.library(chunks)
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / 'arm-boot.log').write_bytes(b'old log must be replaced')
            self.receive(lib, build)
            self.assertEqual((build / 'arm-boot.log').read_bytes(), expected)
        lib.libusb_claim_interface.assert_called_once_with(1, 0)
        lib.libusb_release_interface.assert_called_once_with(1, 0)
        lib.libusb_close.assert_called_once_with(1)
        lib.libusb_exit.assert_called_once_with(None)

    def test_disconnect_preserves_log_and_releases_resources(self):
        lib = self.library([(b'last readable bytes', -4)])
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            with self.assertRaisesRegex(RuntimeError, 'stream ended'):
                self.receive(lib, build)
            self.assertEqual((build / 'arm-boot.log').read_bytes(), b'last readable bytes')
        lib.libusb_release_interface.assert_called_once_with(1, 0)
        lib.libusb_close.assert_called_once_with(1)
        lib.libusb_exit.assert_called_once_with(None)

    def test_failed_claim_closes_device_without_releasing_interface(self):
        lib = self.library([], claim=-6)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, 'cannot claim'):
                self.receive(lib, Path(directory))
        lib.libusb_bulk_transfer.assert_not_called()
        lib.libusb_release_interface.assert_not_called()
        lib.libusb_close.assert_called_once_with(1)
        lib.libusb_exit.assert_called_once_with(None)


class OpenDeviceTests(unittest.TestCase):
    def library(self, vendor=0x1209, product=0x0001, status=0):
        devices = (ctypes.c_void_p * 1)(42)

        def get_list(context, output):
            ctypes.cast(output, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))[0] = devices
            return 1

        def describe(device, output):
            descriptor = output._obj
            descriptor.idVendor, descriptor.idProduct = vendor, product
            return 0

        def open_handle(device, output):
            if not status:
                output._obj.value = 123
            return status

        return SimpleNamespace(
            libusb_get_device_list=Mock(side_effect=get_list),
            libusb_get_device_descriptor=Mock(side_effect=describe),
            libusb_open=Mock(side_effect=open_handle),
            libusb_get_bus_number=Mock(return_value=5),
            libusb_get_device_address=Mock(return_value=19),
            libusb_error_name=Mock(return_value=b'LIBUSB_ERROR_ACCESS'),
            libusb_free_device_list=Mock())

    def test_access_denied_is_reported_instead_of_waiting(self):
        lib = self.library(status=-3)
        with self.assertRaisesRegex(RuntimeError, '/dev/bus/usb/005/019: LIBUSB_ERROR_ACCESS.*denied'):
            RECEIVER.open_device(lib)
        lib.libusb_free_device_list.assert_called_once()

    def test_matching_device_returns_handle(self):
        lib = self.library()
        self.assertEqual(RECEIVER.open_device(lib), 123)
        lib.libusb_free_device_list.assert_called_once()

    def test_unrelated_device_is_not_opened(self):
        lib = self.library(product=2)
        self.assertIsNone(RECEIVER.open_device(lib))
        lib.libusb_open.assert_not_called()
        lib.libusb_free_device_list.assert_called_once()

    def test_device_disappearing_during_open_can_be_retried(self):
        lib = self.library(status=-4)
        self.assertIsNone(RECEIVER.open_device(lib))
        lib.libusb_free_device_list.assert_called_once()


if __name__ == '__main__':
    unittest.main()
