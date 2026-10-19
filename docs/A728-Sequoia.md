# Experimental A728 support on macOS Sequoia

Version: **1.0.3d2**. Runtime observations: **2026-10-15**.

## Tested configuration

- macOS Sequoia 15.8.1, build 24H32, Darwin 24.6.0, x86_64.
- AMD Ryzen 7 5700X, MSI MAG B550 Tomahawk, OpenCore, SMBIOS MacPro7,1.
- Realtek USB VID:PID **0BDA:A728** (decimal **3034:42792**), USB revision 0200.
- Original USB product name: `Bluetooth 5.4 Radio`.
- Lilu 1.7.3 and BlueToolFixup 2.7.2.

USB revision 0200 is not the HCI revision. Firmware selection continues to use
the existing HCI version/revision/LMP matching table. The loader selected
`rtl8761bu`; the exact BUV marketing variant has not been independently verified.

## Problem and change

Renaming the USB provider to `Bluetooth USB Host Controller` made Sequoia find
the device through its internal-controller path. The stack then selected
`BCM_4350C2`, failed Broadcom Read Verbose Config Version Info, and repeatedly
terminated Bluetooth initialization with OI status 718 and restart reason 108.

Static inspection of the tested bluetoothd identified the corresponding vendor
opcode as **0xFC79** with zero parameter bytes. OI status 718 maps to HCI Invalid
Parameters (0x12); no raw HCI response capture was made. The reported OI status,
HCI opcode and USB IOReturn are separate values. A late USB pipe stall did not
establish the cause of the initial VSC failure.

The A728 personality now sets `RealtekExternalController=true`. For that
personality, both the registry entry name and `USB Product Name` remain as read
from the device. The native stack can then probe the external Bluetooth
interface and select `THIRD_PARTY_DONGLE`. The diagnostic `handoff_mode` property
records the loader's intention; it does not force macOS's chipset enum.

The loader also closes its USB interface/device ownership before publishing
`FirmwareLoaded=true`. Firmware success is not published on failed setup.
Other personalities keep the previous naming behavior. Release-before-ready
applies to all personalities, but has only been runtime-tested on this adapter.

The following USB/HCI fixes are included:

- Guard the interrupt completion predicate with the same IOLock as the waiter.
- On timeout, synchronously abort/drain after unlocking, keeping callback state
  alive and allowing the callback to acquire the lock.
- Propagate completion IOReturn, reject oversized interrupt replies, and leave
  output length zero on failure.
- Reject short control transfers; remove asynchronous clearStall from the
  completion callback while retaining the original initial-stall retry.
- Fail setup when post-download Read Local Version fails.
- Avoid leaking a temporary OSString when publishing the firmware name.

Firmware/config blobs, chipset detection and parser support are unchanged.
No firmware-validation bypass, added sleep, NVRAM code or Realtek vendor reset
was added.

## Runtime evidence and limits

The loaded kext UUID matched the experimental binary below. Read-only registry
and native logs showed:

```text
USB Product Name = Bluetooth 5.4 Radio
fw_name = rtl8761bu
handoff_mode = external
FirmwareLoaded = true
FoundBluetoothInterfaces() ... idVendor = 0x0BDA, idProduct = 0xA728 returned true
setting gUseInternalController to FALSE
Found external Bluetooth dongle -- setting chipset to THIRD_PARTY_DONGLE
Matching services found after 0 seconds
```

System Profiler reported Bluetooth On and `THIRD_PARTY_DONGLE`. Its firmware
fields `v55586 c57286` match the two halves of the embedded container version
`0xDFC6D922`. This is consistent with the patched controller; raw HCI version
packets and upload traffic were not captured.

