#include "core/pausemanager.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class TestPauseManager : public QObject
{
    Q_OBJECT

private:
    // Fake clock the tests move forward by hand.
    QDateTime m_now;
    PauseManager::Clock clock()
    {
        return [this] { return m_now; };
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_pausemanager"));
    }

    void init()
    {
        QSettings().clear();
        m_now = QDateTime(QDate(2026, 9, 28), QTime(10, 0));
    }

    void startsActive()
    {
        PauseManager pause(nullptr, clock());
        QVERIFY(!pause.isPaused());
        QVERIFY(!pause.pausedUntil().isValid());
    }

    void pauseForSetsEndTimeAndExpires()
    {
        PauseManager pause(nullptr, clock());
        QSignalSpy spy(&pause, &PauseManager::pausedChanged);

        pause.pauseFor(15);
        QVERIFY(pause.isPaused());
        QCOMPARE(pause.pausedUntil(), m_now.addSecs(15 * 60));
        QCOMPARE(spy.count(), 1);

        m_now = m_now.addSecs(14 * 60);
        pause.checkExpiry();
        QVERIFY(pause.isPaused());

        m_now = m_now.addSecs(60);
        pause.checkExpiry();
        QVERIFY(!pause.isPaused());
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().first().toBool(), false);
    }

    void pauseUntilTomorrowEndsAtMidnight()
    {
        PauseManager pause(nullptr, clock());
        pause.pauseUntilTomorrow();
        QCOMPARE(pause.pausedUntil(), QDateTime(QDate(2026, 9, 29), QTime(0, 0)));
        QCOMPARE(pause.statusText(), QStringLiteral("Paused until tomorrow"));
    }

    void indefinitePauseNeverExpires()
    {
        PauseManager pause(nullptr, clock());
        pause.pauseIndefinitely();
        QVERIFY(pause.isIndefinite());
        m_now = m_now.addDays(30);
        pause.checkExpiry();
        QVERIFY(pause.isPaused());
    }

    void toggleFlipsState()
    {
        PauseManager pause(nullptr, clock());
        pause.toggle();
        QVERIFY(pause.isPaused());
        QVERIFY(pause.isIndefinite());
        pause.toggle();
        QVERIFY(!pause.isPaused());

        // Toggling a timed pause resumes rather than extending it.
        pause.pauseFor(60);
        pause.toggle();
        QVERIFY(!pause.isPaused());
    }

    void pausePersistsAcrossInstances()
    {
        {
            PauseManager pause(nullptr, clock());
            pause.pauseFor(60);
        }
        PauseManager restored(nullptr, clock());
        QVERIFY(restored.isPaused());
        QCOMPARE(restored.pausedUntil(), m_now.addSecs(60 * 60));
    }

    void expiredPauseIsDroppedOnLoad()
    {
        {
            PauseManager pause(nullptr, clock());
            pause.pauseFor(15);
        }
        m_now = m_now.addSecs(2 * 60 * 60);
        PauseManager restored(nullptr, clock());
        QVERIFY(!restored.isPaused());
    }

    void indefinitePausePersists()
    {
        {
            PauseManager pause(nullptr, clock());
            pause.pauseIndefinitely();
        }
        PauseManager restored(nullptr, clock());
        QVERIFY(restored.isIndefinite());
    }
};

QTEST_GUILESS_MAIN(TestPauseManager)
#include "tst_pausemanager.moc"
