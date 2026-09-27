#include "core/dailyupdates.h"

#include "core/database.h"
#include "core/settingskeys.h"

#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QString key(const QDate &day)
{
    return day.toString(Qt::ISODate);
}

DailyUpdate updateFrom(const QSqlQuery &query)
{
    DailyUpdate update;
    update.day = QDate::fromString(query.value(0).toString(), Qt::ISODate);
    update.done = query.value(1).toString();
    update.todo = query.value(2).toString();
    update.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong());
    update.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong());
    return update;
}

void warn(const char *what, const QSqlQuery &query)
{
    qWarning("Deskout: could not %s: %s", what, qPrintable(query.lastError().text()));
}

constexpr char Columns[] = "day, done, todo, created_at, updated_at";

} // namespace

DailyUpdatesStore::DailyUpdatesStore(QObject *parent, Clock clock)
    : QObject(parent)
    , m_now(std::move(clock))
{
    if (!m_now)
        m_now = [] { return QDateTime::currentDateTime(); };
}

std::optional<DailyUpdate> DailyUpdatesStore::get(const QDate &day) const
{
    if (!Database::isOpen() || !day.isValid())
        return std::nullopt;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("SELECT %1 FROM daily_updates WHERE day = ?").arg(QLatin1String(Columns)));
    query.addBindValue(key(day));
    if (!query.exec() || !query.next())
        return std::nullopt;
    return updateFrom(query);
}

bool DailyUpdatesStore::save(const QDate &day, const QString &done, const QString &todo)
{
    if (!Database::isOpen() || !day.isValid())
        return false;
    QSqlQuery query(Database::connection());
    if (done.trimmed().isEmpty() && todo.trimmed().isEmpty()) {
        query.prepare(QStringLiteral("DELETE FROM daily_updates WHERE day = ?"));
        query.addBindValue(key(day));
    } else {
        // COALESCE: a null QString binds as NULL, which the columns reject.
        query.prepare(QStringLiteral(
            "INSERT INTO daily_updates (day, done, todo, created_at, updated_at)"
            " VALUES (?, COALESCE(?, ''), COALESCE(?, ''), ?, ?)"
            " ON CONFLICT(day) DO UPDATE SET done = excluded.done, todo = excluded.todo,"
            " updated_at = excluded.updated_at"));
        const qint64 now = m_now().toMSecsSinceEpoch();
        query.addBindValue(key(day));
        query.addBindValue(done);
        query.addBindValue(todo);
        query.addBindValue(now);
        query.addBindValue(now);
    }
    if (!query.exec()) {
        warn("save daily update", query);
        return false;
    }
    Q_EMIT changed(day);
    return true;
}

QList<DailyUpdate> DailyUpdatesStore::history(int limit, const QDate &before) const
{
    QList<DailyUpdate> updates;
    if (!Database::isOpen())
        return updates;
    QSqlQuery query(Database::connection());
    QString sql = QStringLiteral("SELECT %1 FROM daily_updates").arg(QLatin1String(Columns));
    if (before.isValid())
        sql += QStringLiteral(" WHERE day < ?");
    sql += QStringLiteral(" ORDER BY day DESC LIMIT ?");
    query.prepare(sql);
    if (before.isValid())
        query.addBindValue(key(before)); // ISO dates sort as text
    query.addBindValue(limit);
    if (!query.exec()) {
        warn("load daily updates", query);
        return updates;
    }
    while (query.next())
        updates << updateFrom(query);
    return updates;
}

std::optional<DailyUpdate> DailyUpdatesStore::latestBefore(const QDate &day) const
{
    const QList<DailyUpdate> latest = history(1, day);
    if (latest.isEmpty())
        return std::nullopt;
    return latest.constFirst();
}

int DailyUpdatesStore::count() const
{
    if (!Database::isOpen())
        return 0;
    QSqlQuery query(Database::connection());
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM daily_updates")) && query.next())
        return query.value(0).toInt();
    return 0;
}

std::optional<DailyUpdate> DailyUpdatesStore::recapDue(const QDate &today) const
{
    if (!recapEnabled())
        return std::nullopt;
    if (QSettings().value(SettingsKeys::UpdatesRecapShownOn).toString() == key(today))
        return std::nullopt;
    return latestBefore(today);
}

void DailyUpdatesStore::markRecapShown(const QDate &today)
{
    QSettings().setValue(SettingsKeys::UpdatesRecapShownOn, key(today));
}

bool DailyUpdatesStore::recapEnabled()
{
    return QSettings().value(SettingsKeys::UpdatesRecapEnabled, true).toBool();
}

void DailyUpdatesStore::setRecapEnabled(bool enabled)
{
    QSettings().setValue(SettingsKeys::UpdatesRecapEnabled, enabled);
}
