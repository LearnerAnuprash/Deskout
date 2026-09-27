#include "core/focuslog.h"

#include "core/database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

FocusLog::FocusLog(QObject *parent)
    : QObject(parent)
{
}

bool FocusLog::record(const FocusSession &session)
{
    if (!session.completed && session.focusedSeconds < MinAbandonedSeconds)
        return false;
    if (!Database::isOpen())
        return false;

    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral(
        "INSERT INTO focus_sessions (started_at, day, planned_seconds, focused_seconds, completed, topic)"
        " VALUES (?, ?, ?, ?, ?, COALESCE(?, ''))")); // a null QString binds as NULL
    query.addBindValue(session.startedAt.toMSecsSinceEpoch());
    query.addBindValue(session.startedAt.date().toString(Qt::ISODate));
    query.addBindValue(session.plannedSeconds);
    query.addBindValue(session.focusedSeconds);
    query.addBindValue(session.completed ? 1 : 0);
    query.addBindValue(session.topic);
    if (!query.exec()) {
        qWarning("Deskout: could not save focus session: %s", qPrintable(query.lastError().text()));
        return false;
    }
    Q_EMIT changed();
    return true;
}

QList<FocusSession> FocusLog::recent(int limit) const
{
    QList<FocusSession> sessions;
    if (!Database::isOpen())
        return sessions;

    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral(
        "SELECT started_at, planned_seconds, focused_seconds, completed, topic FROM focus_sessions"
        " ORDER BY started_at DESC, id DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec())
        return sessions;
    while (query.next()) {
        FocusSession session;
        session.startedAt = QDateTime::fromMSecsSinceEpoch(query.value(0).toLongLong());
        session.plannedSeconds = query.value(1).toInt();
        session.focusedSeconds = query.value(2).toInt();
        session.completed = query.value(3).toInt() != 0;
        session.topic = query.value(4).toString();
        sessions << session;
    }
    return sessions;
}

FocusDaySummary FocusLog::summaryFor(const QDate &day) const
{
    FocusDaySummary summary;
    if (!Database::isOpen())
        return summary;

    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(completed), 0), COALESCE(SUM(1 - completed), 0),"
        " COALESCE(SUM(focused_seconds), 0) FROM focus_sessions WHERE day = ?"));
    query.addBindValue(day.toString(Qt::ISODate));
    if (query.exec() && query.next()) {
        summary.completed = query.value(0).toInt();
        summary.stoppedEarly = query.value(1).toInt();
        summary.focusedSeconds = query.value(2).toInt();
    }
    return summary;
}
