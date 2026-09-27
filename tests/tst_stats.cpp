#include "core/database.h"
#include "core/stats.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {

DayAdherence day(int shown, int taken)
{
    DayAdherence d;
    d.shown = shown;
    d.taken = taken;
    return d;
}

} // namespace

class TestStats : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    const QDate m_today{2026, 9, 30}; // Wednesday

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_stats"));
    }

    void init()
    {
        QSettings().clear();
        QString error;
        QVERIFY2(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction())), &error),
                 qPrintable(error));
    }

    void cleanup()
    {
        Database::close();
    }

    void addAccumulatesPerDay()
    {
        StatsStore stats;
        QSignalSpy changed(&stats, &StatsStore::changed);
        const QString eyeShown = StatsMetric::reminderShown(QStringLiteral("eye"));
        QVERIFY(stats.add(m_today, eyeShown));
        QVERIFY(stats.add(m_today, eyeShown));
        QVERIFY(stats.add(m_today, QLatin1String(StatsMetric::FocusSeconds), 1500));
        QVERIFY(stats.add(m_today.addDays(-1), eyeShown, 5));
        QCOMPARE(stats.value(m_today, eyeShown), 2);
        QCOMPARE(stats.value(m_today.addDays(-1), eyeShown), 5);
        QCOMPARE(stats.value(m_today, QLatin1String(StatsMetric::FocusSeconds)), 1500);
        QCOMPARE(stats.value(m_today.addDays(1), eyeShown), 0);
        QCOMPARE(changed.count(), 4);
        QVERIFY(!stats.add(m_today, eyeShown, 0));
    }

    void adherenceSumsReminderTypes()
    {
        StatsStore stats;
        stats.add(m_today, StatsMetric::reminderShown(QStringLiteral("eye")), 6);
        stats.add(m_today, StatsMetric::reminderTaken(QStringLiteral("eye")), 4);
        stats.add(m_today, StatsMetric::reminderShown(QStringLiteral("water")), 4);
        stats.add(m_today, StatsMetric::reminderTaken(QStringLiteral("water")), 4);
        stats.add(m_today, QLatin1String(StatsMetric::FocusCompleted), 3); // not a reminder
        stats.add(m_today.addDays(-10), StatsMetric::reminderShown(QStringLiteral("eye")), 1);

        const QMap<QDate, DayAdherence> days = stats.adherence(m_today.addDays(-6), m_today);
        QCOMPARE(days.size(), 1);
        QCOMPARE(days.value(m_today).shown, 10);
        QCOMPARE(days.value(m_today).taken, 8);
        QCOMPARE(days.value(m_today).percent(), 80);
        QVERIFY(days.value(m_today).meets(80));
        QVERIFY(!days.value(m_today).meets(81));
    }

    void percentIsCapped()
    {
        QCOMPARE(day(2, 3).percent(), 100);
        QCOMPARE(day(0, 1).percent(), 0);
        QVERIFY(!day(0, 1).isActive());
        QCOMPARE(day(3, 1).percent(), 33);
    }

    void streakCountsBackFromToday()
    {
        QMap<QDate, DayAdherence> days;
        days.insert(m_today.addDays(-5), day(10, 2)); // missed
        days.insert(m_today.addDays(-4), day(10, 9));
        days.insert(m_today.addDays(-3), day(10, 8));
        // -2: no reminders (weekend) - skipped, doesn't break
        days.insert(m_today.addDays(-2), day(0, 0));
        days.insert(m_today.addDays(-1), day(5, 5));
        days.insert(m_today, day(4, 4));
        const Streak streak = StatsStore::computeStreak(days, m_today, 80);
        QCOMPARE(streak.current, 4);
        QCOMPARE(streak.best, 4);
    }

    void unfinishedTodayDoesNotBreak()
    {
        QMap<QDate, DayAdherence> days;
        days.insert(m_today.addDays(-2), day(10, 10));
        days.insert(m_today.addDays(-1), day(10, 9));
        days.insert(m_today, day(3, 1)); // only a third so far
        QCOMPARE(StatsStore::computeStreak(days, m_today, 80).current, 2);
        days.insert(m_today, day(3, 3));
        QCOMPARE(StatsStore::computeStreak(days, m_today, 80).current, 3);
    }

    void missedDayResetsAndBestRemembers()
    {
        QMap<QDate, DayAdherence> days;
        for (int i = 10; i >= 5; --i)
            days.insert(m_today.addDays(-i), day(4, 4)); // six good days
        days.insert(m_today.addDays(-4), day(4, 1));     // missed
        days.insert(m_today.addDays(-3), day(4, 4));
        days.insert(m_today.addDays(-1), day(4, 4));
        const Streak streak = StatsStore::computeStreak(days, m_today, 80);
        QCOMPARE(streak.current, 2);
        QCOMPARE(streak.best, 6);

        // Future rows (clock changes) are ignored.
        days.insert(m_today.addDays(3), day(4, 4));
        QCOMPARE(StatsStore::computeStreak(days, m_today, 80).current, 2);
        QCOMPARE(StatsStore::computeStreak({}, m_today, 80).current, 0);
    }

    void streakFromDatabase()
    {
        StatsStore stats;
        for (int i = 0; i < 3; ++i) {
            stats.add(m_today.addDays(-i), StatsMetric::reminderShown(QStringLiteral("eye")), 5);
            stats.add(m_today.addDays(-i), StatsMetric::reminderTaken(QStringLiteral("eye")), 4);
        }
        QCOMPARE(stats.streak(m_today, 80).current, 3);
        QCOMPARE(stats.streak(m_today, 90).current, 0);
    }

    void thresholdSetting()
    {
        QCOMPARE(StatsStore::threshold(), StatsStore::DefaultThreshold);
        StatsStore::setThreshold(65);
        QCOMPARE(StatsStore::threshold(), 65);
        StatsStore::setThreshold(500);
        QCOMPARE(StatsStore::threshold(), 100);
    }
};

QTEST_GUILESS_MAIN(TestStats)
#include "tst_stats.moc"
