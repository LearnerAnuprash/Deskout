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
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QThread>

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
    connect(&m_readingMode, &ReadingMode::changed, this, &DeskoutApp::syncReadingModeUi);
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
    connect(m_window.get(), &MainWindow::readingModeToggled, this, &DeskoutApp::setReadingMode);
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

    m_readingMode.start();
    syncReadingModeUi();
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
    else if (command == QLatin1String(Commands::ToggleReadingMode))
        setReadingMode(!m_readingMode.isEnabled());
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
    m_readingMode.shutdown();
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
    connect(m_tray.get(), &TrayController::readingModeToggled, this, &DeskoutApp::setReadingMode);
    m_tray->setReadingModeChecked(m_readingMode.isEnabled());
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

    QString reading;
    if (m_readingMode.isActive())
        reading = tr("On");
    else if (m_readingMode.isPending())
        reading = tr("Waiting — log out and back in once to finish setup");
    else
        reading = tr("Off");
    m_window->setStatus(Row::ReadingMode, tr("%1 (%2)").arg(reading, m_readingMode.backendName()));
}

QWidget *DeskoutApp::dialogParent() const
{
    return m_window && m_window->isVisible() ? m_window.get() : nullptr;
}

void DeskoutApp::setReadingMode(bool on)
{
    if (!on) {
        m_readingMode.setEnabled(false);
        return;
    }

    const GrayscaleBackend::Status status = m_readingMode.status();
    if (status.availability == ReadingMode::Availability::NeedsInstall) {
        QMessageBox box(QMessageBox::Question, tr("Set up Reading mode"),
                        status.message + QStringLiteral("\n\n")
                            + tr("On GNOME with Wayland, you'll need to log out and back in once "
                                 "after installing."),
                        QMessageBox::Cancel, dialogParent());
        QPushButton *install = box.addButton(tr("Install"), QMessageBox::AcceptRole);
        box.setDefaultButton(install);
        box.exec();
        if (box.clickedButton() != install) {
            syncReadingModeUi();
            return;
        }
        QString error;
        if (!m_readingMode.install(&error)) {
            QMessageBox::warning(dialogParent(), tr("Reading mode"), error);
            syncReadingModeUi();
            return;
        }
        // GNOME activates the extension right away when it already knows it
        // (X11 session, reinstall); give it a moment before falling back to
        // "log out and back in".
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < 1500
               && m_readingMode.status().availability != ReadingMode::Availability::Ready) {
            QCoreApplication::processEvents();
            QThread::msleep(150);
        }
    }

    QString message;
    if (!m_readingMode.setEnabled(true, &message)) {
        if (m_readingMode.isPending())
            QMessageBox::information(dialogParent(), tr("Almost there"), message);
        else
            QMessageBox::warning(dialogParent(), tr("Reading mode"), message);
    }
    syncReadingModeUi();
}

void DeskoutApp::syncReadingModeUi()
{
    const bool on = m_readingMode.isEnabled();
    if (m_tray)
        m_tray->setReadingModeChecked(on);
    if (m_window) {
        const QString tip = m_readingMode.isPending()
                                ? tr("Reading mode is waiting: log out and back in once to finish setup.")
                                : tr("Turn the whole screen grayscale for distraction-free reading.");
        m_window->setReadingMode(on, tip);
    }
    refreshStatus();
}
