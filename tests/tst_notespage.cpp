#include "core/database.h"
#include "core/notesstore.h"
#include "ui/notespage.h"

#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>

// The editor's save behaviour: autosave after typing, nothing stored for
// notes left empty, list kept in step.
class TestNotesPage : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    template<typename T>
    static T *child(QWidget *page, const char *name)
    {
        T *widget = page->findChild<T *>(QLatin1String(name));
        Q_ASSERT(widget);
        return widget;
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction()))));
    }

    void cleanup()
    {
        Database::close();
    }

    void autosavesAfterTyping()
    {
        NotesStore store;
        NotesPage page(&store);
        page.show();
        page.newNote();
        QTest::keyClicks(child<QLineEdit>(&page, "NoteTitle"), QStringLiteral("Standup"));
        QTest::keyClicks(child<QPlainTextEdit>(&page, "NoteBody"), QStringLiteral("Ship phase 4"));
        QCOMPARE(store.count(), 0); // not yet: waits for a pause in typing

        QTRY_COMPARE_WITH_TIMEOUT(store.count(), 1, 3000);
        const QList<NoteSummary> notes = store.list();
        QCOMPARE(notes.constFirst().title, QStringLiteral("Standup"));
        QCOMPARE(notes.constFirst().preview, QStringLiteral("Ship phase 4"));

        auto *list = child<QListWidget>(&page, "NotesList");
        QTRY_COMPARE(list->count(), 1);
        QCOMPARE(list->currentRow(), 0);

        // Further edits update the same note.
        QTest::keyClicks(child<QPlainTextEdit>(&page, "NoteBody"), QStringLiteral(" today"));
        page.save();
        QCOMPARE(store.count(), 1);
        QCOMPARE(store.get(notes.constFirst().id)->body, QStringLiteral("Ship phase 4 today"));
    }

    void blankNewNoteIsNotStored()
    {
        NotesStore store;
        NotesPage page(&store);
        page.show();
        page.newNote();
        page.save();
        page.newNote();
        QCOMPARE(store.count(), 0);
    }

    void switchingNotesSavesAndLoads()
    {
        NotesStore store;
        const qint64 first = store.create(QStringLiteral("First"), QStringLiteral("one"));
        const qint64 second = store.create(QStringLiteral("Second"), QStringLiteral("two"));
        NotesPage page(&store);
        page.show();

        page.openNote(first);
        auto *title = child<QLineEdit>(&page, "NoteTitle");
        auto *body = child<QPlainTextEdit>(&page, "NoteBody");
        QCOMPARE(title->text(), QStringLiteral("First"));
        body->moveCursor(QTextCursor::End);
        QTest::keyClicks(body, QStringLiteral(" edited"));

        // Leaving the note saves it straight away, without waiting.
        page.openNote(second);
        QCOMPARE(store.get(first)->body, QStringLiteral("one edited"));
        QCOMPARE(title->text(), QStringLiteral("Second"));
        QCOMPARE(body->toPlainText(), QStringLiteral("two"));
    }

    void emptiedNoteIsDroppedOnLeave()
    {
        NotesStore store;
        const qint64 keep = store.create(QStringLiteral("Keep"), QString());
        const qint64 empty = store.create(QStringLiteral("x"), QString());
        NotesPage page(&store);
        page.show();

        page.openNote(empty);
        auto *title = child<QLineEdit>(&page, "NoteTitle");
        title->selectAll();
        QTest::keyClick(title, Qt::Key_Backspace);
        page.openNote(keep);
        QVERIFY(!store.get(empty));
        QVERIFY(store.get(keep));
        QTRY_COMPARE(child<QListWidget>(&page, "NotesList")->count(), 1);
    }

    void clickingListOpensNote()
    {
        NotesStore store;
        store.create(QStringLiteral("Older"), QStringLiteral("a"));
        store.create(QStringLiteral("Newer"), QStringLiteral("b"));
        NotesPage page(&store);
        page.show();

        auto *list = child<QListWidget>(&page, "NotesList");
        QCOMPARE(list->count(), 2);
        list->setCurrentRow(1);
        QCOMPARE(child<QLineEdit>(&page, "NoteTitle")->text(), QStringLiteral("Older"));
    }
};

QTEST_MAIN(TestNotesPage)
#include "tst_notespage.moc"
