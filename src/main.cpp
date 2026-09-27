#include "app/deskoutapp.h"
#include "core/commands.h"
#include "core/singleinstance.h"
#include "platform/autostart.h"

#include <QApplication>
#include <QCommandLineParser>

#include <cstdio>

int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    // Full-screen alarms and always-on-top windows need X11 window semantics:
    // on GNOME, Wayland clients can't keep a window above others. Run through
    // XWayland unless the user picked a platform explicitly.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Deskout"));
    QApplication::setOrganizationDomain(QStringLiteral("deskout.app"));
    QApplication::setApplicationName(QStringLiteral("Deskout"));
    QApplication::setApplicationVersion(QStringLiteral(DESKOUT_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("deskout"));
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Deskout — breaks, hydration and focus."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption minimized(QString::fromLatin1(AutoStart::StartMinimizedArg).mid(2),
                                       QStringLiteral("Start hidden in the system tray."));
    parser.addOption(minimized);

    // Commands forwarded to an already running instance.
    const QList<std::pair<const char *, const char *>> commandOptions = {
        {Commands::Show, "Show the main window."},
        {Commands::Settings, "Open the settings dialog."},
        {Commands::TogglePause, "Pause all reminders, or resume if paused."},
        {Commands::Pause, "Pause all reminders until resumed."},
        {Commands::Resume, "Resume all reminders."},
        {Commands::ToggleReadingMode, "Switch Reading mode (grayscale) on or off."},
        {Commands::Quit, "Quit the running instance."},
    };
    for (const auto &[name, help] : commandOptions)
        parser.addOption(QCommandLineOption(QLatin1String(name), QLatin1String(help)));
    parser.process(app);

    QString command;
    for (const auto &[name, help] : commandOptions) {
        Q_UNUSED(help)
        if (parser.isSet(QLatin1String(name))) {
            command = QLatin1String(name);
            break;
        }
    }

    SingleInstance instance;
    if (!instance.tryBecomePrimary()) {
        if (command.isEmpty())
            command = QLatin1String(Commands::Show);
        if (!SingleInstance::sendToPrimary(command)) {
            std::fprintf(stderr, "Deskout is running but did not respond.\n");
            return 1;
        }
        return 0;
    }

    // Nothing to pause/resume/quit if Deskout wasn't running (e.g. the
    // desktop shortcut fired after a crash). Don't start the app for it.
    if (command == QLatin1String(Commands::TogglePause) || command == QLatin1String(Commands::Pause)
        || command == QLatin1String(Commands::Resume) || command == QLatin1String(Commands::Quit)
        || command == QLatin1String(Commands::ToggleReadingMode)) {
        std::fprintf(stderr, "Deskout is not running.\n");
        return 0;
    }

    DeskoutApp deskout(&instance);
    deskout.start(parser.isSet(minimized));
    if (command == QLatin1String(Commands::Settings))
        deskout.handleCommand(command);
    return app.exec();
}
