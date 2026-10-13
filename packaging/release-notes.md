## Install

| File | For |
|---|---|
| **Tonal-{VERSION}-setup.exe** | Installer. It needs no administrator rights, adds a Start menu shortcut and shows up in "Installed apps". |
| **Tonal-{VERSION}-win64.zip** | Portable version: unzip it into a folder and run `Tonal.exe`. |

**Requirements:** Windows 10 (1809) or 11, 64-bit, and a **Spotify Premium** account. On first run, Tonal walks
you through creating a free app in the Spotify developer dashboard (about 2 minutes). The interface is in
Brazilian Portuguese.

The executable is not code-signed: SmartScreen may show "Windows protected your PC". Click
**More info → Run anyway**. SHA-256 hashes are in `SHA256SUMS.txt`.

## Includes

- Qt {QT} (LGPLv3) and the Visual C++ runtime
- librespot {LIBRESPOT} (MIT), the audio engine
- Licenses in the `licenses` folder

No need to install Qt, Visual Studio or Rust.
