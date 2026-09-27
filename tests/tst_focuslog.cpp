#include "core/database.h"
#include "core/focuslog.h"

#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

namespace {

FocusSession session(const QDateTime &start, int minutes, bool completed, const QString &topic = QString())
{
    FocusSession s;
    s.startedAt = start;
    s.plannedSeconds = 25 * 60;
    s.focusedSeconds = minutes * 60;
    s.completed = completed;
    s.topic = topic;
    return s;
}

} // namespace

class TestFocusLog : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_path;
    const QDate m_day{2026, 9, 28};

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction()));
        QString error;
        QVERIFY2(Database::open(m_path, &error), qPrintable(error));
    }

    void cleanup()
    {
        Database::close();
    }

    void createsSchema()
    {
        QSqlQuery query(Database::connection());
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")) && query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM focus_sessions")) && query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void recordsAndSummarises()
    {
        FocusLog log;
        QSignalSpy changed(&log, &FocusLog::changed);
        QVERIFY(log.record(session(QDateTime(m_day, QTime(9, 0)), 25, true, QStringLiteral("Qt SQL"))));
        QVERIFY(log.record(session(QDateTime(m_day, QTime(10, 0)), 25, true)));
        QVERIFY(log.record(session(QDateTime(m_day, QTime(11, 0)), 10, false)));
        QVERIFY(log.record(session(QDateTime(m_day.addDays(-1), QTime(16, 0)), 25, true)));
        QCOMPARE(changed.count(), 4);

        const FocusDaySummary today = log.summaryFor(m_day);
        QCOMPARE(today.completed, 2);
        QCOMPARE(today.stoppedEarly, 1);
        QCOMPARE(today.focusedSeconds, 60 * 60);
        QCOMPARE(log.summaryFor(m_day.addDays(-1)).completed, 1);
        QCOMPARE(log.summaryFor(m_day.addDays(1)).completed, 0);
    }

    void skipsShortAbandonedSessions()
    {
        FocusLog log;
        FocusSession tooShort = session(QDateTime(m_day, QTime(9, 0)), 0, false);
        tooShort.focusedSeconds = FocusLog::MinAbandonedSeconds - 1;
        QVERIFY(!log.record(tooShort));
        QCOMPARE(log.recent(10).size(), 0);

        // A completed session always counts, however short.
        FocusSession shortComplete = session(QDateTime(m_day, QTime(9, 5)), 0, true);
        shortComplete.plannedSeconds = shortComplete.focusedSeconds = 60;
        QVERIFY(log.record(shortComplete));
    }

    void recentIsNewestFirst()
    {
        FocusLog log;
        log.record(session(QDateTime(m_day, QTime(9, 0)), 25, true, QStringLiteral("first")));
        log.record(session(QDateTime(m_day, QTime(14, 0)), 25, true, QStringLiteral("third")));
        log.record(session(QDateTime(m_day, QTime(11, 0)), 5, false, QStringLiteral("second")));

        const QList<FocusSession> recent = log.recent(2);
        QCOMPARE(recent.size(), 2);
        QCOMPARE(recent[0].topic, QStringLiteral("third"));
        QCOMPARE(recent[1].topic, QStringLiteral("second"));
        QVERIFY(!recent[1].completed);
        QCOMPARE(recent[1].focusedSeconds, 5 * 60);
        QCOMPARE(recent[0].startedAt, QDateTime(m_day, QTime(14, 0)));
    }

    void survivesReopen()
    {
        FocusLog log;
        log.record(session(QDateTime(m_day, QTime(9, 0)), 25, true));
        Database::close();
        QVERIFY(Database::open(m_path));
        QCOMPARE(log.summaryFor(m_day).completed, 1);
    }

    void unavailableDatabaseIsHarmless()
    {
        Database::close();
        FocusLog log;
        QVERIFY(!log.record(session(QDateTime(m_day, QTime(9, 0)), 25, true)));
        QCOMPARE(log.recent(5).size(), 0);
        QCOMPARE(log.summaryFor(m_day).completed, 0);
    }

    void refusesNewerSchema()
    {
        QSqlQuery(Database::connection()).exec(QStringLiteral("PRAGMA user_version = 99"));
        Database::close();
        QString error;
        QVERIFY(!Database::open(m_path, &error));
        QVERIFY(error.contains(QLatin1String("newer version")));
        QVERIFY(!Database::isOpen());
    }
};

QTEST_GUILESS_MAIN(TestFocusLog)
#include "tst_focuslog.moc"
