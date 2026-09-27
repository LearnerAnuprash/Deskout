#include "platform/grayscale/grayscalebackend.h"

#include "platform/linux/gsettings.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr char Uuid[] = "deskout-reading-mode@deskout.app";
constexpr char ShellService[] = "org.gnome.Shell";
constexpr char ObjectPath[] = "/app/deskout/ReadingMode";
constexpr char Interface[] = "app.deskout.ReadingMode";
constexpr char ShellSchema[] = "org.gnome.shell";
constexpr int MinShellMajor = 45; // ES-module extensions
constexpr int DBusTimeoutMs = 1000;

// Bundled extension files: resource path -> file name in the extension dir.
const std::pair<const char *, const char *> ExtensionFiles[] = {
    {":/gnome-extension/metadata.json", "metadata.json"},
    {":/gnome-extension/extension.js", "extension.js"},
};

QString tr(const char *text)
{
    return QCoreApplication::translate("ReadingMode", text);
}

QDBusMessage callShell(const QString &path, const QString &interface, const QString &method,
                       const QVariantList &args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(ShellService), path, interface, method);
    message.setArguments(args);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, DBusTimeoutMs);
}

class GnomeGrayscaleBackend final : public GrayscaleBackend
{
public:
    QString name() const override { return tr("GNOME Shell extension"); }

    Status status() override
    {
        const int major = shellMajorVersion();
        if (major == 0)
            return {Availability::Unsupported, tr("GNOME Shell isn't responding.")};
        if (major < MinShellMajor)
            return {Availability::Unsupported, tr("Reading mode needs GNOME 45 or newer (you have %1).").arg(major)};
        if (extensionResponds())
            return {Availability::Ready, QString()};
        if (userExtensionsDisabled())
            return {Availability::Unsupported,
                    tr("GNOME extensions are switched off. Turn them on in the Extensions app to use "
                       "Reading mode.")};
        if (!filesUpToDate() || !listedAsEnabled())
            return {Availability::NeedsInstall,
                    tr("Reading mode needs a small GNOME Shell extension that Deskout installs for "
                       "your account.")};
        return {Availability::NeedsRelogin,
                tr("Log out and back in once to finish setting up Reading mode. It will switch on "
                   "by itself afterwards.")};
    }

    bool install(QString *error) override
    {
        const QString dir = extensionDir();
        if (!QDir().mkpath(dir))
            return fail(error, tr("Could not create %1").arg(dir));
        for (const auto &[resource, fileName] : ExtensionFiles) {
            QFile source{QString::fromLatin1(resource)};
            if (!source.open(QIODevice::ReadOnly))
                return fail(error, tr("Bundled extension file %1 is missing.").arg(QLatin1String(resource)));
            QSaveFile target(dir + QLatin1Char('/') + QLatin1String(fileName));
            if (!target.open(QIODevice::WriteOnly) || target.write(source.readAll()) < 0 || !target.commit())
                return fail(error, tr("Could not write %1: %2").arg(target.fileName(), target.errorString()));
        }

        // GNOME Shell loads extensions listed in enabled-extensions. If it
        // already knows this extension (reinstall, X11 session), it enables
        // it immediately; otherwise it does so at the next login.
        QString detail;
        QStringList enabled;
        if (!GSettings::readStringList(QLatin1String(ShellSchema), QStringLiteral("enabled-extensions"), &enabled, &detail))
            return fail(error, detail);
        if (!enabled.contains(QLatin1String(Uuid))) {
            enabled << QLatin1String(Uuid);
            if (!GSettings::writeStringList(QLatin1String(ShellSchema), QStringLiteral("enabled-extensions"), enabled, &detail))
                return fail(error, detail);
        }
        QStringList disabled;
        if (GSettings::readStringList(QLatin1String(ShellSchema), QStringLiteral("disabled-extensions"), &disabled, nullptr)
            && disabled.removeAll(QLatin1String(Uuid)) > 0)
            GSettings::writeStringList(QLatin1String(ShellSchema), QStringLiteral("disabled-extensions"), disabled, nullptr);
        return true;
    }

    bool apply(bool grayscale, QString *error) override
    {
        const QDBusMessage reply = callShell(QLatin1String(ObjectPath), QLatin1String(Interface),
                                             QStringLiteral("SetEnabled"), {grayscale});
        if (reply.type() == QDBusMessage::ReplyMessage)
            return true;
        return fail(error, reply.errorMessage());
    }

private:
    static bool fail(QString *error, const QString &text)
    {
        if (error)
            *error = text;
        return false;
    }

    static QString extensionDir()
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + QStringLiteral("/gnome-shell/extensions/") + QLatin1String(Uuid);
    }

    static int shellMajorVersion()
    {
        const QDBusMessage reply = callShell(QStringLiteral("/org/gnome/Shell"),
                                             QStringLiteral("org.freedesktop.DBus.Properties"),
                                             QStringLiteral("Get"),
                                             {QStringLiteral("org.gnome.Shell"), QStringLiteral("ShellVersion")});
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            return 0;
        const QString version = reply.arguments().constFirst().value<QDBusVariant>().variant().toString();
        return version.section(QLatin1Char('.'), 0, 0).toInt();
    }

    static bool extensionResponds()
    {
        const QDBusMessage reply = callShell(QLatin1String(ObjectPath), QLatin1String(Interface),
                                             QStringLiteral("IsEnabled"));
        return reply.type() == QDBusMessage::ReplyMessage;
    }

    static bool userExtensionsDisabled()
    {
        const GSettings::Result r = GSettings::run(
            {QStringLiteral("get"), QLatin1String(ShellSchema), QStringLiteral("disable-user-extensions")});
        return r.ok && r.output == QLatin1String("true");
    }

    static bool listedAsEnabled()
    {
        QStringList enabled;
        return GSettings::readStringList(QLatin1String(ShellSchema), QStringLiteral("enabled-extensions"), &enabled, nullptr)
               && enabled.contains(QLatin1String(Uuid));
    }

    static bool filesUpToDate()
    {
        for (const auto &[resource, fileName] : ExtensionFiles) {
            QFile bundled{QString::fromLatin1(resource)};
            QFile installed(extensionDir() + QLatin1Char('/') + QLatin1String(fileName));
            if (!bundled.open(QIODevice::ReadOnly) || !installed.open(QIODevice::ReadOnly)
                || bundled.readAll() != installed.readAll())
                return false;
        }
        return true;
    }
};

class UnsupportedGrayscaleBackend final : public GrayscaleBackend
{
public:
    QString name() const override { return tr("Not available"); }
    Status status() override
    {
        return {Availability::Unsupported,
                tr("Reading mode currently works on GNOME, Windows and macOS. Other Linux desktops "
                   "aren't supported yet.")};
    }
    bool apply(bool, QString *error) override { return fail(error); }

private:
    bool fail(QString *error)
    {
        if (error)
            *error = status().message;
        return false;
    }
};

} // namespace

std::unique_ptr<GrayscaleBackend> createGrayscaleBackend()
{
    const QString desktop = QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP"));
    if (desktop.contains(QLatin1String("GNOME"), Qt::CaseInsensitive))
        return std::make_unique<GnomeGrayscaleBackend>();
    return std::make_unique<UnsupportedGrayscaleBackend>();
}
