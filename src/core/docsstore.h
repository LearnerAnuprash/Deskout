#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>

#include <functional>
#include <optional>

struct Doc
{
    qint64 id = 0;
    QString title;
    QString html;
    QString plainText;
    int wordCount = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

// What the document list needs, without the content.
struct DocSummary
{
    qint64 id = 0;
    QString title;
    int wordCount = 0;
    QDateTime createdAt;
    QDateTime updatedAt;
};

// Topic documents in the app database. A document is identified by its id;
// the title is just a label, so renaming is safe at any time.
class DocsStore : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<QDateTime()>;

    explicit DocsStore(QObject *parent = nullptr, Clock clock = {});

    // Most recently edited first. A non-empty filter keeps documents whose
    // title or text contains it.
    QList<DocSummary> list(const QString &filter = QString()) const;
    std::optional<Doc> get(qint64 id) const;
    int count() const;

    // Creates an empty document and returns its id (0 on failure). A blank
    // title becomes "Untitled document", numbered if taken.
    qint64 create(const QString &title = QString());
    // Blank titles are refused. Renaming doesn't count as an edit, so the
    // "last edited" time stays.
    bool rename(qint64 id, const QString &title);
    bool saveContent(qint64 id, const QString &html, const QString &plainText);
    bool remove(qint64 id);

    // `base`, or "base 2", "base 3"... whichever no document uses yet.
    QString uniqueTitle(const QString &base) const;
    static int countWords(const QString &text);

Q_SIGNALS:
    // Created, renamed or saved.
    void docChanged(qint64 id);
    void docRemoved(qint64 id);

private:
    Clock m_now;
};
