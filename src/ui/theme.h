#pragma once

#include <QString>

// App-wide light/dark look: Fusion style + Deskout palette + one stylesheet.
namespace Theme {

enum class Mode { System, Light, Dark };

Mode savedMode();
void saveMode(Mode mode);

// Applies the palette and stylesheet to the whole application.
void apply(Mode mode);
void applySaved();

// Whether the currently applied theme is dark.
bool isDark();

} // namespace Theme
