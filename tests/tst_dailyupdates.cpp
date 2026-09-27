#include "core/dailyupdates.h"
#include "core/database.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestDailyUpdates : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QDateTime m_now;
    const QDate m_monday{2026, 9, 28};

    DailyUpdatesStore::Clock clock()
    {
        return [this] { return m_now; };
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_dailyupdates"));
    }

    void init()
    {
        QSettings().clear();
        QString error;
        QVERIFY2(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction())), &error),
                 qPrintable(error));
        m_now = QDateTime(m_monday, QTime(17, 30));
    }

    void cleanup()
    {
        Database::close();
    }

    void saveCreatesThenUpdates()
    {
        DailyUpdatesStore store(nullptr, clock());
        QSignalSpy changed(&store, &DailyUpdatesStore::changed);
        QVERIFY(store.save(m_monday, QStringLiteral("Shipped notes"), QStringLiteral("Docs editor")));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed[0][0].toDate(), m_monday);

        const QDateTime created = m_now;
        m_now = m_now.addSecs(600);
        QVERIFY(store.save(m_monday, QStringLiteral("Shipped notes and tests"), QString()));
        const std::optional<DailyUpdate> update = store.get(m_monday);
        QVERIFY(update);
        QCOMPARE(update->done, QStringLiteral("Shipped notes and tests"));
        QCOMPARE(update->todo, QString());
        QCOMPARE(update->createdAt, created);
        QCOMPARE(update->updatedAt, m_now);
        QCOMPARE(store.count(), 1);
    }

    void blankSaveRemovesTheDay()
    {
        DailyUpdatesStore store(nullptr, clock());
        store.save(m_monday, QStringLiteral("x"), QString());
        QVERIFY(store.save(m_monday, QStringLiteral("  \n"), QString()));
        QVERIFY(!store.get(m_monday));
        QCOMPARE(store.count(), 0);
        QVERIFY(store.save(m_monday, QString(), QString())); // nothing to remove: fine
    }

    void historyIsNewestFirstAndPages()
    {
        DailyUpdatesStore store(nullptr, clock());
        for (int i = 0; i < 5; ++i)
            store.save(m_monday.addDays(-i), QStringLiteral("day %1").arg(i), QString());

        const QList<DailyUpdate> all = store.history(10);
        QCOMPARE(all.size(), 5);
        QCOMPARE(all.first().day, m_monday);
        QCOMPARE(all.last().day, m_monday.addDays(-4));

        // "Before" excludes that day itself: today stays out of the history.
        const QList<DailyUpdate> earlier = store.history(2, m_monday);
        QCOMPARE(earlier.size(), 2);
        QCOMPARE(earlier[0].day, m_monday.addDays(-1));
        QCOMPARE(earlier[1].day, m_monday.addDays(-2));
        const QList<DailyUpdate> next = store.history(2, earlier.last().day);
        QCOMPARE(next[0].day, m_monday.addDays(-3));
    }

    void latestBeforeSkipsGaps()
    {
        DailyUpdatesStore store(nullptr, clock());
        const QDate friday = m_monday.addDays(-3);
        store.save(friday, QStringLiteral("Friday work"), QStringLiteral("Monday todos"));
        store.save(m_monday, QStringLiteral("Monday work"), QString());

        const std::optional<DailyUpdate> last = store.latestBefore(m_monday);
        QVERIFY(last);
        QCOMPARE(last->day, friday);
        QVERIFY(!store.latestBefore(friday));
        // Across a year boundary too (dates compare as ISO text).
        store.save(QDate(2025, 12, 31), QStringLiteral("NYE"), QString());
        QCOMPARE(store.latestBefore(QDate(2026, 1, 2))->day, QDate(2025, 12, 31));
    }

    void recapShowsOncePerDay()
    {
        DailyUpdatesStore store(nullptr, clock());
        const QDate tuesday = m_monday.addDays(1);
        QVERIFY(!store.recapDue(tuesday)); // nothing written yet

        store.save(m_monday, QStringLiteral("Monday work"), QStringLiteral("Tuesday todos"));
        QVERIFY(!store.recapDue(m_monday)); // today's own entry isn't a recap
        std::optional<DailyUpdate> due = store.recapDue(tuesday);
        QVERIFY(due);
        QCOMPARE(due->todo, QStringLiteral("Tuesday todos"));

        DailyUpdatesStore::markRecapShown(tuesday);
        QVERIFY(!store.recapDue(tuesday));
        // Next day it's due again, still showing the last entry.
        QVERIFY(store.recapDue(tuesday.addDays(1)));
        QCOMPARE(store.recapDue(tuesday.addDays(1))->day, m_monday);
    }

    void recapCanBeSwitchedOff()
    {
        DailyUpdatesStore store(nullptr, clock());
        store.save(m_monday, QStringLiteral("x"), QString());
        QVERIFY(DailyUpdatesStore::recapEnabled());
        DailyUpdatesStore::setRecapEnabled(false);
        QVERIFY(!store.recapDue(m_monday.addDays(1)));
        DailyUpdatesStore::setRecapEnabled(true);
        QVERIFY(store.recapDue(m_monday.addDays(1)));
    }

    void unavailableDatabaseIsHarmless()
    {
        Database::close();
        DailyUpdatesStore store(nullptr, clock());
        QVERIFY(!store.save(m_monday, QStringLiteral("x"), QString()));
        QVERIFY(store.history(10).isEmpty());
        QVERIFY(!store.recapDue(m_monday));
    }
};

QTEST_GUILESS_MAIN(TestDailyUpdates)
#include "tst_dailyupdates.moc"
