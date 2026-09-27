#include "core/stats.h"

#include "core/database.h"
#include "core/settingskeys.h"

#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

namespace StatsMetric {

QString reminderShown(const QString &reminderId)
{
    return QStringLiteral("reminder.%1.shown").arg(reminderId);
}

QString reminderTaken(const QString &reminderId)
{
    return QStringLiteral("reminder.%1.taken").arg(reminderId);
}

} // namespace StatsMetric

int DayAdherence::percent() const
{
    if (shown <= 0)
        return 0;
    return std::min(100, taken * 100 / shown);
}

StatsStore::StatsStore(QObject *parent)
    : QObject(parent)
{
}

bool StatsStore::add(const QDate &day, const QString &metric, int delta)
{
    if (!Database::isOpen() || !day.isValid() || delta == 0)
        return false;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("INSERT INTO daily_stats (day, metric, value) VALUES (?, ?, ?)"
                                 " ON CONFLICT(day, metric) DO UPDATE SET value = value + excluded.value"));
    query.addBindValue(day.toString(Qt::ISODate));
    query.addBindValue(metric);
    query.addBindValue(delta);
    if (!query.exec()) {
        qWarning("Deskout: could not update stats: %s", qPrintable(query.lastError().text()));
        return false;
    }
    Q_EMIT changed(day);
    return true;
}

int StatsStore::value(const QDate &day, const QString &metric) const
{
    if (!Database::isOpen())
        return 0;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("SELECT value FROM daily_stats WHERE day = ? AND metric = ?"));
    query.addBindValue(day.toString(Qt::ISODate));
    query.addBindValue(metric);
    return query.exec() && query.next() ? query.value(0).toInt() : 0;
}

QMap<QDate, DayAdherence> StatsStore::adherence(const QDate &from, const QDate &to) const
{
    QMap<QDate, DayAdherence> days;
    if (!Database::isOpen())
        return days;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral(
        "SELECT day,"
        " SUM(CASE WHEN metric LIKE 'reminder.%.shown' THEN value ELSE 0 END),"
        " SUM(CASE WHEN metric LIKE 'reminder.%.taken' THEN value ELSE 0 END)"
        " FROM daily_stats WHERE day BETWEEN ? AND ? AND metric LIKE 'reminder.%'"
        " GROUP BY day"));
    query.addBindValue(from.toString(Qt::ISODate));
    query.addBindValue(to.toString(Qt::ISODate));
    if (!query.exec()) {
        qWarning("Deskout: could not read stats: %s", qPrintable(query.lastError().text()));
        return days;
    }
    while (query.next()) {
        DayAdherence day;
        day.shown = query.value(1).toInt();
        day.taken = query.value(2).toInt();
        days.insert(QDate::fromString(query.value(0).toString(), Qt::ISODate), day);
    }
    return days;
}

Streak StatsStore::streak(const QDate &today, int thresholdPercent) const
{
    // Daily rows: even ten years are only a few thousand.
    return computeStreak(adherence(QDate(1970, 1, 1), today), today, thresholdPercent);
}

Streak StatsStore::computeStreak(const QMap<QDate, DayAdherence> &days, const QDate &today, int thresholdPercent)
{
    Streak streak;
    // Current: newest first, stopping at the first active day that missed.
    bool counting = true;
    for (auto it = days.upperBound(today); it != days.begin();) {
        --it;
        if (!it->isActive())
            continue;
        if (it->meets(thresholdPercent)) {
            ++streak.current;
        } else if (it.key() != today) {
            counting = false; // today can still catch up
        }
        if (!counting)
            break;
    }
    // Best: longest run over the whole history, oldest first.
    int run = 0;
    for (auto it = days.cbegin(); it != days.cend() && it.key() <= today; ++it) {
        if (!it->isActive())
            continue;
        if (it->meets(thresholdPercent)) {
            streak.best = std::max(streak.best, ++run);
        } else if (it.key() != today) {
            run = 0;
        }
    }
    streak.best = std::max(streak.best, streak.current);
    return streak;
}

int StatsStore::threshold()
{
    return std::clamp(QSettings().value(SettingsKeys::StatsStreakThreshold, DefaultThreshold).toInt(), 1, 100);
}

void StatsStore::setThreshold(int percent)
{
    QSettings().setValue(SettingsKeys::StatsStreakThreshold, std::clamp(percent, 1, 100));
}
