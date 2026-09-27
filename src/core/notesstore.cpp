#include "core/notesstore.h"

#include "core/database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

QString firstLine(const QString &text, QString *rest = nullptr)
{
    const QString trimmed = text.trimmed();
    const qsizetype newline = trimmed.indexOf(QLatin1Char('\n'));
    if (rest)
        *rest = newline < 0 ? QString() : trimmed.mid(newline + 1);
    return (newline < 0 ? trimmed : trimmed.left(newline)).trimmed();
}

void warn(const char *what, const QSqlQuery &query)
{
    qWarning("Deskout: could not %s: %s", what, qPrintable(query.lastError().text()));
}

} // namespace

NotesStore::NotesStore(QObject *parent, Clock clock)
    : QObject(parent)
    , m_now(std::move(clock))
{
    if (!m_now)
        m_now = [] { return QDateTime::currentDateTime(); };
}

QList<NoteSummary> NotesStore::list(const QString &filter) const
{
    QList<NoteSummary> notes;
    if (!Database::isOpen())
        return notes;

    QSqlQuery query(Database::connection());
    const QString needle = filter.trimmed();
    QString sql = QStringLiteral("SELECT id, title, substr(body, 1, %1), updated_at FROM notes").arg(PreviewLength);
    if (!needle.isEmpty())
        sql += QStringLiteral(" WHERE title LIKE ? ESCAPE '\\' OR body LIKE ? ESCAPE '\\'");
    sql += QStringLiteral(" ORDER BY updated_at DESC, id DESC");
    query.prepare(sql);
    if (!needle.isEmpty()) {
        query.addBindValue(Database::likePattern(needle));
        query.addBindValue(Database::likePattern(needle));
    }
    if (!query.exec()) {
        warn("list notes", query);
        return notes;
    }
    while (query.next()) {
        NoteSummary note;
        note.id = query.value(0).toLongLong();
        note.title = query.value(1).toString();
        note.preview = query.value(2).toString();
        note.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong());
        notes << note;
    }
    return notes;
}

std::optional<Note> NotesStore::get(qint64 id) const
{
    if (!Database::isOpen())
        return std::nullopt;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("SELECT title, body, created_at, updated_at FROM notes WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    Note note;
    note.id = id;
    note.title = query.value(0).toString();
    note.body = query.value(1).toString();
    note.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong());
    note.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong());
    return note;
}

int NotesStore::count() const
{
    if (!Database::isOpen())
        return 0;
    QSqlQuery query(Database::connection());
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM notes")) && query.next())
        return query.value(0).toInt();
    return 0;
}

qint64 NotesStore::create(const QString &title, const QString &body)
{
    if (!Database::isOpen())
        return 0;
    const qint64 now = m_now().toMSecsSinceEpoch();
    QSqlQuery query(Database::connection());
    // COALESCE: a null QString binds as NULL, which the columns reject.
    query.prepare(QStringLiteral("INSERT INTO notes (title, body, created_at, updated_at)"
                                 " VALUES (COALESCE(?, ''), COALESCE(?, ''), ?, ?)"));
    query.addBindValue(title);
    query.addBindValue(body);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        warn("create note", query);
        return 0;
    }
    const qint64 id = query.lastInsertId().toLongLong();
    Q_EMIT noteSaved(id);
    return id;
}

bool NotesStore::update(qint64 id, const QString &title, const QString &body)
{
    if (!Database::isOpen())
        return false;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("UPDATE notes SET title = COALESCE(?, ''), body = COALESCE(?, ''),"
                                 " updated_at = ? WHERE id = ?"));
    query.addBindValue(title);
    query.addBindValue(body);
    query.addBindValue(m_now().toMSecsSinceEpoch());
    query.addBindValue(id);
    if (!query.exec()) {
        warn("save note", query);
        return false;
    }
    if (query.numRowsAffected() != 1)
        return false;
    Q_EMIT noteSaved(id);
    return true;
}

bool NotesStore::remove(qint64 id)
{
    if (!Database::isOpen())
        return false;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("DELETE FROM notes WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        warn("delete note", query);
        return false;
    }
    if (query.numRowsAffected() != 1)
        return false;
    Q_EMIT noteRemoved(id);
    return true;
}

QString NotesStore::displayTitle(const QString &title, const QString &body)
{
    const QString trimmed = title.simplified();
    if (!trimmed.isEmpty())
        return trimmed;
    const QString line = firstLine(body);
    return line.isEmpty() ? tr("Untitled note") : line;
}

QString NotesStore::displayPreview(const QString &title, const QString &body)
{
    if (!title.simplified().isEmpty())
        return body.simplified();
    QString rest;
    firstLine(body, &rest);
    return rest.simplified();
}
