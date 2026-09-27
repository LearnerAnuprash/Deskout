#include "app/reminderalerts.h"

#include "core/pausemanager.h"
#include "core/reminderengine.h"
#include "platform/activity/activitymonitor.h"
#include "ui/fullscreenalarm.h"
#include "ui/notifier.h"

#include <QTimer>

namespace {
// Breathing room between two queued alarms.
constexpr int NextAlarmDelayMs = 800;
constexpr int PauseSnoozeMinutes = 5;
constexpr int MaxTrackedNotifications = 64;
} // namespace

ReminderAlertController::ReminderAlertController(ReminderEngine *engine, ActivityMonitor *activity,
                                                 Notifier *notifier, PauseManager *pause,
                                                 QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_activity(activity)
    , m_notifier(notifier)
{
    connect(m_engine, &ReminderEngine::reminderDue, this, &ReminderAlertController::onDue);
    connect(pause, &PauseManager::pausedChanged, this, &ReminderAlertController::onPausedChanged);
    connect(m_notifier, &Notifier::actionInvoked, this, &ReminderAlertController::onNotificationAction);
}

ReminderAlertController::~ReminderAlertController()
{
    delete m_alarm;
}

void ReminderAlertController::onDue(const QString &id)
{
    if (!m_engine->config(id).fullScreen) {
        notify(id);
        return;
    }
    if (m_activity->shouldAvoidFullscreen()) {
        notify(id, tr("Shown as a notification because another app is full-screen or you're in a call."));
        return;
    }
    const bool showing = m_alarm && m_alarm->reminderId() == id;
    if (!showing && !m_queue.contains(id))
        m_queue << id;
    showNextAlarm();
}

void ReminderAlertController::showNextAlarm()
{
    while (!m_alarm && !m_queue.isEmpty()) {
        const QString id = m_queue.takeFirst();
        // Things may have changed while this one waited in the queue.
        if (m_activity->shouldAvoidFullscreen()) {
            notify(id, tr("Shown as a notification because another app is full-screen or you're in a call."));
            continue;
        }

        auto *alarm = new FullScreenAlarm(id, this);
        m_alarm = alarm;
        connect(alarm, &FullScreenAlarm::confirmed, this, [this, id] { m_engine->confirm(id); });
        connect(alarm, &FullScreenAlarm::snoozed, this, [this, id](int minutes) { m_engine->snooze(id, minutes); });
        connect(alarm, &FullScreenAlarm::fullScreenDisabled, this, [this, id] { disableFullScreen(id); });
        connect(alarm, &FullScreenAlarm::finished, this, [this, alarm] {
            alarm->deleteLater();
            if (m_alarm == alarm)
                m_alarm = nullptr;
            QTimer::singleShot(NextAlarmDelayMs, this, &ReminderAlertController::showNextAlarm);
        });
        alarm->show();
    }
}

void ReminderAlertController::notify(const QString &id, const QString &note)
{
    const ReminderTexts texts = Reminders::texts(id);
    const QString body = note.isEmpty() ? texts.body : texts.body + QLatin1Char('\n') + note;
    const uint notificationId = m_notifier->notify(
        texts.title, body,
        {{QStringLiteral("done"), texts.confirmLabel}, {QStringLiteral("snooze"), tr("Snooze 5 min")}});

    if (notificationId != 0) {
        if (m_notifications.size() >= MaxTrackedNotifications)
            m_notifications.clear();
        m_notifications.insert(notificationId, id);
    }
    // A notification needs no answer: the clock restarts now. Buttons on it
    // still report back through onNotificationAction().
    m_engine->acknowledge(id);
}

void ReminderAlertController::onPausedChanged(bool paused)
{
    if (!paused)
        return;
    // Pausing mid-alarm (e.g. hotkey during a call): put everything showing
    // or queued off until shortly after the pause ends.
    QStringList pending = m_queue;
    m_queue.clear();
    if (m_alarm) {
        pending.prepend(m_alarm->reminderId());
        m_alarm->dismiss();
    }
    for (const QString &id : std::as_const(pending))
        m_engine->snooze(id, PauseSnoozeMinutes);
}

void ReminderAlertController::onNotificationAction(uint notificationId, const QString &actionKey)
{
    // "default" (clicking the notification body) carries no answer.
    if (actionKey != QLatin1String("done") && actionKey != QLatin1String("snooze"))
        return;
    const QString id = m_notifications.take(notificationId);
    if (id.isEmpty())
        return; // another app's notification
    if (actionKey == QLatin1String("done"))
        m_engine->confirm(id);
    else if (actionKey == QLatin1String("snooze"))
        m_engine->snooze(id, 5);
}

void ReminderAlertController::disableFullScreen(const QString &id)
{
    ReminderConfig config = m_engine->config(id);
    config.fullScreen = false;
    m_engine->setConfig(config);
    m_engine->acknowledge(id);
    m_notifier->notify(tr("%1: full-screen alarm turned off").arg(Reminders::texts(id).name),
                       tr("You'll get a notification instead. You can switch it back in "
                          "Settings › Reminders."));
}
