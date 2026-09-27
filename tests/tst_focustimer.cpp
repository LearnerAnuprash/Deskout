#include "core/focustimer.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

namespace {
constexpr qint64 Second = 1000;
constexpr qint64 Minute = 60 * Second;
} // namespace

class TestFocusTimer : public QObject
{
    Q_OBJECT

private:
    QDateTime m_now;

    FocusTimer::Clock clock()
    {
        return [this] { return m_now; };
    }

    static FocusSession lastSession(const QSignalSpy &spy)
    {
        return spy.last().constFirst().value<FocusSession>();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_focustimer"));
    }

    void init()
    {
        QSettings().clear();
        m_now = QDateTime(QDate(2026, 9, 28), QTime(10, 0));
    }

    void defaultsTo25Minutes()
    {
        FocusTimer timer(nullptr, clock());
        QCOMPARE(timer.state(), FocusTimer::State::Idle);
        QCOMPARE(timer.lastMinutes(), 25);
        QCOMPARE(timer.remainingSeconds(), 25 * 60);
        QCOMPARE(FocusTimer::formatSeconds(timer.remainingSeconds()), QStringLiteral("25:00"));
    }

    void completesAfterPlannedTime()
    {
        FocusTimer timer(nullptr, clock());
        QSignalSpy ended(&timer, &FocusTimer::sessionEnded);
        timer.start(25, QStringLiteral("  Qt   internals "));
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        QCOMPARE(timer.topic(), QStringLiteral("Qt internals"));

        timer.advance(24 * Minute + 59 * Second);
        QCOMPARE(timer.remainingSeconds(), 1);
        QCOMPARE(ended.count(), 0);

        timer.advance(Second);
        QCOMPARE(timer.state(), FocusTimer::State::Finished);
        QCOMPARE(timer.remainingSeconds(), 0);
        QCOMPARE(timer.progress(), 1.0);
        QCOMPARE(ended.count(), 1);
        const FocusSession session = lastSession(ended);
        QVERIFY(session.completed);
        QCOMPARE(session.plannedSeconds, 25 * 60);
        QCOMPARE(session.focusedSeconds, 25 * 60);
        QCOMPARE(session.startedAt, QDateTime(QDate(2026, 9, 28), QTime(10, 0)));
        QCOMPARE(session.topic, QStringLiteral("Qt internals"));

        // Nothing more once finished.
        timer.advance(Minute);
        QCOMPARE(ended.count(), 1);
    }

    void remainingRoundsUp()
    {
        FocusTimer timer(nullptr, clock());
        QSignalSpy changed(&timer, &FocusTimer::remainingChanged);
        timer.start(1, QString());
        QCOMPARE(changed.count(), 1);
        timer.advance(1); // 59.999 s left still reads 1:00
        QCOMPARE(timer.remainingSeconds(), 60);
        QCOMPARE(changed.count(), 1);
        timer.advance(Second);
        QCOMPARE(timer.remainingSeconds(), 59);
        QCOMPARE(changed.count(), 2);
    }

    void pausedTimeDoesNotCount()
    {
        FocusTimer timer(nullptr, clock());
        timer.start(10, QString());
        timer.advance(4 * Minute);
        timer.pause();
        QCOMPARE(timer.state(), FocusTimer::State::Paused);
        timer.advance(30 * Minute); // ignored while paused
        QCOMPARE(timer.remainingSeconds(), 6 * 60);

        timer.resume();
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        QSignalSpy ended(&timer, &FocusTimer::sessionEnded);
        timer.advance(6 * Minute);
        QCOMPARE(ended.count(), 1);
        QCOMPARE(lastSession(ended).focusedSeconds, 10 * 60);
    }

