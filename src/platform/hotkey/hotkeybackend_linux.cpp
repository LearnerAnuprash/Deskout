#include "platform/hotkey/linuxbackends.h"

#include "core/commands.h"
#include "platform/autostart.h"

#include <QCoreApplication>

namespace {

class ManualHotkeyBackend final : public HotkeyBackend
{
public:
    using HotkeyBackend::HotkeyBackend;

    QString name() const override
    {
        return QCoreApplication::translate("GlobalHotkey", "Desktop shortcut (manual)");
    }

    bool registerHotkey(QKeyCombination, QString *error) override
    {
        if (error)
            *error = QCoreApplication::translate(
                         "GlobalHotkey",
                         "This desktop doesn't let apps register global shortcuts. "
                         "Add a custom shortcut in your system keyboard settings that runs:\n%1")
                         .arg(togglePauseCommandLine());
        return false;
    }

    void unregisterHotkey() override {}
    bool activatesViaCommand() const override { return true; }
};

bool envContains(const char *name, const char *needle)
{
    return QString::fromLocal8Bit(qgetenv(name)).contains(QLatin1String(needle), Qt::CaseInsensitive);
}

} // namespace

QString togglePauseCommandLine()
{
    QString exe = AutoStart::executablePath();
    exe.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QStringLiteral("'%1' --%2").arg(exe, QLatin1String(Commands::TogglePause));
}

std::unique_ptr<HotkeyBackend> createManualHotkeyBackend(HotkeyBackend::Callback onActivated)
{
    return std::make_unique<ManualHotkeyBackend>(std::move(onActivated));
}

std::unique_ptr<HotkeyBackend> createHotkeyBackend(HotkeyBackend::Callback onActivated)
{
    // DESKOUT_HOTKEY_BACKEND=x11|gnome|manual forces a backend (for testing).
    const QByteArray forced = qgetenv("DESKOUT_HOTKEY_BACKEND").toLower();
    if (forced == "x11")
        return createX11HotkeyBackend(std::move(onActivated));
    if (forced == "gnome")
        return createGnomeHotkeyBackend(std::move(onActivated));
    if (forced == "manual")
        return createManualHotkeyBackend(std::move(onActivated));

    const bool wayland = envContains("XDG_SESSION_TYPE", "wayland")
                         || (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
                             && !envContains("XDG_SESSION_TYPE", "x11"));
    // On Wayland an X11 grab only fires while an XWayland window has focus,
    // which defeats the purpose, so only use it on real X11 sessions.
    if (!wayland && !qEnvironmentVariableIsEmpty("DISPLAY"))
        return createX11HotkeyBackend(std::move(onActivated));
    if (envContains("XDG_CURRENT_DESKTOP", "gnome"))
        return createGnomeHotkeyBackend(std::move(onActivated));
    return createManualHotkeyBackend(std::move(onActivated));
}
