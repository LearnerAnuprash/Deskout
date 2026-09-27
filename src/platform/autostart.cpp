#include "platform/autostart.h"

#include "ui/appicon.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr char MacLabel[] = "app.deskout.Deskout";
constexpr char WindowsRunKey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr char WindowsValueName[] = "Deskout";

// Quote one argument for the Exec= key of a .desktop file.
QString quoteDesktopExecArg(QString arg)
{
    arg.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    arg.replace(QLatin1Char('"'), QLatin1String("\\\""));
    arg.replace(QLatin1Char('`'), QLatin1String("\\`"));
    arg.replace(QLatin1Char('$'), QLatin1String("\\$"));
    arg.replace(QLatin1Char('%'), QLatin1String("%%"));
    return QLatin1Char('"') + arg + QLatin1Char('"');
}

QString xmlEscape(const QString &text)
{
    return text.toHtmlEscaped();
}

bool writeFile(const QString &path, const QString &content, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(content.toUtf8()) < 0 || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate("AutoStart", "Could not write %1: %2")
                         .arg(path, file.errorString());
        return false;
    }
    return true;
}

bool removeFile(const QString &path, QString *error)
{
    if (!QFile::exists(path) || QFile::remove(path))
        return true;
    if (error)
        *error = QCoreApplication::translate("AutoStart", "Could not remove %1").arg(path);
    return false;
}

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
QString linuxEntryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/autostart/deskout.desktop");
}
#endif

#if defined(Q_OS_MACOS)
QString macPlistPath()
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents/") + QLatin1String(MacLabel)
           + QStringLiteral(".plist");
}
#endif

} // namespace

namespace AutoStart {

QString executablePath()
{
    // Inside an AppImage the running binary lives in a temporary mount;
    // the stable path is the .AppImage file itself.
    const QByteArray appImage = qgetenv("APPIMAGE");
    if (!appImage.isEmpty())
        return QString::fromLocal8Bit(appImage);
    return QCoreApplication::applicationFilePath();
}

QString desktopEntry(const QString &executable, const QString &iconPath)
{
    QString entry = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Deskout\n"
        "Comment=Break, hydration and focus reminders\n");
    entry += QStringLiteral("Exec=%1 %2\n")
                 .arg(quoteDesktopExecArg(executable), QLatin1String(StartMinimizedArg));
    if (!iconPath.isEmpty())
        entry += QStringLiteral("Icon=%1\n").arg(iconPath);
    entry += QStringLiteral(
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n"
        // Give the panel/tray a moment to come up before we register our icon.
        "X-GNOME-Autostart-Delay=3\n");
    return entry;
}

QString launchAgentPlist(const QString &executable)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
               "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
               "<plist version=\"1.0\">\n"
               "<dict>\n"
               "    <key>Label</key>\n"
               "    <string>%1</string>\n"
               "    <key>ProgramArguments</key>\n"
               "    <array>\n"
               "        <string>%2</string>\n"
               "        <string>%3</string>\n"
               "    </array>\n"
               "    <key>RunAtLoad</key>\n"
               "    <true/>\n"
               "    <key>ProcessType</key>\n"
               "    <string>Interactive</string>\n"
               "</dict>\n"
               "</plist>\n")
        .arg(QLatin1String(MacLabel), xmlEscape(executable), QLatin1String(StartMinimizedArg));
}

QString windowsRunCommand(const QString &executable)
{
    return QStringLiteral("\"%1\" %2")
        .arg(QDir::toNativeSeparators(executable), QLatin1String(StartMinimizedArg));
}

bool isSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    return true;
#else
    return false;
#endif
}

bool isEnabled()
{
#if defined(Q_OS_WIN)
    QSettings run(QLatin1String(WindowsRunKey), QSettings::NativeFormat);
    return run.contains(QLatin1String(WindowsValueName));
#elif defined(Q_OS_MACOS)
    return QFile::exists(macPlistPath());
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    // A user can disable an entry without deleting it (Hidden=true).
    QFile file(linuxEntryPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line == QLatin1String("Hidden=true")
            || line == QLatin1String("X-GNOME-Autostart-enabled=false"))
            return false;
    }
    return true;
#else
    return false;
#endif
}

bool setEnabled(bool enabled, QString *error)
{
    const QString exe = executablePath();
#if defined(Q_OS_WIN)
    QSettings run(QLatin1String(WindowsRunKey), QSettings::NativeFormat);
    if (enabled)
        run.setValue(QLatin1String(WindowsValueName), windowsRunCommand(exe));
    else
        run.remove(QLatin1String(WindowsValueName));
    run.sync();
    if (run.status() != QSettings::NoError) {
        if (error)
            *error = QCoreApplication::translate("AutoStart", "Could not update the registry.");
        return false;
    }
    return true;
#elif defined(Q_OS_MACOS)
    return enabled ? writeFile(macPlistPath(), launchAgentPlist(exe), error)
                   : removeFile(macPlistPath(), error);
#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    if (!enabled)
        return removeFile(linuxEntryPath(), error);
    return writeFile(linuxEntryPath(), desktopEntry(exe, AppIcon::exportPng()), error);
#else
    Q_UNUSED(enabled)
    Q_UNUSED(exe)
    if (error)
        *error = QCoreApplication::translate("AutoStart", "Not supported on this platform.");
    return false;
#endif
}

void refreshIfEnabled()
{
    if (isEnabled())
        setEnabled(true);
}

} // namespace AutoStart
