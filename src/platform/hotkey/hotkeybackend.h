#pragma once

#include <QKeyCombination>
#include <QString>

#include <functional>
#include <memory>

// One native implementation of a system-wide hotkey. Exactly one backend is
// compiled per platform; Linux picks X11 or GNOME at runtime.
class HotkeyBackend
{
public:
    using Callback = std::function<void()>;

    explicit HotkeyBackend(Callback onActivated)
        : m_onActivated(std::move(onActivated))
    {
    }
    virtual ~HotkeyBackend() = default;

    HotkeyBackend(const HotkeyBackend &) = delete;
    HotkeyBackend &operator=(const HotkeyBackend &) = delete;

    // Short user-facing description, e.g. "Win32 RegisterHotKey".
    virtual QString name() const = 0;
    virtual bool registerHotkey(QKeyCombination combo, QString *error) = 0;
    virtual void unregisterHotkey() = 0;

    // True when activation arrives as a `deskout --toggle-pause` command
    // (desktop-owned shortcut) instead of through the callback.
    virtual bool activatesViaCommand() const { return false; }

protected:
    void activate() const
    {
        if (m_onActivated)
            m_onActivated();
    }

private:
    Callback m_onActivated;
};

// Implemented in the per-platform hotkeybackend_*.cpp file.
std::unique_ptr<HotkeyBackend> createHotkeyBackend(HotkeyBackend::Callback onActivated);
