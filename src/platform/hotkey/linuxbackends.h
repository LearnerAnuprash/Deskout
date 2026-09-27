#pragma once

#include "platform/hotkey/hotkeybackend.h"

// Linux has no single global-hotkey API:
//  - X11 sessions: grab the key on the root window (hotkeybackend_x11.cpp).
//  - GNOME on Wayland: apps can't grab keys, so register a GNOME custom
//    shortcut that runs `deskout --toggle-pause` (hotkeybackend_gnome.cpp).
//  - Anything else: the user binds that command in their desktop settings.
std::unique_ptr<HotkeyBackend> createX11HotkeyBackend(HotkeyBackend::Callback onActivated);
std::unique_ptr<HotkeyBackend> createGnomeHotkeyBackend(HotkeyBackend::Callback onActivated);
std::unique_ptr<HotkeyBackend> createManualHotkeyBackend(HotkeyBackend::Callback onActivated);

// `"<deskout executable>" --toggle-pause`, quoted for a shell-style parser.
QString togglePauseCommandLine();
