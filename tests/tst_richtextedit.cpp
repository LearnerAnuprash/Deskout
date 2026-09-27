#include "ui/richtextedit.h"

#include <QMimeData>
#include <QTest>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTextList>

namespace {

// Exposes the protected paste hook.
class Edit : public RichTextEdit
{
public:
    using RichTextEdit::insertFromMimeData;
};

QTextBlock blockAt(const QTextEdit &edit, int number)
{
    return edit.document()->findBlockByNumber(number);
}

void placeCursorInBlock(QTextEdit &edit, int number)
{
    QTextCursor cursor(blockAt(edit, number));
    cursor.movePosition(QTextCursor::EndOfBlock);
    edit.setTextCursor(cursor);
}

} // namespace

class TestRichTextEdit : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void boldAppliesToSelection()
    {
        Edit edit;
        edit.setPlainText(QStringLiteral("hello world"));
        QTextCursor cursor = edit.textCursor();
        cursor.setPosition(0);
        cursor.setPosition(5, QTextCursor::KeepAnchor);
        edit.setTextCursor(cursor);
        edit.setBold(true);
        QVERIFY(edit.isBold());

        cursor.setPosition(2);
        edit.setTextCursor(cursor);
        QVERIFY(edit.isBold());
        cursor.setPosition(8);
        edit.setTextCursor(cursor);
        QVERIFY(!edit.isBold());
    }

    void shortcutsToggleEmphasis()
    {
        Edit edit;
        QTest::keyClick(&edit, Qt::Key_B, Qt::ControlModifier);
        QTest::keyClick(&edit, Qt::Key_I, Qt::ControlModifier);
        QTest::keyClicks(&edit, QStringLiteral("x"));
        QTextCursor cursor(edit.document());
        cursor.setPosition(1);
        QVERIFY(cursor.charFormat().fontWeight() >= QFont::Bold);
        QVERIFY(cursor.charFormat().fontItalic());
    }

    void headingsAndEnterReturnsToNormal()
    {
        Edit edit;
        edit.show();
        QTest::keyClicks(&edit, QStringLiteral("Title"));
        edit.setHeadingLevel(1);
        QCOMPARE(edit.headingLevel(), 1);
        QVERIFY(edit.isBold());

        QTest::keyClick(&edit, Qt::Key_Return);
        QTest::keyClicks(&edit, QStringLiteral("body"));
        QCOMPARE(edit.headingLevel(), 0);
        QVERIFY(!edit.isBold());
        QCOMPARE(blockAt(edit, 0).blockFormat().headingLevel(), 1);
        QCOMPARE(blockAt(edit, 1).text(), QStringLiteral("body"));

        // Survives saving and loading.
        const QString html = edit.toHtml();
        QVERIFY(html.contains(QLatin1String("<h1")));
        Edit reloaded;
        reloaded.setHtml(html);
        QCOMPARE(blockAt(reloaded, 0).blockFormat().headingLevel(), 1);
        QCOMPARE(blockAt(reloaded, 1).blockFormat().headingLevel(), 0);

        // Back to normal text.
        placeCursorInBlock(edit, 0);
        edit.setHeadingLevel(0);
        QCOMPARE(edit.headingLevel(), 0);
        QVERIFY(!edit.isBold());
    }

    void headingShortcut()
    {
        Edit edit;
        QTest::keyClicks(&edit, QStringLiteral("Section"));
        QTest::keyClick(&edit, Qt::Key_2, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(edit.headingLevel(), 2);
        QTest::keyClick(&edit, Qt::Key_0, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(edit.headingLevel(), 0);
    }

    void listToggle()
    {
        Edit edit;
        edit.setPlainText(QStringLiteral("one\ntwo"));
        QTextCursor cursor(edit.document());
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        edit.setTextCursor(cursor);
        edit.setListStyle(RichTextEdit::ListStyle::Bullet);
        QVERIFY(blockAt(edit, 0).textList());
        QCOMPARE(blockAt(edit, 0).textList(), blockAt(edit, 1).textList());
        QCOMPARE(edit.listStyle(), RichTextEdit::ListStyle::Bullet);

        // Switching the style changes the whole list.
        placeCursorInBlock(edit, 0);
        edit.setListStyle(RichTextEdit::ListStyle::Numbered);
        QCOMPARE(blockAt(edit, 1).textList()->format().style(), QTextListFormat::ListDecimal);

        cursor.select(QTextCursor::Document);
        edit.setTextCursor(cursor);
        edit.setListStyle(RichTextEdit::ListStyle::None);
        QVERIFY(!blockAt(edit, 0).textList());
        QVERIFY(!blockAt(edit, 1).textList());
        QCOMPARE(blockAt(edit, 0).blockFormat().indent(), 0);
    }

    void markdownStyleShortcuts()
    {
        Edit edit;
        edit.show();
        QTest::keyClicks(&edit, QStringLiteral("- item"));
        QCOMPARE(edit.listStyle(), RichTextEdit::ListStyle::Bullet);
        QCOMPARE(blockAt(edit, 0).text(), QStringLiteral("item"));

        Edit numbered;
        numbered.show();
        QTest::keyClicks(&numbered, QStringLiteral("1. first"));
        QCOMPARE(numbered.listStyle(), RichTextEdit::ListStyle::Numbered);
        QCOMPARE(blockAt(numbered, 0).text(), QStringLiteral("first"));

        // Only at the start of a line.
        Edit plain;
        QTest::keyClicks(&plain, QStringLiteral("a - b"));
        QCOMPARE(plain.listStyle(), RichTextEdit::ListStyle::None);
        QCOMPARE(plain.toPlainText(), QStringLiteral("a - b"));
    }

    void tabNestsAndRejoins()
    {
        Edit edit;
        edit.show();
        QTest::keyClicks(&edit, QStringLiteral("1. one"));
        QTest::keyClick(&edit, Qt::Key_Return);
        QTest::keyClicks(&edit, QStringLiteral("sub"));
        QTest::keyClick(&edit, Qt::Key_Tab);
        QTextList *outer = blockAt(edit, 0).textList();
        QTextList *inner = blockAt(edit, 1).textList();
        QVERIFY(outer && inner && outer != inner);
        QCOMPARE(inner->format().indent(), 2);
        QCOMPARE(inner->format().style(), QTextListFormat::ListLowerAlpha);

        QTest::keyClick(&edit, Qt::Key_Return);
        QTest::keyClicks(&edit, QStringLiteral("two"));
        QTest::keyClick(&edit, Qt::Key_Backtab, Qt::ShiftModifier);
        // Back in the first list, so it is numbered 2, not 1.
        QCOMPARE(blockAt(edit, 2).textList(), outer);
        QCOMPARE(outer->itemNumber(blockAt(edit, 2)), 1);

        // Un-nesting the top level leaves the list.
        QTest::keyClick(&edit, Qt::Key_Backtab, Qt::ShiftModifier);
        QVERIFY(!blockAt(edit, 2).textList());
    }

    void underlineRoundTrips()
    {
        Edit edit;
        edit.setUnderline(true);
        QTest::keyClicks(&edit, QStringLiteral("under"));
        Edit reloaded;
        reloaded.setHtml(edit.toHtml());
        QTextCursor cursor(reloaded.document());
        cursor.setPosition(2);
        QVERIFY(cursor.charFormat().fontUnderline());
    }

    void pasteKeepsStructureDropsStyling()
    {
        Edit edit;
        auto *mime = new QMimeData;
        mime->setHtml(QStringLiteral(
            "<h2 style='color:#123456'>Heading</h2>"
            "<p style='color:#ff0000; background:#000000; font-family:\"Comic Sans MS\"; font-size:30px'>"
            "<b>Bold</b> <i>it</i> <a href='https://example.com'>link</a><img src='https://example.com/x.png'></p>"
            "<ul><li>point</li></ul>"));
        edit.insertFromMimeData(mime);
        delete mime;

        QTextDocument *doc = edit.document();
        QCOMPARE(doc->findBlockByNumber(0).blockFormat().headingLevel(), 2);
        QVERIFY(doc->findBlockByNumber(2).textList());
        QVERIFY(!doc->toPlainText().contains(QChar::ObjectReplacementCharacter)); // image gone

        bool sawBold = false;
        bool sawItalic = false;
        bool sawLink = false;
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            QVERIFY(!block.blockFormat().hasProperty(QTextFormat::BackgroundBrush));
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextCharFormat format = it.fragment().charFormat();
                QVERIFY2(!format.hasProperty(QTextFormat::ForegroundBrush), qPrintable(it.fragment().text()));
                QVERIFY(!format.hasProperty(QTextFormat::BackgroundBrush));
                QVERIFY(!format.hasProperty(QTextFormat::FontFamilies));
                QVERIFY(!format.hasProperty(QTextFormat::FontPixelSize));
                sawBold |= it.fragment().text() == QLatin1String("Bold") && format.fontWeight() >= QFont::Bold;
                sawItalic |= it.fragment().text() == QLatin1String("it") && format.fontItalic();
                sawLink |= format.anchorHref() == QLatin1String("https://example.com");
            }
        }
        QVERIFY(sawBold);
        QVERIFY(sawItalic);
        QVERIFY(sawLink);
    }

    void plainPasteStaysPlain()
    {
        Edit edit;
        QMimeData mime;
        mime.setText(QStringLiteral("just text\nsecond line"));
        edit.insertFromMimeData(&mime);
        QCOMPARE(edit.toPlainText(), QStringLiteral("just text\nsecond line"));
    }
};

QTEST_MAIN(TestRichTextEdit)
#include "tst_richtextedit.moc"
