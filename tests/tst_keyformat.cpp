#include "platform/hotkey/keyformat.h"

#include <QKeySequence>
#include <QTest>

class TestKeyFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void gtkAccelerator_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::addColumn<QString>("expected");
        QTest::newRow("default") << "Ctrl+Alt+P" << "<Control><Alt>p";
        QTest::newRow("shift-super-digit") << "Shift+Meta+1" << "<Shift><Super>1";
        QTest::newRow("function-key") << "Ctrl+F12" << "<Control>F12";
        QTest::newRow("page-down") << "Alt+PgDown" << "<Alt>Page_Down";
        QTest::newRow("space") << "Ctrl+Space" << "<Control>space";
        QTest::newRow("unsupported") << "Ctrl+Alt+Tab" << "";
    }

    void gtkAccelerator()
    {
        QFETCH(QString, sequence);
        QFETCH(QString, expected);
        const QKeySequence seq(sequence, QKeySequence::PortableText);
        QCOMPARE(KeyFormat::gtkAccelerator(seq[0]), expected);
    }

    void usableShortcut_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::addColumn<bool>("usable");
        QTest::newRow("ctrl-alt-p") << "Ctrl+Alt+P" << true;
        QTest::newRow("meta-letter") << "Meta+B" << true;
        QTest::newRow("no-modifier") << "P" << false;
        QTest::newRow("shift-only") << "Shift+P" << false;
        QTest::newRow("unsupported-key") << "Ctrl+Tab" << false;
    }

    void usableShortcut()
    {
        QFETCH(QString, sequence);
        QFETCH(bool, usable);
        const QKeySequence seq(sequence, QKeySequence::PortableText);
        QString reason;
        QCOMPARE(KeyFormat::isUsableGlobalShortcut(seq[0], &reason), usable);
        QCOMPARE(reason.isEmpty(), usable);
    }

    void emptyShortcutIsRejected()
    {
        QString reason;
        QVERIFY(!KeyFormat::isUsableGlobalShortcut(QKeyCombination(), &reason));
        QVERIFY(!reason.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestKeyFormat)
#include "tst_keyformat.moc"
