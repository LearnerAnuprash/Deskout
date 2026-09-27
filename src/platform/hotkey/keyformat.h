#pragma once

#include <QKeyCombination>
#include <QString>

// Pure helpers for turning a Qt key combination into the names the native
// hotkey backends understand. No platform headers here, so it is unit tested.
namespace KeyFormat {

// X11 keysym name of the key part ("p", "F5", "space"...). The same names are
// used by GTK accelerators. Empty when the key is not supported.
QString keysymName(Qt::Key key);

// GTK accelerator for GNOME custom shortcuts, e.g. "<Control><Alt>p".
// Empty when the key is not supported.
QString gtkAccelerator(QKeyCombination combo);

// A global shortcut needs Ctrl, Alt or Meta plus a supported key, otherwise
// it would swallow normal typing system-wide.
bool isUsableGlobalShortcut(QKeyCombination combo, QString *reason = nullptr);

} // namespace KeyFormat
