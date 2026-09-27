#include "app/alertcenter.h"

#include "core/pausemanager.h"
#include "platform/activity/activitymonitor.h"
#include "ui/fullscreenalarm.h"
#include "ui/notifier.h"

#include <QTimer>

#include <algorithm>

namespace {
// Breathing room between two queued alarms.
constexpr int NextAlarmDelayMs = 800;
constexpr int MaxTrackedNotifications = 64;
} // namespace

AlertCenter::AlertCenter(ActivityMonitor *activity, Notifier *notifier, PauseManager *pause, QObject *parent)
    : QObject(parent)
    , m_activity(activity)
    , m_notifier(notifier)
    , m_pause(pause)
{
    connect(m_pause, &PauseManager::pausedChanged, this, &AlertCenter::onPausedChanged);
    connect(m_notifier, &Notifier::actionInvoked, this, &AlertCenter::onNotificationAction);
}

AlertCenter::~AlertCenter()
{
    delete m_alarm;
}

void AlertCenter::raise(const Alert &alert)
{
    if (m_pause->isPaused()) {
        Q_EMIT interrupted(alert.key);
        return;
    }
    if (!alert.fullScreen) {
        notify(alert);
        return;
    }
    if (m_activity->shouldAvoidFullscreen()) {
        notify(alert, tr("Shown as a notification because another app is full-screen or you're in a call."));
        return;
    }
    const bool queued = std::any_of(m_queue.cbegin(), m_queue.cend(),
                                    [&alert](const Alert &a) { return a.key == alert.key; });
    if (!queued && !isShowing(alert.key))
        m_queue << alert;
    showNext();
}

bool AlertCenter::isShowing(const QString &key) const
{
    return m_alarm && m_alarm->alertKey() == key;
}

void AlertCenter::showNext()
{
    while (!m_alarm && !m_queue.isEmpty()) {
        const Alert alert = m_queue.takeFirst();
        // Things may have changed while this one waited in the queue.
        if (m_activity->shouldAvoidFullscreen()) {
            notify(alert, tr("Shown as a notification because another app is full-screen or you're in a call."));
            continue;
        }

        auto *alarm = new FullScreenAlarm(alert, this);
        m_alarm = alarm;
        const QString key = alert.key;
        connect(alarm, &FullScreenAlarm::responded, this,
                [this, key](const QString &actionKey) { Q_EMIT responded(key, actionKey); });
        connect(alarm, &FullScreenAlarm::fullScreenDisabled, this, [this, key] { Q_EMIT fullScreenDisabled(key); });
        connect(alarm, &FullScreenAlarm::finished, this, [this, alarm] {
            alarm->deleteLater();
            if (m_alarm == alarm)
                m_alarm = nullptr;
            QTimer::singleShot(NextAlarmDelayMs, this, &AlertCenter::showNext);
        });
        alarm->show();
    }
}

void AlertCenter::notify(const Alert &alert, const QString &note)
{
    QList<Notifier::Action> actions{{QLatin1String(Alert::ConfirmKey), alert.confirmLabel}};
    if (!alert.actions.isEmpty())
        actions << Notifier::Action{alert.actions.constFirst().key, alert.actions.constFirst().label};

    const QString body = note.isEmpty() ? alert.body : alert.body + QLatin1Char('\n') + note;
    const uint notificationId = m_notifier->notify(alert.title, body, actions);
    if (notificationId != 0) {
        if (m_notifications.size() >= MaxTrackedNotifications)
            m_notifications.clear();
        m_notifications.insert(notificationId, alert.key);
    }
    // Buttons on the notification still report back through
    // onNotificationAction().
    Q_EMIT notified(alert.key);
}

void AlertCenter::onPausedChanged(bool paused)
{
    if (!paused)
        return;
    // Pausing mid-alarm (e.g. hotkey during a call) drops everything
    // showing or queued; each source decides when to try again. An alarm
    // already answered (eye exercise running) just closes.
    QStringList dropped;
    if (m_alarm && !m_alarm->isAnswered())
        dropped << m_alarm->alertKey();
    for (const Alert &alert : std::as_const(m_queue))
        dropped << alert.key;
    m_queue.clear();
    if (m_alarm)
        m_alarm->dismiss();
    for (const QString &key : std::as_const(dropped))
        Q_EMIT interrupted(key);
}

void AlertCenter::onNotificationAction(uint notificationId, const QString &actionKey)
{
    // "default" (clicking the notification body) carries no answer.
    if (actionKey == QLatin1String("default"))
        return;
    const QString key = m_notifications.take(notificationId);
    if (!key.isEmpty()) // else: another app's notification
        Q_EMIT responded(key, actionKey);
}
