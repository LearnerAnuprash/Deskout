#include "app/deskoutapp.h"

#include "app/reminderalerts.h"
#include "core/commands.h"
#include "core/settingskeys.h"
#include "core/singleinstance.h"
#include "platform/autostart.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"
#include "ui/traycontroller.h"

#include <QApplication>
#include <QSettings>
#include <QSystemTrayIcon>

namespace {
// At login the panel may register its tray host a few seconds after we start.
constexpr int TrayRetryIntervalMs = 1000;
constexpr int TrayRetryCount = 20;
} // namespace

DeskoutApp::DeskoutApp(SingleInstance *instance, QObject *parent)
    : QObject(parent)
    , m_reminders({[this] { return m_pause.isPaused(); },
                   [this] { return m_activity.isIdle(); },
                   {}})
{
    connect(instance, &SingleInstance::commandReceived, this, &DeskoutApp::handleCommand);
    connect(&m_hotkey, &GlobalHotkey::activated, this, &DeskoutApp::onHotkeyActivated);
    connect(&m_pause, &PauseManager::pausedChanged, this, &DeskoutApp::refreshStatus);
    connect(&m_trayRetry, &QTimer::timeout, this, &DeskoutApp::waitForTray);
}

DeskoutApp::~DeskoutApp()
{
    // The dialog may be parentless (opened from the tray), so it is not
    // cleaned up with the main window.
    delete m_settings;
}

void DeskoutApp::start(bool minimized)
{
    Theme::applySaved();
    AutoStart::refreshIfEnabled();

    m_reminders.start();
    m_alerts = std::make_unique<ReminderAlertController>(&m_reminders, &m_activity, &m_notifier, &m_pause);

    m_window = std::make_unique<MainWindow>(&m_pause, &m_reminders);
    connect(m_window.get(), &MainWindow::settingsRequested, this, [this] { showSettings(); });
    connect(m_window.get(), &MainWindow::reminderSettingsRequested, this,
            [this] { showSettings(SettingsDialog::Tab::Reminders); });
    connect(m_window.get(), &MainWindow::hiddenToTray, this, [this] {
        QSettings settings;
        if (!settings.value(SettingsKeys::UiTrayHintShown, false).toBool()) {
            m_notifier.notify(tr("Deskout is still running"),
                              tr("Reminders keep working in the background. Use the tray icon to "
                                 "reopen Deskout or quit."));
            settings.setValue(SettingsKeys::UiTrayHintShown, true);
        }
    });

    applyHotkey();

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        enableTray();
    } else {
        // No tray (yet): the window must stay reachable, so closing it quits.
        m_window->setHideOnClose(false);
        QApplication::setQuitOnLastWindowClosed(true);
        m_trayRetriesLeft = TrayRetryCount;
        m_trayRetry.start(TrayRetryIntervalMs);
        minimized = false;
    }

    refreshStatus();
    if (!minimized)
        showMainWindow();
}

void DeskoutApp::handleCommand(const QString &command)
{
    if (command == QLatin1String(Commands::Show))
        showMainWindow();
    else if (command == QLatin1String(Commands::Settings))
        showSettings();
    else if (command == QLatin1String(Commands::TogglePause))
        onHotkeyActivated();
    else if (command == QLatin1String(Commands::Pause))
        m_pause.pauseIndefinitely();
    else if (command == QLatin1String(Commands::Resume))
        m_pause.resume();
    else if (command == QLatin1String(Commands::Quit))
        quit();
    else
        qWarning("Deskout: unknown command \"%s\"", qPrintable(command));
}

