#include "core/docsstore.h"

#include "core/database.h"

#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

void warn(const char *what, const QSqlQuery &query)
{
    qWarning("Deskout: could not %s: %s", what, qPrintable(query.lastError().text()));
}

DocSummary summaryFrom(const QSqlQuery &query)
{
    DocSummary doc;
    doc.id = query.value(0).toLongLong();
    doc.title = query.value(1).toString();
    doc.wordCount = query.value(2).toInt();
    doc.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong());
    doc.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong());
    return doc;
}

} // namespace

DocsStore::DocsStore(QObject *parent, Clock clock)
    : QObject(parent)
    , m_now(std::move(clock))
{
    if (!m_now)
        m_now = [] { return QDateTime::currentDateTime(); };
}

QList<DocSummary> DocsStore::list(const QString &filter) const
{
    QList<DocSummary> docs;
    if (!Database::isOpen())
        return docs;

    const QString needle = filter.trimmed();
    QString sql = QStringLiteral("SELECT id, title, word_count, created_at, updated_at FROM docs");
    if (!needle.isEmpty())
        sql += QStringLiteral(" WHERE title LIKE ? ESCAPE '\\' OR plain_text LIKE ? ESCAPE '\\'");
    sql += QStringLiteral(" ORDER BY updated_at DESC, id DESC");
    QSqlQuery query(Database::connection());
    query.prepare(sql);
    if (!needle.isEmpty()) {
        query.addBindValue(Database::likePattern(needle));
        query.addBindValue(Database::likePattern(needle));
    }
    if (!query.exec()) {
        warn("list documents", query);
        return docs;
    }
    while (query.next())
        docs << summaryFrom(query);
    return docs;
}

std::optional<Doc> DocsStore::get(qint64 id) const
{
    if (!Database::isOpen())
        return std::nullopt;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral(
        "SELECT title, html, plain_text, word_count, created_at, updated_at FROM docs WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    Doc doc;
    doc.id = id;
    doc.title = query.value(0).toString();
    doc.html = query.value(1).toString();
    doc.plainText = query.value(2).toString();
    doc.wordCount = query.value(3).toInt();
    doc.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong());
    doc.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(5).toLongLong());
    return doc;
}

int DocsStore::count() const
{
    if (!Database::isOpen())
        return 0;
    QSqlQuery query(Database::connection());
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM docs")) && query.next())
        return query.value(0).toInt();
    return 0;
}

qint64 DocsStore::create(const QString &title)
{
    if (!Database::isOpen())
        return 0;
    const QString trimmed = title.simplified();
    const QString name = trimmed.isEmpty() ? uniqueTitle(tr("Untitled document")) : trimmed;
    const qint64 now = m_now().toMSecsSinceEpoch();
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("INSERT INTO docs (title, created_at, updated_at) VALUES (?, ?, ?)"));
    query.addBindValue(name);
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        warn("create document", query);
        return 0;
    }
    const qint64 id = query.lastInsertId().toLongLong();
    Q_EMIT docChanged(id);
    return id;
}

bool DocsStore::rename(qint64 id, const QString &title)
{
    const QString trimmed = title.simplified();
    if (trimmed.isEmpty() || !Database::isOpen())
        return false;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("UPDATE docs SET title = ? WHERE id = ?"));
    query.addBindValue(trimmed);
    query.addBindValue(id);
    if (!query.exec()) {
        warn("rename document", query);
        return false;
    }
    if (query.numRowsAffected() != 1)
        return false;
    Q_EMIT docChanged(id);
    return true;
}

bool DocsStore::saveContent(qint64 id, const QString &html, const QString &plainText)
{
    if (!Database::isOpen())
        return false;
    QSqlQuery query(Database::connection());
    // COALESCE: a null QString binds as NULL, which the columns reject.
    query.prepare(QStringLiteral("UPDATE docs SET html = COALESCE(?, ''), plain_text = COALESCE(?, ''),"
                                 " word_count = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(html);
    query.addBindValue(plainText);
    query.addBindValue(countWords(plainText));
    query.addBindValue(m_now().toMSecsSinceEpoch());
    query.addBindValue(id);
    if (!query.exec()) {
        warn("save document", query);
        return false;
    }
    if (query.numRowsAffected() != 1)
        return false;
    Q_EMIT docChanged(id);
    return true;
}

bool DocsStore::remove(qint64 id)
{
    if (!Database::isOpen())
        return false;
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("DELETE FROM docs WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        warn("delete document", query);
        return false;
    }
    if (query.numRowsAffected() != 1)
        return false;
    Q_EMIT docRemoved(id);
    return true;
}

QString DocsStore::uniqueTitle(const QString &base) const
{
    QSet<QString> taken;
    if (Database::isOpen()) {
        QSqlQuery query(Database::connection());
        query.prepare(QStringLiteral("SELECT title FROM docs WHERE title = ? OR title LIKE ? ESCAPE '\\'"));
        query.addBindValue(base);
        // "base %": titles that start with "base ".
        query.addBindValue(Database::likePattern(base + QLatin1Char(' ')).mid(1));
        if (query.exec()) {
            while (query.next())
                taken.insert(query.value(0).toString());
        }
    }
    if (!taken.contains(base))
        return base;
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 %2").arg(base).arg(n);
        if (!taken.contains(candidate))
            return candidate;
    }
}

int DocsStore::countWords(const QString &text)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    return int(text.split(whitespace, Qt::SkipEmptyParts).size());
}
