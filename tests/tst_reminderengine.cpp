#include "core/reminderengine.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

namespace {
constexpr qint64 Minute = 60 * 1000;
const QString Eye = QLatin1String(ReminderIds::Eye);     // every 20 min
const QString Water = QLatin1String(ReminderIds::Water); // every 60 min
} // namespace

class TestReminderEngine : public QObject
{
    Q_OBJECT

private:
    bool m_paused = false;
    bool m_idle = false;
    QDateTime m_now;

    ReminderEngine::Hooks hooks()
    {
        return {[this] { return m_paused; }, [this] { return m_idle; }, [this] { return m_now; }};
    }

    // Advance wall clock and engine together, one minute at a time.
    void run(ReminderEngine &engine, int minutes)
    {
        for (int i = 0; i < minutes; ++i) {
            m_now = m_now.addSecs(60);
            engine.advance(Minute);
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_reminderengine"));
    }

    void init()
    {
        QSettings().clear();
        m_paused = false;
        m_idle = false;
        m_now = QDateTime(QDate(2026, 9, 28), QTime(10, 0)); // Monday
    }

    void firesAfterIntervalOnce()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        QSignalSpy due(&engine, &ReminderEngine::reminderDue);

        run(engine, 19);
        QVERIFY(!due.contains({Eye}));
        run(engine, 1);
        QCOMPARE(due.count(), 1);
        QCOMPARE(due.first().first().toString(), Eye);
        QCOMPARE(engine.status(Eye).state, ReminderState::Due);

        // Stays due without re-firing until answered.
        run(engine, 30);
        QCOMPARE(due.count(QList<QVariant>{Eye}), 1);
    }

    void pauseAndIdleFreezeTheClock()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        QSignalSpy due(&engine, &ReminderEngine::reminderDue);

        run(engine, 10);
        m_paused = true;
        run(engine, 30);
        QCOMPARE(engine.status(Eye).state, ReminderState::Paused);
        m_paused = false;
        m_idle = true;
        run(engine, 30);
        QCOMPARE(engine.status(Eye).state, ReminderState::Idle);
        QCOMPARE(engine.status(Eye).secondsRemaining, 10 * 60);
        QCOMPARE(due.count(), 0);

        m_idle = false;
        run(engine, 10);
        QCOMPARE(due.count(), 1);
    }

    void outsideHoursDoesNotCountAndNewDayStartsFresh()
    {
        m_now = QDateTime(QDate(2026, 9, 28), QTime(17, 50));
        ReminderEngine engine(hooks());
        engine.reload();
        QSignalSpy due(&engine, &ReminderEngine::reminderDue);

        run(engine, 9); // 17:59, 9 minutes counted
        run(engine, 60);
        QCOMPARE(engine.status(Eye).state, ReminderState::OutsideHours);
        QCOMPARE(due.count(), 0);

        // Next morning: progress from yesterday is discarded.
        m_now = QDateTime(QDate(2026, 9, 29), QTime(8, 59));
        run(engine, 12);
        QCOMPARE(due.count(), 0);
        QCOMPARE(engine.status(Eye).secondsRemaining, 8 * 60); // ticks 9:00..9:11
    }

    void confirmRestartsAndSnoozeShortens()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        QSignalSpy due(&engine, &ReminderEngine::reminderDue);
        QSignalSpy resolved(&engine, &ReminderEngine::reminderResolved);

        run(engine, 20);
        engine.confirm(Eye);
        QCOMPARE(engine.status(Eye).secondsRemaining, 20 * 60);
        QCOMPARE(resolved.last().at(1).value<ReminderOutcome>(), ReminderOutcome::Confirmed);

        run(engine, 20);
        engine.snooze(Eye, 5);
        QCOMPARE(engine.status(Eye).state, ReminderState::Counting);
        QCOMPARE(engine.status(Eye).secondsRemaining, 5 * 60);
        run(engine, 5);
        QCOMPARE(due.count(QList<QVariant>{Eye}), 3);
    }

    void acknowledgeRestartsClock()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        run(engine, 20);
        engine.acknowledge(Eye);
        QCOMPARE(engine.status(Eye).state, ReminderState::Counting);
        QCOMPARE(engine.status(Eye).secondsRemaining, 20 * 60);
    }

    void triggerNowIgnoresSchedule()
    {
        m_now = QDateTime(QDate(2026, 10, 3), QTime(22, 0)); // Saturday night
        ReminderEngine engine(hooks());
        engine.reload();
        QSignalSpy due(&engine, &ReminderEngine::reminderDue);
        engine.triggerNow(Water);
        QCOMPARE(due.count(), 1);
        engine.triggerNow(Water); // already due: no duplicate
        QCOMPARE(due.count(), 1);
    }

    void configChangesPersistAndKeepProgress()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        run(engine, 10);

        ReminderConfig config = engine.config(Eye);
        config.intervalMinutes = 30;
        engine.setConfig(config);
        QCOMPARE(engine.status(Eye).secondsRemaining, 20 * 60);
        QCOMPARE(Reminders::load(Eye).intervalMinutes, 30);

        config.enabled = false;
        engine.setConfig(config);
        QCOMPARE(engine.status(Eye).state, ReminderState::Disabled);
    }

    void remindersRunIndependently()
    {
        ReminderEngine engine(hooks());
        engine.reload();
        QSignalSpy due(&engine, &ReminderEngine::reminderDue);
        for (int i = 0; i < 60; ++i) {
            run(engine, 1);
            if (engine.status(Eye).state == ReminderState::Due)
                engine.confirm(Eye);
        }
        QCOMPARE(due.count(QList<QVariant>{Eye}), 3);
        QCOMPARE(due.count(QList<QVariant>{Water}), 1);
    }
};

QTEST_GUILESS_MAIN(TestReminderEngine)
#include "tst_reminderengine.moc"
