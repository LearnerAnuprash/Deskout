#include "platform/linux/gsettings.h"

#include <QTest>

class TestGSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parseEmptyTypedArray()
    {
        bool ok = false;
        QVERIFY(GSettings::parseStringArray(QStringLiteral("@as []"), &ok).isEmpty());
        QVERIFY(ok);
    }

    void parseList()
    {
        bool ok = false;
        const QStringList items = GSettings::parseStringArray(
            QStringLiteral("['extension@monitask.com', 'deskout-reading-mode@deskout.app']"), &ok);
        QVERIFY(ok);
        QCOMPARE(items, QStringList({QStringLiteral("extension@monitask.com"),
                                     QStringLiteral("deskout-reading-mode@deskout.app")}));
    }

    void rejectsGarbage()
    {
        bool ok = true;
        GSettings::parseStringArray(QStringLiteral("No such key"), &ok);
        QVERIFY(!ok);
    }

    void roundTripWithQuotes()
    {
        const QStringList items = {QStringLiteral("it's"), QStringLiteral("back\\slash"), QStringLiteral("plain")};
        const QString text = GSettings::serializeStringArray(items);
        QCOMPARE(text, QStringLiteral(R"(['it\'s', 'back\\slash', 'plain'])"));
        bool ok = false;
        QCOMPARE(GSettings::parseStringArray(text, &ok), items);
        QVERIFY(ok);
    }
};

QTEST_GUILESS_MAIN(TestGSettings)
#include "tst_gsettings.moc"
