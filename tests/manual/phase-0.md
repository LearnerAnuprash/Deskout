# Phase 0 manual tests: tray, auto-start, global pause hotkey

Automated tests cover pause timing, key-name conversion and the auto-start
file contents (`ctest --test-dir build`). The checks below need a real desktop
session because they depend on the platform.

## Prerequisites (Linux / GNOME)

- The tray icon needs a StatusNotifier host. On Ubuntu GNOME, enable the
  **Ubuntu AppIndicators** extension:
  `gnome-extensions enable ubuntu-appindicators@ubuntu.com`
- Without a tray host, Deskout still runs. The Home page shows
  "Tray icon: Unavailable", and closing the window quits the app.

## 1. Tray and window lifecycle

1. Run `./build/deskout`. The main window opens and the tray icon appears.
2. Close the window. Deskout keeps running. The first time, a notification
   says "Deskout is still running".
3. Tray menu > **Open Deskout** brings the window back.
4. Run `./build/deskout` a second time. No second process starts; the
   existing window comes to the front.
5. Tray menu > **Quit Deskout** exits. `pgrep deskout` returns nothing.

## 2. Pause menu

1. Tray > Pause all reminders > **For 15 minutes**. The tray icon turns grey
   with a pause badge. The first menu line reads "Paused until HH:MM". The
   window shows an amber banner.
2. Click **Resume** in the banner. The icon, menu and banner return to active.
3. Pause **Until tomorrow**, quit, and start again. The app is still paused
   (the state persists).
4. Pause **For 15 minutes**, quit, and change the pause end time in
   `~/.config/Deskout/Deskout.conf` to a past time. Start again: the app is active.

## 3. Global pause hotkey (default Ctrl+Alt+P)

Check which backend is in use on the Home page ("Pause shortcut" row).

| Session | Backend | What to check |
|---|---|---|
| GNOME Wayland | GNOME custom shortcut | GNOME Settings > Keyboard > View and Customize Shortcuts > Custom Shortcuts lists "Deskout: pause/resume all reminders" |
| X11 (any desktop) | X11 key grab | Works with Caps Lock and Num Lock on; holding the keys toggles only once |
| Other Wayland (KDE, Sway...) | Manual | Settings shows the command to bind by hand |
| Windows | RegisterHotKey | A shortcut already taken by another app shows an error in Settings |
| macOS | Carbon hotkey | Ctrl in Qt means Cmd: the default is Cmd+Option+P |

1. Focus a different app (browser, terminal). Press **Ctrl+Alt+P**. A
   "Reminders paused" notification appears and the tray icon turns grey.
2. Press it again. A "Reminders resumed" notification appears.
3. Settings > change the shortcut to **Ctrl+Alt+O** > Apply. The status line
   shows "Active: Ctrl+Alt+O". The old shortcut no longer works; the new one does.
4. Try **P** alone or **Shift+P**. Settings rejects it (a modifier is required).
5. Uncheck "Enable system-wide shortcut" > Apply. The shortcut does nothing.
   On GNOME, the custom shortcut entry disappears.
6. Quit Deskout. On GNOME, the custom shortcut entry is removed.
7. CLI equivalents: `deskout --toggle-pause`, `--pause`, `--resume`,
   `--show`, `--settings`, `--quit`. With no instance running, the pause
   and quit commands print "Deskout is not running." and exit.

Force a backend for testing: `DESKOUT_HOTKEY_BACKEND=x11|gnome|manual ./build/deskout`.

## 4. Launch at login

1. Settings > check "Start Deskout automatically when I log in" > OK.
   - Linux: `~/.config/autostart/deskout.desktop` exists, and its
     `Exec="<path>/deskout" --minimized` line points at the current binary.
   - Windows: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Deskout`
   - macOS: `~/Library/LaunchAgents/app.deskout.Deskout.plist`
2. Log out and back in. Deskout starts hidden (tray icon only).
3. Uncheck the option. The entry is removed.
4. Move or rebuild the binary somewhere else and start it once. The entry is
   rewritten with the new path.

## Known platform limits

- **GNOME Wayland:** apps cannot grab keys directly. Deskout registers a GNOME
  custom shortcut that runs `deskout --toggle-pause`. GNOME does not report
  conflicts: if another shortcut already uses the same keys, GNOME decides
  which one wins.
- **Windows and macOS backends** are written but not yet compiled or tested
  on those systems.
