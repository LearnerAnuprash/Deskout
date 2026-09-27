# Deskout — Qt/C++ Desktop Wellness & Focus App

You are setting up and building "Deskout," a cross-platform (Windows/macOS/Linux)
native desktop application in C++/Qt6 (Widgets, not QML), using CMake as the build
system. Work in phases — confirm each phase builds and runs before moving to the next.

## Phase 0 — Project setup & shell

- Initialize a CMake project targeting Qt6 Widgets, C++17, with CMAKE_AUTOMOC ON.
- Folder structure: /src, /resources, /ui (if using .ui files), /tests.
- QSystemTrayIcon as the app's persistent home — app runs in the tray, closing the
  main window does not quit the app. Tray menu: Open Deskout, Pause all reminders
  (15 min / 1 hr / until tomorrow), Settings, Quit.
- Auto-start-on-login support (registry on Windows, LaunchAgent on macOS,
  .desktop autostart entry on Linux).
- Global keyboard shortcut (configurable, default e.g. Ctrl+Alt+P) that instantly
  pauses/mutes ALL reminders and alarms until manually resumed — must work even
  when the app window isn't focused (system-wide hotkey).
- QSettings for all persisted config. No backend/network calls — fully local.

## Phase 1 — Reminder engine (core)

Build one generic, reusable "Reminder" system (not 3 hardcoded copies) for:
Eye Break, Drink Water, Walk.

Each reminder type independently supports:

- Custom interval (minutes), on/off toggle
- Custom active days of week (Mon–Sun checkboxes)
- Custom active time window (default 9:00 AM – 6:00 PM, 8-hour standard workday)
- Per-reminder toggle: "Full-screen alarm" vs "Notification only" — a
  "Disable full-screen reminder" button appears on every full-screen alarm
  instance and immediately flips that reminder type to notification-only
- Snooze (5 min / 10 min) alongside a "Confirm / I took the break" button

Full-screen alarm behavior:

- Covers the entire screen (all monitors), alarm-style, cannot be dismissed by
  clicking outside — only via its own buttons.
- Eye Break: after confirming, show one of several built-in eye exercises
  (rotate through a small library — 20-20-20 rule, focus shifting, eye rolls,
  palming) with description/timer.
- Water and Walk: simple confirm + snooze, no exercise content needed.

Suppression logic (apply to ALL reminder/timer alerts app-wide, not just this phase):

- Detect if another application is currently fullscreen (e.g. video call,
  screen share, presentation) — if so, fall back to a system notification
  instead of the full-screen takeover.
- Detect system idle time (no keyboard/mouse input) past a configurable
  threshold (default 5 min) — pause the reminder clock while idle, resume
  counting on input activity.
- Respect the global pause/mute-all state from Phase 0.

## Phase 2 — Reading Mode

- Toggle (tray menu + in-app button): applies a full-screen grayscale
  (zero saturation) filter overlay across all monitors.
- Implement as a lightweight always-on-top overlay per screen with a
  grayscale compositing effect (prefer this over a system-wide display
  setting change, for cross-platform portability).
- Single toggle on/off; persists across restarts if left on.

## Phase 3 — Research Focus Timer

- Countdown timer widget, default 25 minutes, user sets custom duration
  before starting.
- Always-on-top toggle, freely resizable and draggable, plus a compact/mini
  display mode (remaining time only, minimal chrome).
- On completion: alert via the same suppression-aware alert system from
  Phase 1 ("Session complete").
- Log each completed session (start time, duration, completed vs abandoned) —
  feeds Phase 7 stats.

## Phase 4 — Notes (CRUD memo)

- Create, read, update, delete short notes.
- List view (title + preview + last-edited date) + detail/edit view.
- Store in SQLite via Qt SQL module (shared data layer with Topic Docs,
  Phase 5, so Phase 8's cross-search works cleanly).

## Phase 5 — Topic Doc (in-app document editor)

- File-browser-style list view of all "topic documents": name, last-edited
  date, inline or right-click rename.
- Clicking a file opens a rich text editor (QTextEdit/QTextDocument-based):
  bold/italic/underline, headings, bullet lists — lightweight Word/Docs-like
  editing, not full DOCX compatibility.
- Clear "back" button/breadcrumb returns to the file list.
- Auto-save while typing (debounced).
- Internal ID per document, separate from display title, so rename never
  breaks storage/history.

## Phase 6 — Daily Updates / Journal

- Free-text entry for "today" (what the user did, todos for tomorrow).
- On the app's first launch of a NEW calendar day, show yesterday's entry
  first, before the rest of the UI — a "here's what you noted yesterday" screen.
- Show once per new day only, then behave as a normal history view accessible
  anytime from the sidebar. Keep full scrollable history, most recent first.

## Phase 7 — Adherence stats & streaks

- Simple stats view: breaks taken today (by type — eye/water/walk), focus
  sessions completed today, current daily streak (consecutive days meeting
  a configurable minimum adherence threshold).
- Store daily aggregates (not just raw events) so this stays fast as history grows.
- Should be visually clean enough to screenshot for a demo/LinkedIn post.

## Phase 8 — Search across Notes + Topic Docs

- A single search entry point (e.g. Ctrl+K / global search bar) that queries
  titles and content across both Notes and Topic Docs, returns ranked/grouped
  results, and jumps straight to the matching item.

## Phase 9 — Backup & export

- "Export all data" action: bundles Notes, Topic Docs, settings, and stats
  history into a single JSON or zip file the user can save anywhere.
- Corresponding "Import" to restore from that export (at least for reinstall/
  migration purposes, even if conflict handling stays simple in v1).

## Cross-cutting requirements

- Consistent app-wide light/dark theme, Deskout branding/icon placeholder.
- Central Settings dialog reachable from the tray menu, covering: all
  reminder configs, idle threshold, fullscreen-detection toggle, global
  hotkey binding, auto-start toggle, backup/export/import.
- Manual test notes per phase for the logic most likely to be platform-
  specific: interval/day-of-week math, idle detection, fullscreen detection,
  global hotkey registration, auto-start entries.
- After each phase: build, run, and report what works, what's stubbed, and
  what needs platform-specific follow-up.

Start with Phase 0. Confirm the tray icon, auto-start, and global pause
hotkey all work before moving to Phase 1.
