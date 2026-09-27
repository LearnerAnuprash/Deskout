#include "core/database.h"
#include "core/stats.h"
#include "ui/statspage.h"
#include "ui/statswidgets.h"

#include <QLabel>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestStatsPage : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    static QStringList texts(QWidget *page)
    {
        QStringList result;
        for (QLabel *label : page->findChildren<QLabel *>())
            result << label->text();
        return result;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_statspage"));
    }

    void init()
    {
        QSettings().clear();
        QVERIFY(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction()))));
    }

    void cleanup()
    {
        Database::close();
    }

    void emptyDayReadsCleanly()
    {
        StatsStore stats;
        StatsPage page(&stats);
        page.show();
        const QStringList shown = texts(&page);
        QVERIFY(shown.contains(QStringLiteral("no breaks due yet")));
        QVERIFY(shown.contains(QStringLiteral("glasses of water")));
        for (ProgressRing *ring : page.findChildren<ProgressRing *>())
            QCOMPARE(ring->percent(), -1);
    }

    void showsTodayAndUpdatesLive()
    {
        StatsStore stats;
        const QDate today = QDate::currentDate();
        stats.add(today.addDays(-1), StatsMetric::reminderShown(QStringLiteral("eye")), 3);
        stats.add(today.addDays(-1), StatsMetric::reminderTaken(QStringLiteral("eye")), 3);
        stats.add(today, StatsMetric::reminderShown(QStringLiteral("eye")), 4);
        stats.add(today, StatsMetric::reminderTaken(QStringLiteral("eye")), 4);
        stats.add(today, StatsMetric::reminderShown(QStringLiteral("water")), 2);
        stats.add(today, StatsMetric::reminderTaken(QStringLiteral("water")), 1);
        stats.add(today, QLatin1String(StatsMetric::FocusCompleted), 2);
        stats.add(today, QLatin1String(StatsMetric::FocusSeconds), 50 * 60);

        StatsPage page(&stats);
        page.show();
        QStringList shown = texts(&page);
        QVERIFY(shown.contains(QStringLiteral("5/6")));   // breaks today
        QVERIFY(shown.contains(QStringLiteral("4/4")));   // eye
        QVERIFY(shown.contains(QStringLiteral("2")));     // focus sessions / streak
        QVERIFY(shown.contains(QStringLiteral("sessions · 50 min")));
        QVERIFY(shown.contains(QStringLiteral("goal met")));
        QVERIFY(shown.contains(QStringLiteral("days in a row · best 2")));

        stats.add(today, StatsMetric::reminderTaken(QStringLiteral("water")));
        shown = texts(&page);
        QVERIFY(shown.contains(QStringLiteral("6/6")));
    }
};

QTEST_MAIN(TestStatsPage)
#include "tst_statspage.moc"
