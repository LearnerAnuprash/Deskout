#pragma once

#include "app/readingmode.h"
#include "core/focuslog.h"
#include "core/focustimer.h"
#include "core/notesstore.h"
#include "core/pausemanager.h"
#include "core/reminderengine.h"
#include "platform/activity/activitymonitor.h"
#include "platform/hotkey/globalhotkey.h"
#include "ui/notifier.h"
#include "ui/settingsdialog.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>

class AlertCenter;
class FocusAlertController;
class FocusTimerWindow;
class MainWindow;
class ReminderAlertController;
class SingleInstance;
class TrayController;

// Owns and wires the app-level components: pause state, reminders, focus
// timer, notes, tray, main window, settings, global hotkey and commands
// from other `deskout` launches.
class DeskoutApp : public QObject
{
    Q_OBJECT

public:
    explicit DeskoutApp(SingleInstance *instance, QObject *parent = nullptr);
    ~DeskoutApp() override;

    void start(bool minimized);
    void handleCommand(const QString &command);

private:
    void showMainWindow();
    void showSettings(SettingsDialog::Tab tab = SettingsDialog::Tab::General);
    void quit();
    void applyHotkey();
    void onHotkeyActivated();
    void waitForTray();
    void enableTray();
    void refreshStatus();
    void setReadingMode(bool on);
    void syncReadingModeUi();
    void showFocusWindow();
    void toggleFocus();
    QWidget *dialogParent() const;

    // Declaration order matters: the engine's hooks read pause and activity.
    PauseManager m_pause;
    ActivityMonitor m_activity;
    ReminderEngine m_reminders;
    GlobalHotkey m_hotkey;
    Notifier m_notifier;
    ReadingMode m_readingMode;
    FocusTimer m_focus;
    FocusLog m_focusLog;
    NotesStore m_notes;
    std::unique_ptr<AlertCenter> m_alertCenter;
    std::unique_ptr<ReminderAlertController> m_reminderAlerts;
    std::unique_ptr<FocusAlertController> m_focusAlerts;
    std::unique_ptr<MainWindow> m_window;
    std::unique_ptr<FocusTimerWindow> m_focusWindow;
    std::unique_ptr<TrayController> m_tray;
    QPointer<SettingsDialog> m_settings;
    QTimer m_trayRetry;
    int m_trayRetriesLeft = 0;
};
