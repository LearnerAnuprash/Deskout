#include "platform/autostart.h"

#include <QTest>

class TestAutoStart : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void desktopEntryQuotesExec()
    {
        const QString entry = AutoStart::desktopEntry(QStringLiteral("/opt/My Apps/deskout"),
                                                      QStringLiteral("/tmp/deskout.png"));
        QVERIFY(entry.startsWith(QLatin1String("[Desktop Entry]\n")));
        QVERIFY(entry.contains(QLatin1String("Exec=\"/opt/My Apps/deskout\" --minimized\n")));
        QVERIFY(entry.contains(QLatin1String("Icon=/tmp/deskout.png\n")));
        QVERIFY(entry.contains(QLatin1String("X-GNOME-Autostart-enabled=true\n")));
    }

    void desktopEntryEscapesSpecialCharacters()
    {
        const QString entry = AutoStart::desktopEntry(QStringLiteral("/home/u/100%$\"x\"/deskout"), QString());
        QVERIFY(entry.contains(QLatin1String(R"(Exec="/home/u/100%%\$\"x\"/deskout" --minimized)")));
        QVERIFY(!entry.contains(QLatin1String("Icon=")));
    }

    void launchAgentHasProgramArguments()
    {
        const QString plist = AutoStart::launchAgentPlist(
            QStringLiteral("/Applications/Deskout.app/Contents/MacOS/Deskout"));
        QVERIFY(plist.contains(QLatin1String("<string>app.deskout.Deskout</string>")));
        QVERIFY(plist.contains(
            QLatin1String("<string>/Applications/Deskout.app/Contents/MacOS/Deskout</string>")));
        QVERIFY(plist.contains(QLatin1String("<string>--minimized</string>")));
        QVERIFY(plist.contains(QLatin1String("<key>RunAtLoad</key>")));
    }

    void launchAgentEscapesXml()
    {
        const QString plist = AutoStart::launchAgentPlist(QStringLiteral("/Apps/R&D <x>/Deskout"));
        QVERIFY(plist.contains(QLatin1String("<string>/Apps/R&amp;D &lt;x&gt;/Deskout</string>")));
    }

    void windowsRunCommandIsQuoted()
    {
        const QString command = AutoStart::windowsRunCommand(QStringLiteral("C:/Program Files/Deskout/deskout.exe"));
#ifdef Q_OS_WIN
        QCOMPARE(command, QStringLiteral("\"C:\\Program Files\\Deskout\\deskout.exe\" --minimized"));
#else
        QCOMPARE(command, QStringLiteral("\"C:/Program Files/Deskout/deskout.exe\" --minimized"));
#endif
    }
};

QTEST_GUILESS_MAIN(TestAutoStart)
#include "tst_autostart.moc"
