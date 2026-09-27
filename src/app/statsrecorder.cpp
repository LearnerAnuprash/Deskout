#include "app/statsrecorder.h"

#include "core/focuslog.h"
#include "core/focustimer.h"
#include "core/reminderengine.h"
#include "core/stats.h"

StatsRecorder::StatsRecorder(ReminderEngine *engine, FocusTimer *focus, StatsStore *stats, QObject *parent,
                             Today today)
    : QObject(parent)
    , m_engine(engine)
    , m_stats(stats)
    , m_today(std::move(today))
{
    if (!m_today)
        m_today = [] { return QDate::currentDate(); };
    connect(m_engine, &ReminderEngine::reminderDue, this, &StatsRecorder::onDue);
    connect(m_engine, &ReminderEngine::reminderResolved, this, &StatsRecorder::onResolved);
    connect(focus, &FocusTimer::sessionEnded, this, &StatsRecorder::onSessionEnded);
}

void StatsRecorder::onDue(const QString &id)
{
    if (m_engine->isManual(id))
        return;
    // Back after a snooze: still the same break.
    if (m_snoozedOn.take(id) == m_today())
        return;
    m_stats->add(m_today(), StatsMetric::reminderShown(id));
}

void StatsRecorder::onResolved(const QString &id, ReminderOutcome outcome)
{
    if (m_engine->isManual(id))
        return;
    switch (outcome) {
    case ReminderOutcome::Snoozed:
        m_snoozedOn.insert(id, m_today());
        break;
    case ReminderOutcome::Confirmed:
        m_snoozedOn.remove(id);
        m_stats->add(m_today(), StatsMetric::reminderTaken(id));
        break;
    case ReminderOutcome::Notified:
        m_snoozedOn.remove(id);
        break;
    }
}

void StatsRecorder::onSessionEnded(const FocusSession &session)
{
    // Same rule as the focus history: brief stops are noise.
    if (!session.completed && session.focusedSeconds < FocusLog::MinAbandonedSeconds)
        return;
    const QDate day = session.startedAt.isValid() ? session.startedAt.date() : m_today();
    if (session.completed)
        m_stats->add(day, QLatin1String(StatsMetric::FocusCompleted));
    m_stats->add(day, QLatin1String(StatsMetric::FocusSeconds), session.focusedSeconds);
}
