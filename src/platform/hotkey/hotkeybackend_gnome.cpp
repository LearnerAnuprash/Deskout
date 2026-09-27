#include "platform/hotkey/keyformat.h"
#include "platform/hotkey/linuxbackends.h"
#include "platform/linux/gsettings.h"

#include <QCoreApplication>
#include <QStringList>

namespace {

constexpr char MediaKeysSchema[] = "org.gnome.settings-daemon.plugins.media-keys";
constexpr char CustomListKey[] = "custom-keybindings";
constexpr char CustomSchema[] = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr char DeskoutPath[] = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/deskout-pause/";

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
        QString detail;
        if (!GSettings::readStringList(QLatin1String(MediaKeysSchema), QLatin1String(CustomListKey), &paths, &detail))
            return fail(error, detail);
        if (!paths.contains(QLatin1String(DeskoutPath))) {
            paths << QLatin1String(DeskoutPath);
            if (!GSettings::writeStringList(QLatin1String(MediaKeysSchema), QLatin1String(CustomListKey), paths, &detail))
                return fail(error, detail);
        }

        const QList<QStringList> writes = {
            {QStringLiteral("name"), GSettings::quote(QStringLiteral("Deskout: pause/resume all reminders"))},
            {QStringLiteral("command"), GSettings::quote(togglePauseCommandLine())},
            {QStringLiteral("binding"), GSettings::quote(accel)},
        };
        for (const QStringList &kv : writes) {
            const GSettings::Result r = GSettings::run({QStringLiteral("set"), relocatable, kv[0], kv[1]});
            if (!r.ok)
                return fail(error, r.error);
        }
        return true;
    }

    void unregisterHotkey() override
    {
        QStringList paths;
        if (GSettings::readStringList(QLatin1String(MediaKeysSchema), QLatin1String(CustomListKey), &paths, nullptr)
            && paths.removeAll(QLatin1String(DeskoutPath)) > 0)
            GSettings::writeStringList(QLatin1String(MediaKeysSchema), QLatin1String(CustomListKey), paths, nullptr);
        const QString relocatable = QLatin1String(CustomSchema) + QLatin1Char(':') + QLatin1String(DeskoutPath);
        for (const char *key : {"name", "command", "binding"})
            GSettings::run({QStringLiteral("reset"), relocatable, QLatin1String(key)});
    }

private:
    static bool fail(QString *error, const QString &detail)
    {
        if (error)
            *error = QCoreApplication::translate("GlobalHotkey", "Could not register GNOME shortcut: %1")
                         .arg(detail.isEmpty() ? QStringLiteral("gsettings failed") : detail);
        return false;
    }
};

} // namespace

std::unique_ptr<HotkeyBackend> createGnomeHotkeyBackend(HotkeyBackend::Callback onActivated)
{
    return std::make_unique<GnomeHotkeyBackend>(std::move(onActivated));
}
