#pragma once

#include <QIcon>
#include <QString>

// Deskout branding (placeholder artwork in resources/icons).
namespace AppIcon {

// Rendered from SVG at common tray/window sizes, so it does not depend on
// the Qt SVG icon-engine plugin being deployed.
QIcon icon(bool paused = false);

// Writes a 256px PNG copy to the app data dir (for .desktop Icon= entries)
// and returns its path, or an empty string on failure.
QString exportPng();

} // namespace AppIcon
