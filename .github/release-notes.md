## Download

| System | File |
| --- | --- |
| Windows 10 and 11 (64-bit) | `Deskout-{version}-windows-x64-setup.exe` |
| macOS (Apple Silicon and Intel) | `Deskout-{version}-macos.dmg` |
| Ubuntu 22.04 or later, Debian | `deskout_{version}_amd64.deb` |
| Other Linux distributions | `Deskout-{version}-x86_64.AppImage` |

## Install

**Windows:** run the installer. The app is not code-signed yet, so SmartScreen may show a warning. Choose **More info**, then **Run anyway**.

**macOS:** open the disk image and drag Deskout into Applications. The app is not notarized by Apple yet, so the first launch is blocked. Open **System Settings > Privacy & Security** and choose **Open Anyway**.

**Ubuntu:** `sudo apt install ./deskout_{version}_amd64.deb`

**AppImage:** `chmod +x Deskout-{version}-x86_64.AppImage`, then run it.

Deskout lives in the system tray. On GNOME it needs the AppIndicator extension, which Ubuntu enables by default.

File checksums are in `SHA256SUMS.txt`.
