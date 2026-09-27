#include "ui/timeformat.h"

#include <QCoreApplication>
#include <QLocale>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("TimeFormat", text);
}

} // namespace

namespace TimeFormat {

QString dateTime(const QDateTime &when, const QDate &today)
{
    const QLocale locale;
    const QString time = locale.toString(when.time(), QLocale::ShortFormat);
    const QDate date = when.date();
    if (date == today)
        return time;
    if (date == today.addDays(-1))
        return tr("Yesterday %1").arg(time);
    const QString format = date.year() == today.year() ? QStringLiteral("ddd d MMM") : QStringLiteral("d MMM yyyy");
    return locale.toString(date, format) + QLatin1Char(' ') + time;
}

QString compact(const QDateTime &when, const QDate &today)
{
    const QLocale locale;
    const QDate date = when.date();
    if (date == today)
        return locale.toString(when.time(), QLocale::ShortFormat);
    if (date == today.addDays(-1))
        return tr("Yesterday");
    const qint64 daysAgo = date.daysTo(today);
    if (daysAgo > 0 && daysAgo < 7)
        return locale.toString(date, QStringLiteral("ddd"));
    return locale.toString(date, date.year() == today.year() ? QStringLiteral("d MMM") : QStringLiteral("d MMM yyyy"));
}

} // namespace TimeFormat
