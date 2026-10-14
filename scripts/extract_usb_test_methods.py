"""Compile the production interrupt/control methods against deterministic USB mocks.

No copied implementation: extract these methods verbatim so regressions in the
driver are exercised by the host tests. This does not simulate the entire KPI.
"""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text()
if 'USBDeviceController' in pathlib.Path(sys.argv[1]).name:
    start = source.index('void USBDeviceController::\ninterruptHandler(')
    end = source.index('IOReturn USBDeviceController::\nbulkWrite(', start)
elif pathlib.Path(sys.argv[1]).name == 'BtRtl.cpp':
    start = source.index('bool BtRtl::\ndownloadFirmware(')
    end = source.index('OSData *BtRtl::\nfirmwareConvertion(', start)
elif len(sys.argv) > 3 and sys.argv[3] == '--lifecycle':
    start = source.index('bool RealtekBluetoothFirmware::start(')
    end = source.index('IOReturn RealtekBluetoothFirmware::setPowerState(', start)
else:
    start = source.index('void RealtekBluetoothFirmware::publishReg(')
    end = source.index('void RealtekBluetoothFirmware::cleanUp(', start)
pathlib.Path(sys.argv[2]).write_text(source[start:end])
