#include "core/database.h"
#include "core/docsstore.h"
#include "ui/docspage.h"
#include "ui/richtextedit.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

// Browsing, opening, renaming and autosaving topic documents.
class TestDocsPage : public QObject
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

private Q_SLOTS:
    void init()
    {
        QVERIFY(Database::open(m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction()))));
    }

    void cleanup()
    {
        Database::close();
    }

    void newDocumentOpensForNaming()
    {
        DocsStore store;
        DocsPage page(&store);
        page.show();
        page.newDoc();
        QVERIFY(page.isEditing());
        QCOMPARE(store.count(), 1);
        auto *title = child<QLineEdit>(&page, "DocTitle");
        QCOMPARE(title->text(), QStringLiteral("Untitled document"));
        QCOMPARE(title->selectedText(), title->text());
    }

    void editsAutosave()
    {
        DocsStore store;
        const qint64 id = store.create(QStringLiteral("Research"));
        DocsPage page(&store);
        page.show();
        page.openDoc(id);
        const QDateTime before = store.get(id)->updatedAt;

        auto *edit = child<RichTextEdit>(&page, "DocPage");
        edit->setBold(true);
        QTest::keyClicks(edit, QStringLiteral("Findings so far"));
        QTRY_COMPARE_WITH_TIMEOUT(store.get(id)->plainText, QStringLiteral("Findings so far"), 3000);
        const std::optional<Doc> doc = store.get(id);
        QCOMPARE(doc->wordCount, 3);
        QVERIFY(doc->html.contains(QLatin1String("font-weight")));
        QVERIFY(doc->updatedAt >= before);
    }

    void openingDoesNotCountAsEdit()
    {
        DocsStore store(nullptr, [] { return QDateTime(QDate(2026, 1, 1), QTime(8, 0)); });
        const qint64 id = store.create(QStringLiteral("Old"));
        store.saveContent(id, QStringLiteral("<p>kept</p>"), QStringLiteral("kept"));
        DocsPage page(&store);
        page.show();
        page.openDoc(id);
        QCOMPARE(child<RichTextEdit>(&page, "DocPage")->toPlainText(), QStringLiteral("kept"));
        page.showList();
        QTest::qWait(50);
        QCOMPARE(store.get(id)->updatedAt, QDateTime(QDate(2026, 1, 1), QTime(8, 0)));
    }

    void renameFromEditorAndList()
    {
        DocsStore store;
        const qint64 id = store.create(QStringLiteral("Draft"));
        DocsPage page(&store);
        page.show();

        page.openDoc(id);
        auto *title = child<QLineEdit>(&page, "DocTitle");
        title->setText(QStringLiteral("Design notes"));
        Q_EMIT title->editingFinished();
        QCOMPARE(store.get(id)->title, QStringLiteral("Design notes"));

        // A blank name is refused and the old one comes back.
        title->setText(QStringLiteral("  "));
        Q_EMIT title->editingFinished();
        QCOMPARE(title->text(), QStringLiteral("Design notes"));
        QCOMPARE(store.get(id)->title, QStringLiteral("Design notes"));

        page.showList();
        QVERIFY(!page.isEditing());
        auto *tree = child<QTreeWidget>(&page, "DocsList");
        QTRY_COMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Design notes"));
        tree->topLevelItem(0)->setText(0, QStringLiteral("Renamed inline"));
        QCOMPARE(store.get(id)->title, QStringLiteral("Renamed inline"));
    }

    void clickOpensAndBackReturns()
    {
        DocsStore store;
        store.create(QStringLiteral("First"));
        const qint64 second = store.create(QStringLiteral("Second"));
        DocsPage page(&store);
        page.show();
        auto *tree = child<QTreeWidget>(&page, "DocsList");
        QCOMPARE(tree->topLevelItemCount(), 2);
        // Newest first by default.
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Second"));

        const QRect rect = tree->visualItemRect(tree->topLevelItem(0));
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, {}, rect.center());
        QVERIFY(page.isEditing());
        QCOMPARE(child<QLineEdit>(&page, "DocTitle")->text(), QStringLiteral("Second"));

        QPushButton *back = child<QPushButton>(&page, "BackButton");
        back->click();
        QVERIFY(!page.isEditing());
        QCOMPARE(tree->currentItem()->text(0), QStringLiteral("Second"));
        Q_UNUSED(second)
    }

    void filterMatchesText()
    {
        DocsStore store;
        const qint64 id = store.create(QStringLiteral("Alpha"));
        store.saveContent(id, QStringLiteral("<p>quantum</p>"), QStringLiteral("quantum"));
        store.create(QStringLiteral("Beta"));
        DocsPage page(&store);
        page.show();
        child<QLineEdit>(&page, "DocsFilter")->setText(QStringLiteral("quant"));
        auto *tree = child<QTreeWidget>(&page, "DocsList");
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Alpha"));
    }
};

QTEST_MAIN(TestDocsPage)
#include "tst_docspage.moc"
