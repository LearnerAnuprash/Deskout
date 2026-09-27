#include "app/alertcenter.h"

#include "core/pausemanager.h"
#include "core/settingskeys.h"
#include "platform/activity/activitymonitor.h"
#include "ui/notifier.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

// Runs on the offscreen platform with no session bus (see CMakeLists), so
// no real window or notification reaches the desktop.
class TestAlertCenter : public QObject
{
    Q_OBJECT

private:
    static Alert alert(const QString &key, bool fullScreen = true)
    {
        Alert a;
        a.key = key;
        a.title = QStringLiteral("Title %1").arg(key);
        a.body = QStringLiteral("Body");
        a.confirmLabel = QStringLiteral("OK");
        a.actions << AlertAction{QStringLiteral("later"), QStringLiteral("Later")};
        a.disableFullScreenLabel = QStringLiteral("Disable");
        a.fullScreen = fullScreen;
        return a;
    }

    static QList<QWidget *> alarmWindows()
    {
        QList<QWidget *> windows;
        const QWidgetList all = QApplication::topLevelWidgets();
        for (QWidget *w : all) {
            if (w->objectName() == QLatin1String("AlarmRoot") && w->isVisible())
                windows << w;
        }
        return windows;
    }

    static QString shownTitle()
    {
        for (QWidget *window : alarmWindows()) {
            for (QLabel *label : window->findChildren<QLabel *>()) {
                if (label->objectName() == QLatin1String("AlarmTitle") && label->isVisibleTo(window))
                    return label->text();
            }
        }
        return QString();
    }

    // Waits out the alarm's input grace period, then clicks.
    static bool click(const QString &actionKey)
    {
        for (QWidget *window : alarmWindows()) {
            for (QPushButton *button : window->findChildren<QPushButton *>()) {
                const bool match = actionKey == QLatin1String("disable")
                                       ? button->objectName() == QLatin1String("AlarmLink")
                                       : button->property("actionKey").toString() == actionKey;
                if (!match)
                    continue;
                if (!QTest::qWaitFor([button] { return button->isEnabled(); }, 3000))
                    return false;
                button->click();
                return true;
            }
        }
        return false;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_alertcenter"));
    }

    void init()
    {
        QSettings settings;
        settings.clear();
        // Keep the real desktop out of it.
        settings.setValue(SettingsKeys::FullscreenDetectionEnabled, false);
        settings.setValue(SettingsKeys::IdleDetectionEnabled, false);
    }

    void cleanup()
    {
        QTRY_VERIFY(alarmWindows().isEmpty());
    }

    void fullScreenAlarmReportsButtons()
    {
        PauseManager pause;
        ActivityMonitor activity;
        Notifier notifier;
        AlertCenter center(&activity, &notifier, &pause);
        QSignalSpy responded(&center, &AlertCenter::responded);

        center.raise(alert(QStringLiteral("a")));
        QVERIFY(center.isShowing(QStringLiteral("a")));
        QCOMPARE(alarmWindows().size(), QGuiApplication::screens().size());
        QCOMPARE(shownTitle(), QStringLiteral("Title a"));

        QVERIFY(click(QStringLiteral("later")));
        QCOMPARE(responded.count(), 1);
        QCOMPARE(responded[0], (QVariantList{QStringLiteral("a"), QStringLiteral("later")}));
        QVERIFY(!center.isShowing(QStringLiteral("a")));
    }

    void queuesOneAtATimeWithoutDuplicates()
    {
        PauseManager pause;
        ActivityMonitor activity;
        Notifier notifier;
        AlertCenter center(&activity, &notifier, &pause);
        QSignalSpy responded(&center, &AlertCenter::responded);

        center.raise(alert(QStringLiteral("a")));
        center.raise(alert(QStringLiteral("b")));
        center.raise(alert(QStringLiteral("a"))); // already showing
        center.raise(alert(QStringLiteral("b"))); // already queued
        QCOMPARE(alarmWindows().size(), QGuiApplication::screens().size());
        QCOMPARE(shownTitle(), QStringLiteral("Title a"));

        QVERIFY(click(QLatin1String(Alert::ConfirmKey)));
        QTRY_VERIFY(center.isShowing(QStringLiteral("b")));
        QCOMPARE(shownTitle(), QStringLiteral("Title b"));
        QVERIFY(click(QLatin1String(Alert::ConfirmKey)));

        QCOMPARE(responded.count(), 2);
        QCOMPARE(responded[0][0].toString(), QStringLiteral("a"));
        QCOMPARE(responded[1][0].toString(), QStringLiteral("b"));
        // Nothing else was queued.
        QTest::qWait(1200);
        QVERIFY(alarmWindows().isEmpty());
    }

