#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTime>

// The built-in reminder types. All share one ReminderConfig shape and one
// engine; only their texts (and the eye-exercise follow-up) differ.
namespace ReminderIds {
inline constexpr char Eye[] = "eye";
inline constexpr char Water[] = "water";
inline constexpr char Walk[] = "walk";
} // namespace ReminderIds

// Days are a bit mask: bit 0 = Monday ... bit 6 = Sunday (Qt::DayOfWeek - 1).
namespace ReminderDays {
inline constexpr int Weekdays = 0b0011111;
inline constexpr int EveryDay = 0b1111111;
inline constexpr int bit(int qtDayOfWeek) { return 1 << (qtDayOfWeek - 1); }
} // namespace ReminderDays

struct ReminderConfig
{
    QString id;
    bool enabled = true;
    int intervalMinutes = 60;
    int days = ReminderDays::Weekdays;
    QTime start = QTime(9, 0);
    QTime end = QTime(18, 0);
    bool fullScreen = true;

    // True if the reminder should be counting at `when`: enabled, on an
    // active day, inside the active hours. `end` <= `start` means the window
    // runs past midnight (and belongs to the day it started); start == end
    // means all day.
    bool isActiveAt(const QDateTime &when) const;
    bool isDayActive(int qtDayOfWeek) const { return days & ReminderDays::bit(qtDayOfWeek); }
    qint64 intervalMs() const { return qint64(intervalMinutes) * 60 * 1000; }

    // e.g. "Every 20 min · Mon–Fri · 09:00–18:00 · Full-screen"
    QString summary() const;

    bool operator==(const ReminderConfig &other) const;
    bool operator!=(const ReminderConfig &other) const { return !(*this == other); }
};

struct ReminderTexts
{
    QString name;         // "Eye break"
    QString title;        // "Time for an eye break"
    QString body;         // one-line instruction
    QString confirmLabel; // "I took the break"
};

namespace Reminders {

QStringList allIds();
ReminderTexts texts(const QString &id);
ReminderConfig defaults(const QString &id);
ReminderConfig load(const QString &id);
void save(const ReminderConfig &config);

// "Mon–Fri", "Every day", "Mon, Wed, Fri"...
QString describeDays(int days);

} // namespace Reminders
