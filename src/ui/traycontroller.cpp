#include "ui/traycontroller.h"

#include "core/focustimer.h"
#include "core/pausemanager.h"
#include "ui/appicon.h"

#include <QMenu>

TrayController::TrayController(PauseManager *pause, FocusTimer *focus, QObject *parent)
    : QObject(parent)
    , m_pause(pause)
    , m_focus(focus)
    , m_menu(new QMenu)
{
    m_menu->addAction(tr("Open Deskout"), this, &TrayController::openRequested);
    m_menu->addSeparator();

    m_statusAction = m_menu->addAction(QString());
    m_statusAction->setEnabled(false);

    m_pauseMenu = m_menu->addMenu(tr("Pause all reminders"));
    m_pauseMenu->addAction(tr("For 15 minutes"), m_pause, [this] { m_pause->pauseFor(15); });
    m_pauseMenu->addAction(tr("For 1 hour"), m_pause, [this] { m_pause->pauseFor(60); });
    m_pauseMenu->addAction(tr("Until tomorrow"), m_pause, &PauseManager::pauseUntilTomorrow);
    m_pauseMenu->addAction(tr("Until I resume"), m_pause, &PauseManager::pauseIndefinitely);

    m_resumeAction = m_menu->addAction(tr("Resume reminders"), m_pause, &PauseManager::resume);

    m_menu->addSeparator();
    m_focusAction = m_menu->addAction(QString(), this, &TrayController::onFocusActionTriggered);
    m_focusStopAction = m_menu->addAction(tr("Stop focus session"), m_focus, &FocusTimer::stop);
    m_menu->addAction(tr("Show focus timer"), this, &TrayController::focusWindowRequested);

    m_menu->addSeparator();
    m_readingModeAction = m_menu->addAction(tr("Reading mode (grayscale)"));
    m_readingModeAction->setCheckable(true);
    connect(m_readingModeAction, &QAction::triggered, this, &TrayController::readingModeToggled);

    m_menu->addSeparator();
    m_menu->addAction(tr("Settings…"), this, &TrayController::settingsRequested);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit Deskout"), this, &TrayController::quitRequested);

    m_tray.setContextMenu(m_menu);
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            Q_EMIT openRequested();
    });
    connect(m_pause, &PauseManager::pausedChanged, this, &TrayController::refresh);
    connect(m_focus, &FocusTimer::stateChanged, this, &TrayController::refreshFocus);
    // A timed pause's label ("until 14:30") and the focus time left change
    // without a state change, so refresh on open too.
    connect(m_menu, &QMenu::aboutToShow, this, &TrayController::refresh);
    refresh();
}

TrayController::~TrayController()
{
    m_tray.hide();
    delete m_menu;
}

void TrayController::show()
{
    m_tray.show();
}

bool TrayController::isVisible() const
{
    return m_tray.isVisible();
}

void TrayController::notify(const QString &title, const QString &message)
{
    if (m_tray.isVisible() && QSystemTrayIcon::supportsMessages())
        m_tray.showMessage(title, message, AppIcon::icon(m_pause->isPaused()), 5000);
}

void TrayController::setReadingModeChecked(bool checked)
{
    const QSignalBlocker blocker(m_readingModeAction);
    m_readingModeAction->setChecked(checked);
}

void TrayController::refresh()
{
    const bool paused = m_pause->isPaused();
    const QString status = m_pause->statusText();
    m_statusAction->setText(status);
    m_resumeAction->setVisible(paused);
    m_pauseMenu->setTitle(paused ? tr("Change pause") : tr("Pause all reminders"));
    m_tray.setIcon(AppIcon::icon(paused));
    m_tray.setToolTip(QStringLiteral("Deskout — %1").arg(status));
    refreshFocus();
}

void TrayController::refreshFocus()
{
    const QString left = FocusTimer::formatSeconds(m_focus->remainingSeconds());
    switch (m_focus->state()) {
    case FocusTimer::State::Running:
        m_focusAction->setText(tr("Pause focus session (%1 left)").arg(left));
        break;
    case FocusTimer::State::Paused:
        m_focusAction->setText(tr("Resume focus session (%1 left)").arg(left));
        break;
    case FocusTimer::State::Idle:
    case FocusTimer::State::Finished:
        m_focusAction->setText(tr("Start focus session (%1 min)").arg(m_focus->lastMinutes()));
        break;
    }
    m_focusStopAction->setVisible(m_focus->isActive());
}

void TrayController::onFocusActionTriggered()
{
    const bool starting = !m_focus->isActive();
    m_focus->toggle();
    // Starting from the tray: show the timer so the session is visible.
    if (starting)
        Q_EMIT focusWindowRequested();
}