void DeskoutApp::showMainWindow()
{
    m_window->show();
    m_window->setWindowState((m_window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    m_window->raise();
    m_window->activateWindow();
}

void DeskoutApp::showSettings(SettingsDialog::Tab tab)
{
    if (!m_settings) {
        // Parent to the main window only while it is visible, otherwise the
        // dialog would be hidden along with it.
        const SettingsDialog::Context context{&m_hotkey, &m_reminders, &m_activity};
        m_settings = new SettingsDialog(context, m_window->isVisible() ? m_window.get() : nullptr);
        m_settings->setAttribute(Qt::WA_DeleteOnClose);
        m_settings->setWindowIcon(m_window->windowIcon());
        connect(m_settings, &SettingsDialog::applied, this, [this] {
            Theme::applySaved();
            applyHotkey();
            m_activity.reload();
            refreshStatus();
        });
    }
    m_settings->showTab(tab);
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void DeskoutApp::quit()
{
    if (m_window && m_window->isVisible())
        QSettings().setValue(SettingsKeys::UiWindowGeometry, m_window->saveGeometry());
    m_hotkey.clear();
    QApplication::quit();
}

void DeskoutApp::applyHotkey()
{
    QSettings settings;
    if (!settings.value(SettingsKeys::HotkeyEnabled, true).toBool()) {
        m_hotkey.clear();
        return;
    }
    const QKeySequence sequence(
        settings.value(SettingsKeys::HotkeySequence, QLatin1String(SettingsKeys::HotkeyDefaultSequence))
            .toString(),
        QKeySequence::PortableText);
    if (!m_hotkey.setShortcut(sequence))
        qWarning("Deskout: global shortcut not registered: %s", qPrintable(m_hotkey.lastError()));
}

void DeskoutApp::onHotkeyActivated()
{
    m_pause.toggle();
    const QString shortcut = m_hotkey.isRegistered()
                                 ? m_hotkey.shortcut().toString(QKeySequence::NativeText)
                                 : QString();
    if (m_pause.isPaused())
        m_notifier.notify(tr("Reminders paused"),
                          shortcut.isEmpty() ? tr("Everything is muted until you resume.")
                                             : tr("Everything is muted. Press %1 to resume.").arg(shortcut));
    else
        m_notifier.notify(tr("Reminders resumed"), tr("Deskout is active again."));
}

void DeskoutApp::waitForTray()
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayRetry.stop();
        enableTray();
        refreshStatus();
    } else if (--m_trayRetriesLeft <= 0) {
        m_trayRetry.stop();
        qWarning("Deskout: no system tray found; running with the main window only.");
        refreshStatus();
    }
}

void DeskoutApp::enableTray()
{
    m_tray = std::make_unique<TrayController>(&m_pause);
    connect(m_tray.get(), &TrayController::openRequested, this, &DeskoutApp::showMainWindow);
    connect(m_tray.get(), &TrayController::settingsRequested, this, [this] { showSettings(); });
    connect(m_tray.get(), &TrayController::quitRequested, this, &DeskoutApp::quit);
    m_tray->show();
    m_notifier.setTray(m_tray.get());
    m_window->setHideOnClose(true);
    QApplication::setQuitOnLastWindowClosed(false);
}

void DeskoutApp::refreshStatus()
{
    if (!m_window)
        return;
    using Row = MainWindow::StatusRow;

    if (!QSettings().value(SettingsKeys::HotkeyEnabled, true).toBool())
        m_window->setStatus(Row::Hotkey, tr("Disabled in Settings"));
    else if (m_hotkey.isRegistered())
        m_window->setStatus(Row::Hotkey, tr("%1 (%2)").arg(m_hotkey.shortcut().toString(QKeySequence::NativeText),
                                                           m_hotkey.backendName()));
    else
        m_window->setStatus(Row::Hotkey, tr("Not active — %1").arg(m_hotkey.lastError()));

    m_window->setStatus(Row::AutoStart, AutoStart::isEnabled() ? tr("On") : tr("Off"));

    if (m_tray)
        m_window->setStatus(Row::Tray, tr("Visible"));
    else if (m_trayRetry.isActive())
        m_window->setStatus(Row::Tray, tr("Waiting for the system tray…"));
    else
        m_window->setStatus(Row::Tray, tr("Unavailable — enable a tray/AppIndicator extension; "
                                          "closing this window quits Deskout."));

    m_window->setStatus(Row::Notifications, m_notifier.backendName());

    if (!m_activity.idleSupported())
        m_window->setStatus(Row::IdleDetection, tr("Unavailable on this desktop"));
    else if (!m_activity.idleDetectionEnabled())
        m_window->setStatus(Row::IdleDetection, tr("Off"));
    else
        m_window->setStatus(Row::IdleDetection, tr("After %1 min without input (%2)")
                                                    .arg(m_activity.idleThresholdMinutes())
                                                    .arg(m_activity.idleBackendName()));

    m_window->setStatus(Row::FullscreenDetection,
                        m_activity.fullscreenDetectionEnabled() ? m_activity.fullscreenBackendName()
                                                                : tr("Off"));
}
