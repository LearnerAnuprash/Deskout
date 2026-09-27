#include "platform/hotkey/keyformat.h"
#include "platform/hotkey/linuxbackends.h"

#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

namespace {

constexpr char MediaKeysSchema[] = "org.gnome.settings-daemon.plugins.media-keys";
constexpr char CustomListKey[] = "custom-keybindings";
constexpr char CustomSchema[] = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr char DeskoutPath[] = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/deskout-pause/";

struct GSettingsResult
{
    bool ok = false;
    QString output;
    QString error;
};

GSettingsResult gsettings(const QStringList &args)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), args);
    GSettingsResult result;
    if (!process.waitForFinished(3000)) {
        process.kill();
        result.error = process.errorString();
        return result;
    }
    result.ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    result.error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return result;
}

// GVariant text form of a string.
QString gvariantString(QString value)
{
    value.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    value.replace(QLatin1Char('\''), QLatin1String("\\'"));
    return QLatin1Char('\'') + value + QLatin1Char('\'');
}

// Parses gsettings output such as "@as []" or "['/a/', '/b/']".
QStringList parseStringArray(const QString &text, bool *ok)
{
    QString body = text;
    if (body.startsWith(QLatin1String("@as")))
        body = body.mid(3).trimmed();
    *ok = body.startsWith(QLatin1Char('[')) && body.endsWith(QLatin1Char(']'));
    QStringList items;
    static const QRegularExpression item(QStringLiteral(R"('((?:[^'\\]|\\.)*)')"));
    auto it = item.globalMatch(body);
    while (it.hasNext())
        items << it.next().captured(1);
    return items;
}

QString serializeStringArray(const QStringList &items)
{
    QStringList quoted;
    for (const QString &value : items)
        quoted << gvariantString(value);
    return QLatin1Char('[') + quoted.join(QLatin1String(", ")) + QLatin1Char(']');
}

class GnomeHotkeyBackend final : public HotkeyBackend
{
public:
    using HotkeyBackend::HotkeyBackend;

    QString name() const override
    {
        return QCoreApplication::translate("GlobalHotkey", "GNOME custom shortcut");
    }

    bool activatesViaCommand() const override { return true; }

    bool registerHotkey(QKeyCombination combo, QString *error) override
    {
        const QString accel = KeyFormat::gtkAccelerator(combo);
        const QString relocatable = QLatin1String(CustomSchema) + QLatin1Char(':') + QLatin1String(DeskoutPath);

        QStringList paths;
        if (!readPaths(&paths, error))
            return false;
        if (!paths.contains(QLatin1String(DeskoutPath))) {
            paths << QLatin1String(DeskoutPath);
            if (!writePaths(paths, error))
                return false;
        }

        const QList<QStringList> writes = {
            {QStringLiteral("name"), gvariantString(QStringLiteral("Deskout: pause/resume all reminders"))},
            {QStringLiteral("command"), gvariantString(togglePauseCommandLine())},
            {QStringLiteral("binding"), gvariantString(accel)},
        };
        for (const QStringList &kv : writes) {
            const GSettingsResult r = gsettings({QStringLiteral("set"), relocatable, kv[0], kv[1]});
            if (!r.ok)
                return fail(error, r.error);
        }
        return true;
    }

    void unregisterHotkey() override
    {
        QStringList paths;
        if (readPaths(&paths, nullptr) && paths.removeAll(QLatin1String(DeskoutPath)) > 0)
            writePaths(paths, nullptr);
        const QString relocatable = QLatin1String(CustomSchema) + QLatin1Char(':') + QLatin1String(DeskoutPath);
        for (const char *key : {"name", "command", "binding"})
            gsettings({QStringLiteral("reset"), relocatable, QLatin1String(key)});
    }

private:
    static bool fail(QString *error, const QString &detail)
    {
        if (error)
            *error = QCoreApplication::translate("GlobalHotkey", "Could not register GNOME shortcut: %1")
                         .arg(detail.isEmpty() ? QStringLiteral("gsettings failed") : detail);
        return false;
    }

    static bool readPaths(QStringList *paths, QString *error)
    {
        const GSettingsResult r = gsettings({QStringLiteral("get"), QLatin1String(MediaKeysSchema),
                                             QLatin1String(CustomListKey)});
        if (!r.ok)
            return fail(error, r.error);
        bool parsed = false;
        *paths = parseStringArray(r.output, &parsed);
        // Never overwrite the user's own shortcuts with a list we misread.
        if (!parsed)
            return fail(error, QStringLiteral("unexpected value \"%1\"").arg(r.output));
        return true;
    }

    static bool writePaths(const QStringList &paths, QString *error)
    {
        const GSettingsResult r = gsettings({QStringLiteral("set"), QLatin1String(MediaKeysSchema),
                                             QLatin1String(CustomListKey), serializeStringArray(paths)});
        return r.ok || fail(error, r.error);
    }
};

} // namespace

std::unique_ptr<HotkeyBackend> createGnomeHotkeyBackend(HotkeyBackend::Callback onActivated)
{
    return std::make_unique<GnomeHotkeyBackend>(std::move(onActivated));
}
