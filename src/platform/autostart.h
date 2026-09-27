#pragma once

#include <QString>

// Launch-at-login support. The OS entry itself is the source of truth:
//   Windows: HKCU\Software\Microsoft\Windows\CurrentVersion\Run value
//   macOS:   ~/Library/LaunchAgents/app.deskout.Deskout.plist
//   Linux:   ~/.config/autostart/deskout.desktop (XDG autostart spec)
namespace AutoStart {

inline constexpr char StartMinimizedArg[] = "--minimized";

bool isSupported();
bool isEnabled();
bool setEnabled(bool enabled, QString *error = nullptr);
// If enabled, rewrite the entry so it points at the current executable
// (the binary may have moved since the entry was created).
void refreshIfEnabled();

// Path of the executable the OS should launch (AppImage aware on Linux).
QString executablePath();

// Pure builders, exposed for unit tests.
QString desktopEntry(const QString &executable, const QString &iconPath);
QString launchAgentPlist(const QString &executable);
QString windowsRunCommand(const QString &executable);

} // namespace AutoStart
