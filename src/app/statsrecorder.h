#pragma once

#include <QObject>
#include <QDate>
#include <QHash>

#include <functional>

class FocusTimer;
class ReminderEngine;
class StatsStore;
struct FocusSession;
enum class ReminderOutcome;

// Turns reminder and focus events into the daily totals behind the stats
// view. A reminder counts as shown once per due (coming back after a
// snooze doesn't count again) and as taken when the user confirms it.
// "Test now" doesn't count at all.
class StatsRecorder : public QObject
{
    Q_OBJECT

public:
    using Today = std::function<QDate()>;

    StatsRecorder(ReminderEngine *engine, FocusTimer *focus, StatsStore *stats, QObject *parent = nullptr,
                  Today today = {});

private:
    void onDue(const QString &id);
    void onResolved(const QString &id, ReminderOutcome outcome);
    void onSessionEnded(const FocusSession &session);

    ReminderEngine *m_engine;
    StatsStore *m_stats;
    Today m_today;
    // Reminder id -> day it was snoozed. A snooze left hanging (app quit,
    // paused all evening) doesn't swallow the next day's first break.
    QHash<QString, QDate> m_snoozedOn;
};