    void notificationOnlyNeedsNoAnswer()
    {
        PauseManager pause;
        ActivityMonitor activity;
        Notifier notifier;
        AlertCenter center(&activity, &notifier, &pause);
        QSignalSpy notified(&center, &AlertCenter::notified);

        center.raise(alert(QStringLiteral("n"), false));
        QCOMPARE(notified.count(), 1);
        QCOMPARE(notified[0][0].toString(), QStringLiteral("n"));
        QVERIFY(alarmWindows().isEmpty());
    }

    void pauseDropsShowingAndQueued()
    {
        PauseManager pause;
        ActivityMonitor activity;
        Notifier notifier;
        AlertCenter center(&activity, &notifier, &pause);
        QSignalSpy interrupted(&center, &AlertCenter::interrupted);
        QSignalSpy responded(&center, &AlertCenter::responded);

        center.raise(alert(QStringLiteral("a")));
        center.raise(alert(QStringLiteral("b")));
        pause.pauseIndefinitely();
        QCOMPARE(interrupted.count(), 2);
        QCOMPARE(interrupted[0][0].toString(), QStringLiteral("a"));
        QCOMPARE(interrupted[1][0].toString(), QStringLiteral("b"));
        QVERIFY(alarmWindows().isEmpty());

        // While paused nothing is shown at all.
        center.raise(alert(QStringLiteral("c")));
        center.raise(alert(QStringLiteral("d"), false));
        QCOMPARE(interrupted.count(), 4);
        QVERIFY(alarmWindows().isEmpty());
        QCOMPARE(responded.count(), 0);
        pause.resume();
    }

    void disableLinkReportsFullScreenDisabled()
    {
        PauseManager pause;
        ActivityMonitor activity;
        Notifier notifier;
        AlertCenter center(&activity, &notifier, &pause);
        QSignalSpy disabled(&center, &AlertCenter::fullScreenDisabled);
        QSignalSpy responded(&center, &AlertCenter::responded);

        center.raise(alert(QStringLiteral("a")));
        QVERIFY(click(QStringLiteral("disable")));
        QCOMPARE(disabled.count(), 1);
        QCOMPARE(disabled[0][0].toString(), QStringLiteral("a"));
        QCOMPARE(responded.count(), 0);
    }

    void eyeExerciseFollowsConfirm()
    {
        PauseManager pause;
        ActivityMonitor activity;
        Notifier notifier;
        AlertCenter center(&activity, &notifier, &pause);
        QSignalSpy responded(&center, &AlertCenter::responded);

        Alert eye = alert(QStringLiteral("eye"));
        eye.eyeExercise = true;
        center.raise(eye);
        QVERIFY(click(QLatin1String(Alert::ConfirmKey)));
        QCOMPARE(responded.count(), 1);
        // Still up, now showing the exercise.
        QVERIFY(center.isShowing(QStringLiteral("eye")));
        QVERIFY(!alarmWindows().isEmpty());
        QVERIFY(shownTitle() != QStringLiteral("Title eye"));

        // Pausing now closes it, but the break was taken: not "interrupted".
        QSignalSpy interrupted(&center, &AlertCenter::interrupted);
        pause.pauseIndefinitely();
        QVERIFY(!center.isShowing(QStringLiteral("eye")));
        QCOMPARE(interrupted.count(), 0);
        pause.resume();
    }
};

QTEST_MAIN(TestAlertCenter)
#include "tst_alertcenter.moc"
