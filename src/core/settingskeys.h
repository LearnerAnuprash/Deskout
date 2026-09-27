#pragma once

// Central list of QSettings keys so every component reads and writes the
// same names. Grouped by prefix ("group/key").
namespace SettingsKeys {

inline constexpr char PausePaused[] = "pause/paused";
inline constexpr char PauseUntil[] = "pause/until";

inline constexpr char HotkeyEnabled[] = "hotkey/enabled";
inline constexpr char HotkeySequence[] = "hotkey/sequence";
inline constexpr char HotkeyDefaultSequence[] = "Ctrl+Alt+P";

// Per-reminder config lives under "reminders/<id>/..." (see core/reminders.cpp).
inline constexpr char EyeExerciseNext[] = "reminders/eye/nextExercise";

inline constexpr char IdleDetectionEnabled[] = "detection/idleEnabled";
inline constexpr char IdleThresholdMinutes[] = "detection/idleThresholdMinutes";
inline constexpr int IdleThresholdDefault = 5;
inline constexpr char FullscreenDetectionEnabled[] = "detection/fullscreenEnabled";

inline constexpr char ReadingModeEnabled[] = "readingMode/enabled";

inline constexpr char UiTheme[] = "ui/theme";
inline constexpr char UiTrayHintShown[] = "ui/trayHintShown";
inline constexpr char UiWindowGeometry[] = "ui/windowGeometry";

} // namespace SettingsKeys
