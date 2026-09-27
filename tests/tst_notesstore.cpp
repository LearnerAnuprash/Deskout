#include "core/database.h"
#include "core/notesstore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestNotesStore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QDateTime m_now;

    NotesStore::Clock clock()
    {
        return [this] { return m_now; };
    }

    void tick(int minutes = 1)
    {
        m_now = m_now.addSecs(minutes * 60);
    }

    static QStringList titles(const QList<NoteSummary> &notes)
    {
        QStringList result;
        for (const NoteSummary &note : notes)
            result << note.title;
        return result;
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QString error;
        QVERIFY2(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction())), &error),
                 qPrintable(error));
        m_now = QDateTime(QDate(2026, 9, 28), QTime(9, 0));
    }

    void cleanup()
    {
        Database::close();
    }

    void createReadUpdateDelete()
    {
        NotesStore store(nullptr, clock());
        QSignalSpy saved(&store, &NotesStore::noteSaved);
        QSignalSpy removed(&store, &NotesStore::noteRemoved);

        const qint64 id = store.create(QStringLiteral("Groceries"), QStringLiteral("milk\neggs"));
        QVERIFY(id > 0);
        QCOMPARE(saved.count(), 1);
        QCOMPARE(saved[0][0].toLongLong(), id);

        std::optional<Note> note = store.get(id);
        QVERIFY(note);
        QCOMPARE(note->title, QStringLiteral("Groceries"));
        QCOMPARE(note->body, QStringLiteral("milk\neggs"));
        QCOMPARE(note->createdAt, m_now);
        QCOMPARE(note->updatedAt, m_now);

        tick(5);
        QVERIFY(store.update(id, QStringLiteral("Groceries"), QStringLiteral("milk\neggs\nbread")));
        note = store.get(id);
        QCOMPARE(note->body, QStringLiteral("milk\neggs\nbread"));
        QCOMPARE(note->createdAt, m_now.addSecs(-5 * 60));
        QCOMPARE(note->updatedAt, m_now);
        QCOMPARE(saved.count(), 2);

        QVERIFY(store.remove(id));
        QCOMPARE(removed.count(), 1);
        QVERIFY(!store.get(id));
        QCOMPARE(store.count(), 0);
    }

    void missingNotesAreReported()
    {
        NotesStore store(nullptr, clock());
        QSignalSpy saved(&store, &NotesStore::noteSaved);
        QSignalSpy removed(&store, &NotesStore::noteRemoved);
        QVERIFY(!store.update(42, QStringLiteral("x"), QStringLiteral("y")));
        QVERIFY(!store.remove(42));
        QVERIFY(!store.get(42));
        QCOMPARE(saved.count(), 0);
        QCOMPARE(removed.count(), 0);
    }

    void emptyAndNullTextIsStored()
    {
        NotesStore store(nullptr, clock());
        const qint64 id = store.create(QString(), QString());
        QVERIFY(id > 0);
        QVERIFY(store.update(id, QString(), QStringLiteral("body only")));
        QCOMPARE(store.get(id)->title, QString());
    }

    void listIsMostRecentlyEditedFirst()
    {
        NotesStore store(nullptr, clock());
        const qint64 a = store.create(QStringLiteral("A"), QString());
        tick();
        store.create(QStringLiteral("B"), QString());
        tick();
        store.create(QStringLiteral("C"), QString());
        QCOMPARE(titles(store.list()), (QStringList{QStringLiteral("C"), QStringLiteral("B"), QStringLiteral("A")}));

        tick();
        store.update(a, QStringLiteral("A"), QStringLiteral("edited"));
        QCOMPARE(titles(store.list()), (QStringList{QStringLiteral("A"), QStringLiteral("C"), QStringLiteral("B")}));
        QCOMPARE(store.list().constFirst().updatedAt, m_now);
    }

    void previewIsTruncated()
    {
        NotesStore store(nullptr, clock());
        store.create(QStringLiteral("Long"), QString(NotesStore::PreviewLength * 3, QLatin1Char('x')));
        QCOMPARE(store.list().constFirst().preview.size(), NotesStore::PreviewLength);
    }

    void filterMatchesTitleOrBody()
    {
        NotesStore store(nullptr, clock());
        store.create(QStringLiteral("Meeting notes"), QStringLiteral("Discuss the roadmap"));
        store.create(QStringLiteral("Recipes"), QStringLiteral("Pasta with basil"));
        store.create(QStringLiteral("100% done"), QStringLiteral("snake_case names"));

        QCOMPARE(titles(store.list(QStringLiteral("roadmap"))), QStringList{QStringLiteral("Meeting notes")});
        QCOMPARE(titles(store.list(QStringLiteral("RECIPE"))), QStringList{QStringLiteral("Recipes")});
        QCOMPARE(titles(store.list(QStringLiteral("  basil "))), QStringList{QStringLiteral("Recipes")});
        // Wildcard characters match literally.
        QCOMPARE(titles(store.list(QStringLiteral("0%"))), QStringList{QStringLiteral("100% done")});
        QCOMPARE(titles(store.list(QStringLiteral("e_c"))), QStringList{QStringLiteral("100% done")});
        QCOMPARE(store.list(QStringLiteral("%")).size(), 1);
        QVERIFY(store.list(QStringLiteral("nothing like this")).isEmpty());
        QCOMPARE(store.list(QString()).size(), 3);
    }

    void displayTitleFallsBackToFirstLine()
    {
        QCOMPARE(NotesStore::displayTitle(QStringLiteral("  Plan  "), QStringLiteral("body")), QStringLiteral("Plan"));
        QCOMPARE(NotesStore::displayTitle(QString(), QStringLiteral("\n  First line \nSecond\nThird")),
                 QStringLiteral("First line"));
        QCOMPARE(NotesStore::displayTitle(QString(), QStringLiteral("   ")), QStringLiteral("Untitled note"));

        QCOMPARE(NotesStore::displayPreview(QStringLiteral("Plan"), QStringLiteral("a\nb")), QStringLiteral("a b"));
        QCOMPARE(NotesStore::displayPreview(QString(), QStringLiteral("First\nSecond\n\nThird")),
                 QStringLiteral("Second Third"));
        QCOMPARE(NotesStore::displayPreview(QString(), QStringLiteral("Only line")), QString());
    }

    void unavailableDatabaseIsHarmless()
    {
        Database::close();
        NotesStore store(nullptr, clock());
        QCOMPARE(store.create(QStringLiteral("x"), QString()), qint64(0));
        QVERIFY(store.list().isEmpty());
        QCOMPARE(store.count(), 0);
        QVERIFY(!store.get(1));
    }
};

QTEST_GUILESS_MAIN(TestNotesStore)
#include "tst_notesstore.moc"
