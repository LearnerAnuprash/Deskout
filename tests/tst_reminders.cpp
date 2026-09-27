#include "core/eyeexercises.h"
#include "core/reminders.h"

#include <QSettings>
#include <QStandardPaths>
#include <QTest>

namespace {
// 2026-09-28 is a Monday.
QDateTime at(int day, int hour, int minute = 0)
{
    return QDateTime(QDate(2026, 9, 28).addDays(day), QTime(hour, minute));
}
} // namespace

class TestReminders : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_reminders"));
    }

    void init() { QSettings().clear(); }

    void defaultsAreStandardWorkday()
    {
        const ReminderConfig eye = Reminders::defaults(QLatin1String(ReminderIds::Eye));
        QCOMPARE(eye.intervalMinutes, 20);
        QCOMPARE(eye.start, QTime(9, 0));
        QCOMPARE(eye.end, QTime(18, 0));
        QCOMPARE(eye.days, ReminderDays::Weekdays);
        QVERIFY(eye.fullScreen);
        QCOMPARE(Reminders::allIds().size(), 3);
    }

    void activeInsideHoursOnWeekdays()
    {
        const ReminderConfig c = Reminders::defaults(QLatin1String(ReminderIds::Water));
        QVERIFY(!c.isActiveAt(at(0, 8, 59)));
        QVERIFY(c.isActiveAt(at(0, 9, 0)));
        QVERIFY(c.isActiveAt(at(0, 17, 59)));
        QVERIFY(!c.isActiveAt(at(0, 18, 0)));  // end is exclusive
        QVERIFY(c.isActiveAt(at(4, 12)));      // Friday
        QVERIFY(!c.isActiveAt(at(5, 12)));     // Saturday
        QVERIFY(!c.isActiveAt(at(6, 12)));     // Sunday
    }

    void disabledOrNoDaysIsNeverActive()
    {
        ReminderConfig c = Reminders::defaults(QLatin1String(ReminderIds::Walk));
        c.enabled = false;
        QVERIFY(!c.isActiveAt(at(0, 12)));
        c.enabled = true;
        c.days = 0;
        QVERIFY(!c.isActiveAt(at(0, 12)));
    }

    void sameStartAndEndMeansAllDay()
    {
        ReminderConfig c = Reminders::defaults(QLatin1String(ReminderIds::Eye));
        c.start = c.end = QTime(0, 0);
        QVERIFY(c.isActiveAt(at(0, 0, 0)));
        QVERIFY(c.isActiveAt(at(0, 23, 59)));
        QVERIFY(!c.isActiveAt(at(5, 12)));
    }

    void overnightWindowBelongsToStartDay()
    {
        ReminderConfig c = Reminders::defaults(QLatin1String(ReminderIds::Eye));
        c.start = QTime(22, 0);
        c.end = QTime(6, 0);
        c.days = ReminderDays::bit(Qt::Friday);
        QVERIFY(c.isActiveAt(at(4, 23)));     // Friday night
        QVERIFY(c.isActiveAt(at(5, 3)));      // early Saturday, still Friday's shift
        QVERIFY(!c.isActiveAt(at(5, 6)));
        QVERIFY(!c.isActiveAt(at(5, 23)));    // Saturday night not selected
        QVERIFY(!c.isActiveAt(at(4, 3)));     // early Friday = Thursday's shift
    }

    void saveAndLoadRoundTrip()
    {
        ReminderConfig c = Reminders::defaults(QLatin1String(ReminderIds::Walk));
        c.enabled = false;
        c.intervalMinutes = 45;
        c.days = ReminderDays::bit(Qt::Monday) | ReminderDays::bit(Qt::Wednesday);
        c.start = QTime(8, 30);
        c.end = QTime(17, 15);
        c.fullScreen = false;
        Reminders::save(c);
        QCOMPARE(Reminders::load(c.id), c);
    }

    void loadWithoutSettingsGivesDefaults()
    {
        const QString id = QLatin1String(ReminderIds::Eye);
        QCOMPARE(Reminders::load(id), Reminders::defaults(id));
    }

    void describeDays()
    {
        QCOMPARE(Reminders::describeDays(ReminderDays::EveryDay), QStringLiteral("Every day"));
        QCOMPARE(Reminders::describeDays(ReminderDays::bit(Qt::Saturday) | ReminderDays::bit(Qt::Sunday)),
                 QStringLiteral("Weekends"));
        QCOMPARE(Reminders::describeDays(0), QStringLiteral("No days"));
    }

    void eyeExercisesRotate()
    {
        const QList<EyeExercise> all = EyeExercises::all();
        QVERIFY(all.size() >= 4);
        for (int round = 0; round < 2; ++round) {
            for (const EyeExercise &expected : all) {
                const EyeExercise got = EyeExercises::takeNext();
                QCOMPARE(got.title, expected.title);
                QVERIFY(got.seconds > 0);
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestReminders)
#include "tst_reminders.moc"
