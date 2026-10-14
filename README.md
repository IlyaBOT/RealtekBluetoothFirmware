# RealtekBluetoothFirmware

## Intro

RealtekBluetoothFirmware is a Kernel Extension that uploads Realtek Wireless Bluetooth Firmware to provide native Bluetooth in macOS.
The firmware binary files are from the Linux Open Source Project.

## Experimental RTL8761BU / A728 support on Sequoia

This branch contains experimental **1.0.3d2** support for the Realtek USB adapter
**0BDA:A728** on macOS **Sequoia 15.8.1 (24H32), x86_64**.
The adapter keeps its original USB names so that macOS can select
`THIRD_PARTY_DONGLE`. Firmware-loader USB ownership is released before readiness
is published. Previous USB/HCI synchronization and transfer-error fixes are
included.

Runtime testing confirmed the external-controller path and device discovery.
The tester also confirmed headphone pairing and audio playback. Sleep/wake,
cold starts, reconnects, microphone/SCO audio and other adapter models remain
unverified. This is an experimental build, not a claim of complete macOS
Bluetooth compatibility.

See [A728 Sequoia notes](docs/A728-Sequoia.md) for the changes, test scope,
build instructions and a separate-EFI test procedure. Use only one Realtek
firmware loader. Keep a known-working EFI available for rollback.

## Documentation

**Please read the docs carefully before using the Kernel Extensions or submitting an Issue Report!**

- [Supported Devices](docs/Supported-Devices.md)
- [Installation](docs/Installation.md)

## Credits

- [OpenIntelWireless/IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware)
- [torvalds/linux](https://github.com/torvalds/linux)
- [acidanthera/BrcmPatchRAM](https://github.com/acidanthera/BrcmPatchRAM)
- [bennetzakaria/RTLBluetoothFirmware](https://github.com/bennetzakaria/RTLBluetoothFirmware)
  for the external-controller handoff reference; the transport and firmware
  validation in this branch remain based on thegwchr's implementation.
