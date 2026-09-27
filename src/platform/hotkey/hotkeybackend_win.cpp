#include "platform/hotkey/hotkeybackend.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>

#include <windows.h>

namespace {

constexpr int HotkeyId = 0x0D01; // app-defined ids must be below 0xC000

UINT virtualKeyFor(Qt::Key key)
{
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9))
        return UINT(key); // VK codes for letters/digits equal their ASCII value
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + UINT(key - Qt::Key_F1);

    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Slash: return VK_OEM_2;
    case Qt::Key_Semicolon: return VK_OEM_1;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    case Qt::Key_BracketLeft: return VK_OEM_4;
    case Qt::Key_Backslash: return VK_OEM_5;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Apostrophe: return VK_OEM_7;
    default: return 0;
    }
}

class WinHotkeyBackend final : public HotkeyBackend, public QAbstractNativeEventFilter
{
public:
    explicit WinHotkeyBackend(Callback onActivated)
        : HotkeyBackend(std::move(onActivated))
    {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }

    ~WinHotkeyBackend() override
    {
        unregisterHotkey();
        if (auto *app = QCoreApplication::instance())
            app->removeNativeEventFilter(this);
    }

    QString name() const override
    {
        return QCoreApplication::translate("GlobalHotkey", "Windows RegisterHotKey");
    }

    bool registerHotkey(QKeyCombination combo, QString *error) override
    {
        unregisterHotkey();
        const UINT vk = virtualKeyFor(combo.key());
        if (vk == 0) {
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey", "That key can't be used on Windows.");
            return false;
        }
        UINT mods = MOD_NOREPEAT;
        const Qt::KeyboardModifiers qtMods = combo.keyboardModifiers();
        if (qtMods & Qt::ControlModifier)
            mods |= MOD_CONTROL;
        if (qtMods & Qt::AltModifier)
            mods |= MOD_ALT;
        if (qtMods & Qt::ShiftModifier)
            mods |= MOD_SHIFT;
        if (qtMods & Qt::MetaModifier)
            mods |= MOD_WIN;

        // A null HWND posts WM_HOTKEY to this thread's message queue, which
        // Qt's event dispatcher hands to native event filters.
        if (!RegisterHotKey(nullptr, HotkeyId, mods, vk)) {
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey",
                                                     "That shortcut is already taken by another application.");
            return false;
        }
        m_registered = true;
        return true;
    }

    void unregisterHotkey() override
    {
        if (m_registered)
            UnregisterHotKey(nullptr, HotkeyId);
        m_registered = false;
    }

    bool nativeEventFilter(const QByteArray &, void *message, qintptr *) override
    {
        const MSG *msg = static_cast<const MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == WPARAM(HotkeyId)) {
            activate();
            return true;
        }
        return false;
    }

private:
    bool m_registered = false;
};

} // namespace

std::unique_ptr<HotkeyBackend> createHotkeyBackend(HotkeyBackend::Callback onActivated)
{
    return std::make_unique<WinHotkeyBackend>(std::move(onActivated));
}
