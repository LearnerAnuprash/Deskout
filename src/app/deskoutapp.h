#pragma once

#include "core/pausemanager.h"
#include "platform/hotkey/globalhotkey.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>

class MainWindow;
class SettingsDialog;
class SingleInstance;
class TrayController;

// Owns and wires the app-level components: pause state, tray, main window,
// settings, global hotkey and commands from other `deskout` launches.
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
    void showSettings();
    void quit();
    void applyHotkey();
    void onHotkeyActivated();
    void waitForTray();
    void enableTray();
    void refreshStatus();

    PauseManager m_pause;
    GlobalHotkey m_hotkey;
    std::unique_ptr<MainWindow> m_window;
    std::unique_ptr<TrayController> m_tray;
    QPointer<SettingsDialog> m_settings;
    QTimer m_trayRetry;
    int m_trayRetriesLeft = 0;
};
