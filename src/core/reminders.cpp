#include "core/reminders.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>

namespace {

QString key(const QString &id, const char *field)
{
    return QStringLiteral("reminders/%1/%2").arg(id, QLatin1String(field));
}

QString timeText(const QTime &time)
{
    return time.toString(QStringLiteral("HH:mm"));
}

QString tr(const char *text)
{
    return QCoreApplication::translate("Reminders", text);
}

} // namespace

bool ReminderConfig::isActiveAt(const QDateTime &when) const
{
    if (!enabled || days == 0 || intervalMinutes <= 0)
        return false;

    const QDate date = when.date();
    const QTime time = when.time();
    if (start == end)
        return isDayActive(date.dayOfWeek());
    if (start < end)
        return isDayActive(date.dayOfWeek()) && time >= start && time < end;

    // Overnight window, e.g. 22:00–06:00: the early-morning part belongs to
    // the previous day's schedule.
    if (time >= start)
        return isDayActive(date.dayOfWeek());
    if (time < end)
        return isDayActive(date.addDays(-1).dayOfWeek());
    return false;
}

QString ReminderConfig::summary() const
{
    const QString hours = start == end ? tr("all day")
                                       : QStringLiteral("%1–%2").arg(timeText(start), timeText(end));
    return tr("Every %1 min · %2 · %3 · %4")
        .arg(QString::number(intervalMinutes), Reminders::describeDays(days), hours,
             fullScreen ? tr("Full-screen") : tr("Notification"));
}

bool ReminderConfig::operator==(const ReminderConfig &o) const
{
    return id == o.id && enabled == o.enabled && intervalMinutes == o.intervalMinutes
           && days == o.days && start == o.start && end == o.end && fullScreen == o.fullScreen;
}

namespace Reminders {

QStringList allIds()
{
    return {QLatin1String(ReminderIds::Eye), QLatin1String(ReminderIds::Water),
            QLatin1String(ReminderIds::Walk)};
}

ReminderTexts texts(const QString &id)
{
    if (id == QLatin1String(ReminderIds::Eye))
        return {tr("Eye break"), tr("Time for an eye break"),
                tr("Look away from your screen and let your eyes relax."), tr("I took the break")};
    if (id == QLatin1String(ReminderIds::Water))
        return {tr("Drink water"), tr("Time to drink some water"),
                tr("Grab a glass of water and take a few sips."), tr("I drank water")};
    if (id == QLatin1String(ReminderIds::Walk))
        return {tr("Walk & stretch"), tr("Time to get up and move"),
                tr("Stand up, stretch and walk around for a couple of minutes."), tr("I'm back")};
    return {id, id, QString(), tr("Done")};
}

ReminderConfig defaults(const QString &id)
{
    // Standard 9:00–18:00 workday, Monday to Friday.
    ReminderConfig config;
    config.id = id;
    if (id == QLatin1String(ReminderIds::Eye))
        config.intervalMinutes = 20; // the 20-20-20 rule
    else if (id == QLatin1String(ReminderIds::Water))
        config.intervalMinutes = 60;
    else if (id == QLatin1String(ReminderIds::Walk))
        config.intervalMinutes = 60;
    return config;
}

ReminderConfig load(const QString &id)
{
    const ReminderConfig fallback = defaults(id);
    QSettings settings;
    ReminderConfig config;
    config.id = id;
    config.enabled = settings.value(key(id, "enabled"), fallback.enabled).toBool();
    config.intervalMinutes = qBound(1, settings.value(key(id, "intervalMinutes"), fallback.intervalMinutes).toInt(), 24 * 60);
    config.days = settings.value(key(id, "days"), fallback.days).toInt() & ReminderDays::EveryDay;
    config.start = QTime::fromString(settings.value(key(id, "start")).toString(), QStringLiteral("HH:mm"));
    config.end = QTime::fromString(settings.value(key(id, "end")).toString(), QStringLiteral("HH:mm"));
    if (!config.start.isValid())
        config.start = fallback.start;
    if (!config.end.isValid())
        config.end = fallback.end;
    config.fullScreen = settings.value(key(id, "fullScreen"), fallback.fullScreen).toBool();
    return config;
}

void save(const ReminderConfig &config)
{
    QSettings settings;
    settings.setValue(key(config.id, "enabled"), config.enabled);
    settings.setValue(key(config.id, "intervalMinutes"), config.intervalMinutes);
    settings.setValue(key(config.id, "days"), config.days);
    settings.setValue(key(config.id, "start"), timeText(config.start));
    settings.setValue(key(config.id, "end"), timeText(config.end));
    settings.setValue(key(config.id, "fullScreen"), config.fullScreen);
}

QString describeDays(int days)
{
    days &= ReminderDays::EveryDay;
    if (days == ReminderDays::EveryDay)
        return tr("Every day");
    if (days == ReminderDays::Weekdays)
        return QStringLiteral("%1–%2").arg(QLocale().dayName(Qt::Monday, QLocale::ShortFormat),
                                            QLocale().dayName(Qt::Friday, QLocale::ShortFormat));
    if (days == (ReminderDays::bit(Qt::Saturday) | ReminderDays::bit(Qt::Sunday)))
        return tr("Weekends");
    if (days == 0)
        return tr("No days");
    QStringList names;
    for (int day = Qt::Monday; day <= Qt::Sunday; ++day) {
        if (days & ReminderDays::bit(day))
            names << QLocale().dayName(day, QLocale::ShortFormat);
    }
    return names.join(QLatin1String(", "));
}

} // namespace Reminders
