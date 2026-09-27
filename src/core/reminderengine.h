#pragma once

#include "core/reminders.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <functional>

enum class ReminderState {
    Disabled,     // switched off
    OutsideHours, // not an active day / outside active hours
    Paused,       // global pause
    Idle,         // user away; clock frozen
    Counting,     // clock running
    Due,          // alert raised, waiting for confirm/snooze
};

enum class ReminderOutcome {
    Confirmed, // user confirmed on a full-screen alarm or notification
    Snoozed,
    Notified,  // delivered as a notification (no confirmation required)
};

struct ReminderStatus
{
    ReminderState state = ReminderState::Disabled;
    int secondsRemaining = 0;
};

// One engine drives every reminder type. Each reminder has its own clock
// that only advances while the reminder is active (enabled, right day,
// inside its hours) and the user is present (not paused, not idle).
// Presentation (full-screen vs notification) is someone else's job; the
// engine only says "due" and is told how the user responded.
class ReminderEngine : public QObject
{
    Q_OBJECT

public:
    struct Hooks
    {
        std::function<bool()> isPaused;
        std::function<bool()> isIdle;
        std::function<QDateTime()> now;
    };

    explicit ReminderEngine(Hooks hooks = {}, QObject *parent = nullptr);

    // Loads configs from QSettings and starts the 1 s tick.
    void start();
    void reload();

    QStringList ids() const;
    ReminderConfig config(const QString &id) const;
    // Saves to QSettings and applies immediately; progress is kept.
    void setConfig(const ReminderConfig &config);
    ReminderStatus status(const QString &id) const;

    // Advances every reminder clock by `elapsedMs` of wall time. Called by
    // the internal tick; public so tests can drive time.
    void advance(qint64 elapsedMs);

    // Raise the reminder right away (the "Test now" button).
    void triggerNow(const QString &id);
    // Responses to a due reminder.
    void confirm(const QString &id);
    void snooze(const QString &id, int minutes);
    void acknowledge(const QString &id);

Q_SIGNALS:
    void reminderDue(const QString &id);
    void reminderResolved(const QString &id, ReminderOutcome outcome);
    void configChanged(const QString &id);

private:
    struct Entry
    {
        ReminderConfig config;
        qint64 elapsedMs = 0;
        bool due = false;
        bool wasActive = false;
    };

    Entry *find(const QString &id);
    const Entry *find(const QString &id) const;
    void onTick();
    void resolve(Entry *entry, qint64 elapsedMs, ReminderOutcome outcome);

    Hooks m_hooks;
    QList<Entry> m_entries;
    QTimer m_tick;
    QElapsedTimer m_clock;
};