    void stopAbandons()
    {
        FocusTimer timer(nullptr, clock());
        QSignalSpy ended(&timer, &FocusTimer::sessionEnded);
        timer.start(25, QStringLiteral("Reading"));
        timer.advance(12 * Minute);
        timer.stop();

        QCOMPARE(timer.state(), FocusTimer::State::Idle);
        QCOMPARE(ended.count(), 1);
        const FocusSession session = lastSession(ended);
        QVERIFY(!session.completed);
        QCOMPARE(session.focusedSeconds, 12 * 60);
        QCOMPARE(session.plannedSeconds, 25 * 60);
        // Ready for the next one at full length.
        QCOMPARE(timer.remainingSeconds(), 25 * 60);

        timer.stop(); // nothing active
        QCOMPARE(ended.count(), 1);
    }

    void startWhileActiveAbandonsFirst()
    {
        FocusTimer timer(nullptr, clock());
        QSignalSpy ended(&timer, &FocusTimer::sessionEnded);
        timer.start(25, QStringLiteral("A"));
        timer.advance(5 * Minute);
        timer.start(50, QStringLiteral("B"));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(lastSession(ended).topic, QStringLiteral("A"));
        QVERIFY(!lastSession(ended).completed);
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        QCOMPARE(timer.remainingSeconds(), 50 * 60);
    }

    void clampsDuration()
    {
        FocusTimer timer(nullptr, clock());
        timer.start(0, QString());
        QCOMPARE(timer.plannedMs(), qint64(FocusTimer::MinMinutes) * Minute);
        timer.start(10000, QString());
        QCOMPARE(timer.plannedMs(), qint64(FocusTimer::MaxMinutes) * Minute);
        QCOMPARE(FocusTimer::formatSeconds(int(timer.remainingSeconds())), QStringLiteral("4:00:00"));
    }

    void remembersLastSettings()
    {
        {
            FocusTimer timer(nullptr, clock());
            timer.start(50, QStringLiteral("Paper review"));
        }
        FocusTimer timer(nullptr, clock());
        QCOMPARE(timer.lastMinutes(), 50);
        QCOMPARE(timer.lastTopic(), QStringLiteral("Paper review"));
        QCOMPARE(timer.remainingSeconds(), 50 * 60);

        timer.startAgain();
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        QCOMPARE(timer.topic(), QStringLiteral("Paper review"));
        QCOMPARE(timer.plannedMs(), 50 * Minute);
    }

    void setNextUpdatesIdleTimerOnly()
    {
        FocusTimer timer(nullptr, clock());
        timer.setNext(40, QStringLiteral("Draft"));
        QCOMPARE(timer.remainingSeconds(), 40 * 60);
        QCOMPARE(timer.lastMinutes(), 40);

        timer.start(15, QString());
        timer.setNext(90, QStringLiteral("Later"));
        // The running session is untouched; the next one is remembered.
        QCOMPARE(timer.plannedMs(), 15 * Minute);
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        QCOMPARE(timer.lastMinutes(), 90);
        QCOMPARE(timer.lastTopic(), QStringLiteral("Later"));
    }

    void toggleCyclesStates()
    {
        FocusTimer timer(nullptr, clock());
        timer.toggle();
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        timer.toggle();
        QCOMPARE(timer.state(), FocusTimer::State::Paused);
        timer.toggle();
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        timer.advance(25 * Minute);
        QCOMPARE(timer.state(), FocusTimer::State::Finished);
        timer.toggle();
        QCOMPARE(timer.state(), FocusTimer::State::Running);
        QCOMPARE(timer.remainingSeconds(), 25 * 60);
    }

    void realTickCountsDown()
    {
        FocusTimer timer(nullptr, clock());
        QSignalSpy changed(&timer, &FocusTimer::remainingChanged);
        timer.start(1, QString());
        QTRY_VERIFY_WITH_TIMEOUT(timer.remainingSeconds() <= 59, 3000);
        QVERIFY(changed.count() >= 2);
    }
};

QTEST_GUILESS_MAIN(TestFocusTimer)
#include "tst_focustimer.moc"
