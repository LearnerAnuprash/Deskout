#include "core/dailyupdates.h"
#include "core/database.h"
#include "ui/recapdialog.h"
#include "ui/updatespage.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestUpdatesPage : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    template<typename T>
    static T *child(QWidget *page, const char *name)
    {
        T *widget = page->findChild<T *>(QLatin1String(name));
        Q_ASSERT(widget);
        return widget;
    }

    // Cards in the history (today's card excluded).
    static int historyCards(QWidget *page)
    {
        int cards = 0;
        for (QFrame *frame : page->findChildren<QFrame *>(QStringLiteral("Card"))) {
            if (!frame->findChild<QPlainTextEdit *>())
                ++cards;
        }
        return cards;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DeskoutTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_updatespage"));
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

    void typingSavesToday()
    {
        DailyUpdatesStore store;
        UpdatesPage page(&store);
        page.show();
        QTest::keyClicks(child<QPlainTextEdit>(&page, "UpdateDone"), QStringLiteral("Wrote tests"));
        QTest::keyClicks(child<QPlainTextEdit>(&page, "UpdateTodo"), QStringLiteral("Ship phase 6"));
        QTRY_VERIFY_WITH_TIMEOUT(store.get(QDate::currentDate()).has_value(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(store.get(QDate::currentDate())->todo, QStringLiteral("Ship phase 6"), 3000);
        QCOMPARE(store.get(QDate::currentDate())->done, QStringLiteral("Wrote tests"));
    }

    void loadsTodayWithoutSaving()
    {
        const QDateTime written(QDate::currentDate(), QTime(0, 1));
        DailyUpdatesStore store(nullptr, [written] { return written; });
        store.save(QDate::currentDate(), QStringLiteral("Earlier today"), QString());
        UpdatesPage page(&store);
        page.show();
        QCOMPARE(child<QPlainTextEdit>(&page, "UpdateDone")->toPlainText(), QStringLiteral("Earlier today"));
        page.save();
        QTest::qWait(100);
        QCOMPARE(store.get(QDate::currentDate())->updatedAt, written);
    }

    void historyShowsEarlierDaysAndPages()
    {
        DailyUpdatesStore store;
        const QDate today = QDate::currentDate();
        store.save(today, QStringLiteral("today"), QString()); // not part of the history
        for (int i = 1; i <= DailyUpdatesStore::HistoryPageSize + 3; ++i)
            store.save(today.addDays(-i), QStringLiteral("day %1").arg(i), QString());

        UpdatesPage page(&store);
        page.show();
        QCOMPARE(historyCards(&page), DailyUpdatesStore::HistoryPageSize);
        QPushButton *older = nullptr;
        for (QPushButton *button : page.findChildren<QPushButton *>()) {
            if (button->text().contains(QLatin1String("older")))
                older = button;
        }
        QVERIFY(older && older->isVisible());
        older->click();
        QCOMPARE(historyCards(&page), DailyUpdatesStore::HistoryPageSize + 3);
        QVERIFY(!older->isVisible());

        // Newest first: the first history card is yesterday's.
        bool sawYesterday = false;
        for (QLabel *label : page.findChildren<QLabel *>()) {
            if (label->text() == QLatin1String("Yesterday"))
                sawYesterday = true;
        }
        QVERIFY(sawYesterday);
    }

    void recapSettingToggles()
    {
        DailyUpdatesStore store;
        UpdatesPage page(&store);
        page.show();
        QCheckBox *box = page.findChild<QCheckBox *>();
        QVERIFY(box->isChecked());
        box->click();
        QVERIFY(!DailyUpdatesStore::recapEnabled());
    }

    void recapWording()
    {
        const QDate today(2026, 9, 29);
        QCOMPARE(RecapDialog::greeting(QTime(8, 0)), QStringLiteral("Good morning"));
        QCOMPARE(RecapDialog::greeting(QTime(13, 0)), QStringLiteral("Good afternoon"));
        QCOMPARE(RecapDialog::greeting(QTime(20, 0)), QStringLiteral("Good evening"));
        QCOMPARE(RecapDialog::subtitle(today.addDays(-1), today), QStringLiteral("Here's what you noted yesterday."));
        QVERIFY(RecapDialog::subtitle(today.addDays(-3), today).contains(QLatin1String("26 September")));
    }

    void recapShowsBothSections()
    {
        DailyUpdate update;
        update.day = QDate(2026, 9, 28);
        update.done = QStringLiteral("Did things");
        update.todo = QStringLiteral("Do more");
        RecapDialog dialog(update, QDate(2026, 9, 29));
        QStringList texts;
        for (QLabel *label : dialog.findChildren<QLabel *>())
            texts << label->text();
        QVERIFY(texts.contains(QStringLiteral("Did things")));
        QVERIFY(texts.contains(QStringLiteral("Do more")));
        QVERIFY(texts.contains(QStringLiteral("Todos for today")));
    }
};

QTEST_MAIN(TestUpdatesPage)
#include "tst_updatespage.moc"
