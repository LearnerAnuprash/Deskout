#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>

#include <functional>
#include <optional>

struct Note
{
    qint64 id = 0;
    QString title;
    QString body;
    QDateTime createdAt;
    QDateTime updatedAt;
};

// What the notes list needs; the body is cut to a preview.
struct NoteSummary
{
    qint64 id = 0;
    QString title;
    QString preview;
    QDateTime updatedAt;
};

// Notes in the app database. Every change is written straight away and
// announced, so any view (list, search, export) stays in step.
class NotesStore : public QObject
{
    Q_OBJECT

public:
    // Characters of the body returned as NoteSummary::preview.
    static constexpr int PreviewLength = 300;

    using Clock = std::function<QDateTime()>;

    explicit NotesStore(QObject *parent = nullptr, Clock clock = {});

    // Most recently edited first. A non-empty filter keeps notes whose
    // title or body contains it.
    QList<NoteSummary> list(const QString &filter = QString()) const;
    std::optional<Note> get(qint64 id) const;
    int count() const;

    // Returns the new note's id, or 0 on failure.
    qint64 create(const QString &title, const QString &body);
    // False if the note doesn't exist or can't be written.
    bool update(qint64 id, const QString &title, const QString &body);
    bool remove(qint64 id);

    // Title to show for a note: its title, else the first line of the body.
    static QString displayTitle(const QString &title, const QString &body);
    // One-line preview: the body, minus the line used as the display title.
    static QString displayPreview(const QString &title, const QString &body);

Q_SIGNALS:
    void noteSaved(qint64 id);
    void noteRemoved(qint64 id);

private:
    Clock m_now;
};
