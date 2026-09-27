#include "app/focusalerts.h"

#include "app/alertcenter.h"
#include "core/focustimer.h"
#include "core/settingskeys.h"
#include "ui/notifier.h"

#include <QSettings>

FocusAlertController::FocusAlertController(FocusTimer *timer, AlertCenter *alerts, Notifier *notifier,
                                           QObject *parent)
    : QObject(parent)
    , m_timer(timer)
    , m_alerts(alerts)
    , m_notifier(notifier)
{
    connect(m_timer, &FocusTimer::sessionEnded, this, &FocusAlertController::onSessionEnded);
    connect(m_alerts, &AlertCenter::responded, this, &FocusAlertController::onResponded);
    connect(m_alerts, &AlertCenter::fullScreenDisabled, this, &FocusAlertController::onFullScreenDisabled);
}

bool FocusAlertController::fullScreenEnabled()
{
    return QSettings().value(SettingsKeys::FocusFullScreenAlert, SettingsKeys::FocusFullScreenAlertDefault).toBool();
}

void FocusAlertController::setFullScreenEnabled(bool enabled)
{
    QSettings().setValue(SettingsKeys::FocusFullScreenAlert, enabled);
}

void FocusAlertController::onSessionEnded(const FocusSession &session)
{
    if (!session.completed)
        return;

    const int minutes = session.plannedSeconds / 60;
    const QString duration = minutes == 1 ? tr("1 minute") : tr("%1 minutes").arg(minutes);
    Alert alert;
    alert.key = QLatin1String(AlertKey);
    alert.title = tr("Focus session complete");
    alert.body = session.topic.isEmpty()
                     ? tr("You focused for %1. Stand up and rest your eyes for a few minutes.").arg(duration)
                     : tr("%1 on “%2”. Stand up and rest your eyes for a few minutes.")
                           .arg(duration, session.topic);
    alert.confirmLabel = tr("Done");
    alert.actions << AlertAction{QLatin1String(StartAgainKey), tr("Start another %1 min").arg(minutes)};
    alert.disableFullScreenLabel = tr("Disable full-screen alert");
    alert.fullScreen = fullScreenEnabled();
    m_alerts->raise(alert);
}

void FocusAlertController::onResponded(const QString &key, const QString &actionKey)
{
    if (key == QLatin1String(AlertKey) && actionKey == QLatin1String(StartAgainKey))
        m_timer->startAgain();
}

void FocusAlertController::onFullScreenDisabled(const QString &key)
{
    if (key != QLatin1String(AlertKey))
        return;
    setFullScreenEnabled(false);
    m_notifier->notify(tr("Focus timer: full-screen alert turned off"),
                       tr("You'll get a notification when a session ends. You can switch it back "
                          "on the Focus Timer page."));
}
