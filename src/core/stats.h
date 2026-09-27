#pragma once

#include <QDate>
#include <QHash>
#include <QMap>
#include <QObject>

// Metric names in the daily_stats table.
namespace StatsMetric {

// "reminder.eye.shown": a reminder came up (once per due, snoozes
// included). "reminder.eye.taken": the user confirmed the break.
QString reminderShown(const QString &reminderId);
QString reminderTaken(const QString &reminderId);
inline constexpr char FocusCompleted[] = "focus.completed";
inline constexpr char FocusSeconds[] = "focus.seconds";

} // namespace StatsMetric

// How well one day's breaks went, all reminder types together.
struct DayAdherence
{
    int shown = 0;
    int taken = 0;

    // A day without reminders (weekend, day off) neither makes nor breaks
    // a streak.
    bool isActive() const { return shown > 0; }
    // 0-100. Taken can briefly exceed shown around midnight; capped.
    int percent() const;
    bool meets(int thresholdPercent) const { return isActive() && percent() >= thresholdPercent; }
};

struct Streak
{
    int current = 0;
    int best = 0;
};

// Per-day totals for the stats view. Stored as aggregates, not events, so
// reading a day or a streak stays cheap however long the history gets.
class StatsStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int DefaultThreshold = 80;

    explicit StatsStore(QObject *parent = nullptr);

    bool add(const QDate &day, const QString &metric, int delta = 1);
    int value(const QDate &day, const QString &metric) const;
    // Days with reminder activity in [from, to].
    QMap<QDate, DayAdherence> adherence(const QDate &from, const QDate &to) const;
    Streak streak(const QDate &today, int thresholdPercent) const;

    // Walks back from today over active days: each must meet the
    // threshold. Today only counts once it does (it isn't over yet), and
    // inactive days are skipped.
    static Streak computeStreak(const QMap<QDate, DayAdherence> &days, const QDate &today, int thresholdPercent);

    static int threshold();
    static void setThreshold(int percent);

Q_SIGNALS:
    void changed(const QDate &day);
};