During an initial observation of about six minutes, the previous HCI/VSC failure
and restart messages were absent, and launchd reported one bluetoothd run with
no exit. The tester saw nearby devices, then confirmed successful headphone
pairing, audio playback and input from the Bluetooth headset microphone.
Playback quality drops while the microphone is in use and returns to normal
when it is released. This matches the two-mode behavior described by
[Apple](https://support.apple.com/en-us/102217). The exact negotiated voice
profile, codec and SCO/eSCO transport were not captured, and microphone quality
was not measured. This functional test does not validate every Bluetooth profile.

The total boot-time improvement was not measured under controlled conditions.
Cold/warm-start behavior, radio power cycling, reconnects, sleep/wake,
long-duration stability and other chipsets still need testing.

## NVRAM observations

The working snapshot used:

| Variable | Observed value |
| --- | --- |
| bluetoothExternalDongleFailed | One-byte CFData `00` |
| bluetoothInternalControllerInfo | 14-byte CFData, all zero in this test |
| bluetoothHostControllerSwitchBehavior | Absent |

These are observations, **not values to write**. No NVRAM changes were made by
the investigation. On the inspected Sequoia implementation, invalid or missing
internal-controller information can prevent external probing. Do not blindly
delete all three variables or fabricate controller address data.

BlueToolFixup 2.7.2 includes vendor/chipset checks relevant to external dongles.
Its `-btlfxnvramcheck` option is read only through Sonoma in that version; adding
the argument should not be assumed to bypass the Sequoia guard. No new boot
arguments or security-policy changes were used for this fix.

## Offline validation and build

```sh
make test
MACOSX_DEPLOYMENT_TARGET=15.5 make \
  SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk \
  BUILD=build-external-sequoia
```

The SDK path above is the SDK used for the tested build. Select an available
compatible Kernel SDK explicitly if it differs on your machine.

Test results:

- 63 firmware/parser checks across 14 firmware files.
- 11 USB interrupt/transfer cases.
- 6 handoff-property cases.
- 4 download-completion cases.
- 3 start/cleanup lifecycle cases.

The latter four harnesses compile extracted real driver methods against mocks
with AddressSanitizer and UndefinedBehaviorSanitizer. These tests validate
specific error paths and ordering, not the full IOKit scheduler or radio stack.
Clang static analysis of the modified handoff implementation reported no
diagnostics. Compilation, linking, plist syntax and bundle structure passed.

The local kextlibs lookup could not resolve the standalone KPI library kexts;
all 325 import names were separately found in the current kernel/KC symbol
tables. Name matches alone are not an ABI/export-policy guarantee. The actual
successful boot validates this build on the tested system, not all macOS SDKs
or other machines.

Tested executable fingerprint:

```text
UUID: DE4D111E-CA0A-33C8-8E49-778E35BA075C
SHA256: 4068a245b0a7812261d03f69be2e9bc2b6eb2b9ed40bfa7fae44ff43b8cb2361
```

The local build is unsigned. A rebuild can have a different UUID/hash. Do not
change security settings merely to force an unaccepted experimental build.

## Manual test and rollback

1. Keep the original working EFI untouched and selectable in the boot picker.
   Use a separate experimental copy on an existing prepared boot medium.
2. Replace only `RealtekBluetoothFirmware.kext` in that copy. Keep Lilu,
   BlueToolFixup, USB mapping, NVRAM and security settings unchanged initially.
3. Enable only one Realtek firmware loader; do not simultaneously install
   RTLBluetoothFirmware or another copy of this loader.
4. Manually boot the experimental EFI. First inspect loaded version, original
   USB names, FirmwareLoaded, external classification and daemon stability.
5. Only after successful HCI initialization, manually test one known device.
   Pairing/radio changes, cold starts and sleep/wake should be separate tests.
6. If the experiment fails, manually choose the untouched working EFI. Switching
   EFI does not undo any pairing or NVRAM changes made separately.

Do not live-load/unload the kext or restart bluetoothd as part of this procedure.

## References

- [thegwchr baseline](https://github.com/thegwchr/RealtekBluetoothFirmware/tree/22e501a8a0a7aba8bb4e594bc6f40b1a67391af9)
- [External-handoff reference](https://github.com/bennetzakaria/RTLBluetoothFirmware/tree/b83e314500be5b88b48cd3516b9c0d46e3e64e2d):
  reports a TP-Link UB500/Tahoe setup, distinct from this A728/Sequoia test.
- [BlueToolFixup 2.7.2](https://github.com/acidanthera/BrcmPatchRAM/blob/2.7.2/BrcmPatchRAM/BlueToolFixup.cpp)
- [OI status definitions](https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/master/embdrv/sbc/decoder/include/oi_status.h)
- [Linux Broadcom vendor-command reference](https://github.com/torvalds/linux/blob/v6.17/drivers/bluetooth/btbcm.c)
