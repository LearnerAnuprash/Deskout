#include "app/statsrecorder.h"
#include "core/database.h"
#include "core/focustimer.h"
#include "core/reminderengine.h"
#include "core/stats.h"

#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
constexpr qint64 Minute = 60 * 1000;
const QString Eye = QLatin1String(ReminderIds::Eye); // every 20 min by default
} // namespace

// The real reminder engine and focus timer drive the recorder.
class TestStatsRecorder : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QDateTime m_now;

    ReminderEngine::Hooks hooks()
    {
        return {[] { return false; }, [] { return false; }, [this] { return m_now; }};
    }

    StatsRecorder::Today today()
    {
        return [this] { return m_now.date(); };
    }

    void run(ReminderEngine &engine, int minutes)
    {
        for (int i = 0; i < minutes; ++i) {
            m_now = m_now.addSecs(60);
            engine.advance(Minute);
        }
    }

    int shown(const StatsStore &stats) const { return stats.value(m_now.date(), StatsMetric::reminderShown(Eye)); }
    int taken(const StatsStore &stats) const { return stats.value(m_now.date(), StatsMetric::reminderTaken(Eye)); }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_statsrecorder"));
    }

    void init()
    {
        QSettings().clear();
        QVERIFY(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction()))));
        m_now = QDateTime(QDate(2026, 9, 28), QTime(10, 0)); // Monday, inside 9-18
    }

    void cleanup()
    {
        Database::close();
    }

    void confirmedBreakCounts()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        FocusTimer focus;
        StatsStore stats;
        StatsRecorder recorder(&engine, &focus, &stats, nullptr, today());

        run(engine, 20);
        QCOMPARE(shown(stats), 1);
        QCOMPARE(taken(stats), 0);
        engine.confirm(Eye);
        QCOMPARE(taken(stats), 1);
    }

    void snoozeIsTheSameBreak()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        FocusTimer focus;
        StatsStore stats;
        StatsRecorder recorder(&engine, &focus, &stats, nullptr, today());

        run(engine, 20);
        engine.snooze(Eye, 5);
        run(engine, 5); // due again
        QCOMPARE(engine.status(Eye).state, ReminderState::Due);
        QCOMPARE(shown(stats), 1);
        engine.confirm(Eye);
        QCOMPARE(taken(stats), 1);
    }

    void unansweredNotificationIsMissedUntilConfirmed()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        FocusTimer focus;
        StatsStore stats;
        StatsRecorder recorder(&engine, &focus, &stats, nullptr, today());

        run(engine, 20);
        engine.acknowledge(Eye); // shown as a notification
        QCOMPARE(shown(stats), 1);
        QCOMPARE(taken(stats), 0);
        engine.confirm(Eye); // "I took the break" on the notification later
        QCOMPARE(taken(stats), 1);
    }

    void testNowIsIgnored()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        FocusTimer focus;
        StatsStore stats;
        StatsRecorder recorder(&engine, &focus, &stats, nullptr, today());

        engine.triggerNow(Eye);
        engine.snooze(Eye, 5);
        run(engine, 5);
        engine.confirm(Eye);
        QCOMPARE(shown(stats), 0);
        QCOMPARE(taken(stats), 0);

        // The next real one counts.
        run(engine, 20);
        QCOMPARE(shown(stats), 1);
    }

    void snoozeFromYesterdayDoesNotHideToday()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        FocusTimer focus;
        StatsStore stats;
        StatsRecorder recorder(&engine, &focus, &stats, nullptr, today());

        run(engine, 20);
        engine.snooze(Eye, 5);
        // Next morning, a fresh active period.
        m_now = QDateTime(m_now.date().addDays(1), QTime(9, 0));
        engine.advance(1000);
        run(engine, 20);
        QCOMPARE(shown(stats), 1);
    }

    void focusSessionsCount()
    {
        ReminderEngine engine(hooks());
        FocusTimer focus(nullptr, [this] { return m_now; });
        StatsStore stats;
        StatsRecorder recorder(&engine, &focus, &stats, nullptr, today());

        focus.start(25, QString());
        focus.advance(25 * Minute);
        focus.start(25, QString());
        focus.advance(10 * Minute);
        focus.stop(); // stopped early: time counts, session doesn't
        focus.start(25, QString());
        focus.stop(); // under a minute: ignored

        QCOMPARE(stats.value(m_now.date(), QLatin1String(StatsMetric::FocusCompleted)), 1);
        const int seconds = stats.value(m_now.date(), QLatin1String(StatsMetric::FocusSeconds));
        QVERIFY(seconds >= 35 * 60 && seconds < 35 * 60 + 5);
    }
};

QTEST_GUILESS_MAIN(TestStatsRecorder)
#include "tst_statsrecorder.moc"
