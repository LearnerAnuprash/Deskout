#include "core/database.h"
#include "core/docsstore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestDocsStore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QDateTime m_now;

    DocsStore::Clock clock()
    {
        return [this] { return m_now; };
    }

    static QStringList titles(const QList<DocSummary> &docs)
    {
        QStringList result;
        for (const DocSummary &doc : docs)
            result << doc.title;
        return result;
    }

private Q_SLOTS:
    void init()
    {
        QString error;
        QVERIFY2(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction())), &error),
                 qPrintable(error));
        m_now = QDateTime(QDate(2026, 9, 28), QTime(9, 0));
    }

    void cleanup()
    {
        Database::close();
    }

    void createNamesUntitledDocuments()
    {
        DocsStore store(nullptr, clock());
        QSignalSpy changed(&store, &DocsStore::docChanged);
        const qint64 a = store.create();
        const qint64 b = store.create(QStringLiteral("   "));
        const qint64 c = store.create();
        QVERIFY(a > 0 && b > 0 && c > 0);
        QCOMPARE(store.get(a)->title, QStringLiteral("Untitled document"));
        QCOMPARE(store.get(b)->title, QStringLiteral("Untitled document 2"));
        QCOMPARE(store.get(c)->title, QStringLiteral("Untitled document 3"));
        QCOMPARE(changed.count(), 3);

        const qint64 named = store.create(QStringLiteral("  Qt   internals "));
        QCOMPARE(store.get(named)->title, QStringLiteral("Qt internals"));
        const std::optional<Doc> doc = store.get(named);
        QCOMPARE(doc->html, QString());
        QCOMPARE(doc->wordCount, 0);
        QCOMPARE(doc->createdAt, m_now);
    }

    void uniqueTitleOnlyCountsExactNames()
    {
        DocsStore store(nullptr, clock());
        store.create(QStringLiteral("Plan"));
        store.create(QStringLiteral("Plan 2"));
        store.create(QStringLiteral("Planning"));
        QCOMPARE(store.uniqueTitle(QStringLiteral("Plan")), QStringLiteral("Plan 3"));
        QCOMPARE(store.uniqueTitle(QStringLiteral("Plan_")), QStringLiteral("Plan_"));
        QCOMPARE(store.uniqueTitle(QStringLiteral("Other")), QStringLiteral("Other"));
    }

    void renameKeepsIdAndEditTime()
    {
        DocsStore store(nullptr, clock());
        const qint64 id = store.create(QStringLiteral("Draft"));
        m_now = m_now.addSecs(3600);
        QSignalSpy changed(&store, &DocsStore::docChanged);
        QVERIFY(store.rename(id, QStringLiteral("  Final report ")));
        QCOMPARE(changed.count(), 1);
        const std::optional<Doc> doc = store.get(id);
        QCOMPARE(doc->title, QStringLiteral("Final report"));
        QCOMPARE(doc->updatedAt, m_now.addSecs(-3600));

        QVERIFY(!store.rename(id, QStringLiteral("   ")));
        QCOMPARE(store.get(id)->title, QStringLiteral("Final report"));
        QVERIFY(!store.rename(9999, QStringLiteral("Nope")));
    }

    void saveContentUpdatesTextAndTime()
    {
        DocsStore store(nullptr, clock());
        const qint64 id = store.create(QStringLiteral("Doc"));
        m_now = m_now.addSecs(120);
        QVERIFY(store.saveContent(id, QStringLiteral("<p><b>Hello</b> world</p>"),
                                  QStringLiteral("Hello world\n  again ")));
        const std::optional<Doc> doc = store.get(id);
        QCOMPARE(doc->html, QStringLiteral("<p><b>Hello</b> world</p>"));
        QCOMPARE(doc->plainText, QStringLiteral("Hello world\n  again "));
        QCOMPARE(doc->wordCount, 3);
        QCOMPARE(doc->updatedAt, m_now);
        QVERIFY(store.saveContent(id, QString(), QString()));
        QCOMPARE(store.get(id)->wordCount, 0);
        QVERIFY(!store.saveContent(9999, QString(), QString()));
    }

    void listOrderAndFilter()
    {
        DocsStore store(nullptr, clock());
        const qint64 older = store.create(QStringLiteral("Architecture"));
        m_now = m_now.addSecs(60);
        const qint64 newer = store.create(QStringLiteral("Onboarding"));
        store.saveContent(newer, QStringLiteral("<span style=\"color:red\">Welcome</span>"),
                          QStringLiteral("Welcome aboard"));
        QCOMPARE(titles(store.list()), (QStringList{QStringLiteral("Onboarding"), QStringLiteral("Architecture")}));

        m_now = m_now.addSecs(60);
        store.saveContent(older, QStringLiteral("<p>layers</p>"), QStringLiteral("Layers and modules"));
        QCOMPARE(titles(store.list()), (QStringList{QStringLiteral("Architecture"), QStringLiteral("Onboarding")}));

        QCOMPARE(titles(store.list(QStringLiteral("welcome"))), QStringList{QStringLiteral("Onboarding")});
        QCOMPARE(titles(store.list(QStringLiteral("archi"))), QStringList{QStringLiteral("Architecture")});
        // Markup isn't searched, only the text.
        QVERIFY(store.list(QStringLiteral("span")).isEmpty());
        QCOMPARE(store.list().constFirst().wordCount, 3);
    }

    void removeDeletes()
    {
        DocsStore store(nullptr, clock());
        const qint64 id = store.create();
        QSignalSpy removed(&store, &DocsStore::docRemoved);
        QVERIFY(store.remove(id));
        QCOMPARE(removed.count(), 1);
        QVERIFY(!store.get(id));
        QVERIFY(!store.remove(id));
        QCOMPARE(store.count(), 0);
    }

    void countWords()
    {
        QCOMPARE(DocsStore::countWords(QString()), 0);
        QCOMPARE(DocsStore::countWords(QStringLiteral("  one\ttwo\n\nthree  ")), 3);
        QCOMPARE(DocsStore::countWords(QStringLiteral("don't stop-me")), 2);
    }

    void unavailableDatabaseIsHarmless()
    {
        Database::close();
        DocsStore store(nullptr, clock());
        QCOMPARE(store.create(), qint64(0));
        QVERIFY(store.list().isEmpty());
        QVERIFY(!store.rename(1, QStringLiteral("x")));
        QCOMPARE(store.uniqueTitle(QStringLiteral("x")), QStringLiteral("x"));
    }
};

QTEST_GUILESS_MAIN(TestDocsStore)
#include "tst_docsstore.moc"
