#pragma once

// Commands a second `deskout` launch forwards to the running instance.
// Each maps 1:1 to a command-line flag of the same name ("--toggle-pause").
namespace Commands {

inline constexpr char Show[] = "show";
inline constexpr char Settings[] = "settings";
inline constexpr char TogglePause[] = "toggle-pause";
inline constexpr char Pause[] = "pause";
inline constexpr char Resume[] = "resume";
inline constexpr char ToggleReadingMode[] = "toggle-reading-mode";
inline constexpr char ToggleFocus[] = "toggle-focus";
inline constexpr char Quit[] = "quit";

} // namespace Commands
