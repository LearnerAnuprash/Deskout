#include "app/reminderalerts.h"

#include "app/alertcenter.h"
#include "core/reminderengine.h"
#include "ui/notifier.h"

namespace {

constexpr char KeyPrefix[] = "reminder/";
constexpr int PauseSnoozeMinutes = 5;

const struct
{
    const char *key;
    int minutes;
} SnoozeActions[] = {{"snooze-5", 5}, {"snooze-10", 10}};

QString alertKey(const QString &id)
{
    return QLatin1String(KeyPrefix) + id;
}

// Empty when the alert isn't a reminder's.
QString reminderId(const QString &key)
{
    return key.startsWith(QLatin1String(KeyPrefix)) ? key.mid(int(qstrlen(KeyPrefix))) : QString();
}

} // namespace

ReminderAlertController::ReminderAlertController(ReminderEngine *engine, AlertCenter *alerts,
                                                 Notifier *notifier, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_alerts(alerts)
    , m_notifier(notifier)
{
    connect(m_engine, &ReminderEngine::reminderDue, this, &ReminderAlertController::onDue);
    connect(m_alerts, &AlertCenter::responded, this, &ReminderAlertController::onResponded);
    connect(m_alerts, &AlertCenter::notified, this, &ReminderAlertController::onNotified);
    connect(m_alerts, &AlertCenter::interrupted, this, &ReminderAlertController::onInterrupted);
    connect(m_alerts, &AlertCenter::fullScreenDisabled, this, &ReminderAlertController::disableFullScreen);
}

void ReminderAlertController::onDue(const QString &id)
{
    const ReminderTexts texts = Reminders::texts(id);
    Alert alert;
    alert.key = alertKey(id);
    alert.title = texts.title;
    alert.body = texts.body;
    alert.confirmLabel = texts.confirmLabel;
    for (const auto &snooze : SnoozeActions)
        alert.actions << AlertAction{QLatin1String(snooze.key), tr("Snooze %1 min").arg(snooze.minutes)};
    alert.disableFullScreenLabel = tr("Disable full-screen reminder");
    alert.fullScreen = m_engine->config(id).fullScreen;
    alert.eyeExercise = id == QLatin1String(ReminderIds::Eye);
    m_alerts->raise(alert);
}

void ReminderAlertController::onResponded(const QString &key, const QString &actionKey)
{
    const QString id = reminderId(key);
    if (id.isEmpty())
        return;
    if (actionKey == QLatin1String(Alert::ConfirmKey)) {
        m_engine->confirm(id);
        return;
    }
    for (const auto &snooze : SnoozeActions) {
        if (actionKey == QLatin1String(snooze.key))
            m_engine->snooze(id, snooze.minutes);
    }
}

void ReminderAlertController::onNotified(const QString &key)
{
    // A notification needs no answer: the clock restarts now.
    const QString id = reminderId(key);
    if (!id.isEmpty())
        m_engine->acknowledge(id);
}

void ReminderAlertController::onInterrupted(const QString &key)
{
    // Paused mid-alarm: try again shortly after the pause ends.
    const QString id = reminderId(key);
    if (!id.isEmpty())
        m_engine->snooze(id, PauseSnoozeMinutes);
}

void ReminderAlertController::disableFullScreen(const QString &key)
{
    const QString id = reminderId(key);
    if (id.isEmpty())
        return;
    ReminderConfig config = m_engine->config(id);
    config.fullScreen = false;
    m_engine->setConfig(config);
    m_engine->acknowledge(id);
    m_notifier->notify(tr("%1: full-screen alarm turned off").arg(Reminders::texts(id).name),
                       tr("You'll get a notification instead. You can switch it back in "
                          "Settings › Reminders."));
}
