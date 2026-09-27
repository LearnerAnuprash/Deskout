#include "platform/hotkey/keyformat.h"
#include "platform/hotkey/linuxbackends.h"

#include <QCoreApplication>
#include <QSocketNotifier>

// X11 headers define macros (None, KeyPress, Bool...) that clash with Qt, so
// they come last and nothing Qt-heavy follows.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>

namespace {

bool g_grabFailed = false;

int grabErrorHandler(Display *, XErrorEvent *event)
{
    if (event->error_code == BadAccess)
        g_grabFailed = true;
    return 0;
}

// Grab with every combination of the "lock" modifiers so the hotkey still
// fires when Caps Lock or Num Lock is on.
constexpr unsigned int LockVariants[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
constexpr unsigned int RelevantMods = ShiftMask | ControlMask | Mod1Mask | Mod4Mask;

class X11HotkeyBackend final : public HotkeyBackend
{
public:
    using HotkeyBackend::HotkeyBackend;

    ~X11HotkeyBackend() override
    {
        unregisterHotkey();
        m_notifier.reset();
        if (m_display)
            XCloseDisplay(m_display);
    }

    QString name() const override
    {
        return QCoreApplication::translate("GlobalHotkey", "X11 key grab");
    }

    bool registerHotkey(QKeyCombination combo, QString *error) override
    {
        unregisterHotkey();
        if (!ensureDisplay(error))
            return false;

        const QByteArray keysymName = KeyFormat::keysymName(combo.key()).toLatin1();
        const KeySym keysym = XStringToKeysym(keysymName.constData());
        const KeyCode keycode = keysym == NoSymbol ? 0 : XKeysymToKeycode(m_display, keysym);
        if (keycode == 0) {
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey",
                                                     "That key doesn't exist on the current keyboard layout.");
            return false;
        }

        unsigned int mods = 0;
        const Qt::KeyboardModifiers qtMods = combo.keyboardModifiers();
        if (qtMods & Qt::ShiftModifier)
            mods |= ShiftMask;
        if (qtMods & Qt::ControlModifier)
            mods |= ControlMask;
        if (qtMods & Qt::AltModifier)
            mods |= Mod1Mask;
        if (qtMods & Qt::MetaModifier)
            mods |= Mod4Mask;

        // Grab errors arrive asynchronously; sync so we see them now.
        const Window root = DefaultRootWindow(m_display);
        g_grabFailed = false;
        auto *previousHandler = XSetErrorHandler(grabErrorHandler);
        for (unsigned int variant : LockVariants)
            XGrabKey(m_display, keycode, mods | variant, root, False, GrabModeAsync, GrabModeAsync);
        XSync(m_display, False);
        XSetErrorHandler(previousHandler);

        m_keycode = keycode;
        m_mods = mods;
        m_grabbed = true;
        if (g_grabFailed) {
            unregisterHotkey();
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey",
                                                     "That shortcut is already taken by another application.");
            return false;
        }
        return true;
    }

    void unregisterHotkey() override
    {
        if (!m_grabbed || !m_display)
            return;
        const Window root = DefaultRootWindow(m_display);
        for (unsigned int variant : LockVariants)
            XUngrabKey(m_display, m_keycode, m_mods | variant, root);
        XFlush(m_display);
        m_grabbed = false;
        m_keyDown = false;
    }

private:
    // A dedicated X connection keeps us independent of which Qt platform
    // plugin is active; its socket is watched by the Qt event loop.
    bool ensureDisplay(QString *error)
    {
        if (m_display)
            return true;
        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey", "Could not connect to the X server.");
            return false;
        }
        // Report held keys as repeated KeyPress without fake KeyRelease
        // events, so holding the shortcut toggles only once.
        XkbSetDetectableAutoRepeat(m_display, True, nullptr);
        m_notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(m_display), QSocketNotifier::Read);
        QObject::connect(m_notifier.get(), &QSocketNotifier::activated, [this] { processEvents(); });
        return true;
    }

    void processEvents()
    {
        while (XPending(m_display)) {
            XEvent event;
            XNextEvent(m_display, &event);
            if (!m_grabbed || event.xkey.keycode != m_keycode)
                continue;
            if (event.type == KeyRelease) {
                m_keyDown = false;
            } else if (event.type == KeyPress && !m_keyDown
                       && (event.xkey.state & RelevantMods) == m_mods) {
                m_keyDown = true;
                activate();
            }
        }
    }

    Display *m_display = nullptr;
    std::unique_ptr<QSocketNotifier> m_notifier;
    unsigned int m_keycode = 0;
    unsigned int m_mods = 0;
    bool m_grabbed = false;
    bool m_keyDown = false;
};

} // namespace

std::unique_ptr<HotkeyBackend> createX11HotkeyBackend(HotkeyBackend::Callback onActivated)
{
    return std::make_unique<X11HotkeyBackend>(std::move(onActivated));
}
