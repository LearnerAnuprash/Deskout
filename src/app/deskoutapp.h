#pragma once

#include "app/readingmode.h"
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

class MainWindow;
class ReminderAlertController;
class SingleInstance;
class TrayController;

// Owns and wires the app-level components: pause state, reminders, tray,
// main window, settings, global hotkey and commands from other `deskout`
// launches.
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
    QWidget *dialogParent() const;

    // Declaration order matters: the engine's hooks read pause and activity.
    PauseManager m_pause;
    ActivityMonitor m_activity;
    ReminderEngine m_reminders;
    GlobalHotkey m_hotkey;
    Notifier m_notifier;
    ReadingMode m_readingMode;
    std::unique_ptr<ReminderAlertController> m_alerts;
    std::unique_ptr<MainWindow> m_window;
    std::unique_ptr<TrayController> m_tray;
    QPointer<SettingsDialog> m_settings;
    QTimer m_trayRetry;
    int m_trayRetriesLeft = 0;
};
