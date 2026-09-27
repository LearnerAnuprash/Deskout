#include "platform/hotkey/hotkeybackend.h"

#include <QCoreApplication>

#include <Carbon/Carbon.h>

namespace {

constexpr OSType HotkeySignature = 'DSKT';
constexpr UInt32 HotkeyId = 1;

// Carbon virtual key codes (ANSI layout positions).
int keyCodeFor(Qt::Key key)
{
    static const int letters[] = {
        kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
        kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N,
        kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
        kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z};
    static const int digits[] = {kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
                                 kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
    static const int functionKeys[] = {kVK_F1,  kVK_F2,  kVK_F3,  kVK_F4,  kVK_F5,
                                       kVK_F6,  kVK_F7,  kVK_F8,  kVK_F9,  kVK_F10,
                                       kVK_F11, kVK_F12, kVK_F13, kVK_F14, kVK_F15,
                                       kVK_F16, kVK_F17, kVK_F18, kVK_F19, kVK_F20};

    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return letters[key - Qt::Key_A];
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return digits[key - Qt::Key_0];
    if (key >= Qt::Key_F1 && key <= Qt::Key_F20)
        return functionKeys[key - Qt::Key_F1];

    switch (key) {
    case Qt::Key_Space: return kVK_Space;
    case Qt::Key_Return: return kVK_Return;
    case Qt::Key_Escape: return kVK_Escape;
    case Qt::Key_Backspace: return kVK_Delete;
    case Qt::Key_Delete: return kVK_ForwardDelete;
    case Qt::Key_Home: return kVK_Home;
    case Qt::Key_End: return kVK_End;
    case Qt::Key_PageUp: return kVK_PageUp;
    case Qt::Key_PageDown: return kVK_PageDown;
    case Qt::Key_Left: return kVK_LeftArrow;
    case Qt::Key_Right: return kVK_RightArrow;
    case Qt::Key_Up: return kVK_UpArrow;
    case Qt::Key_Down: return kVK_DownArrow;
    case Qt::Key_Minus: return kVK_ANSI_Minus;
    case Qt::Key_Equal: return kVK_ANSI_Equal;
    case Qt::Key_Comma: return kVK_ANSI_Comma;
    case Qt::Key_Period: return kVK_ANSI_Period;
    case Qt::Key_Slash: return kVK_ANSI_Slash;
    case Qt::Key_Backslash: return kVK_ANSI_Backslash;
    case Qt::Key_Semicolon: return kVK_ANSI_Semicolon;
    case Qt::Key_Apostrophe: return kVK_ANSI_Quote;
    case Qt::Key_BracketLeft: return kVK_ANSI_LeftBracket;
    case Qt::Key_BracketRight: return kVK_ANSI_RightBracket;
    case Qt::Key_QuoteLeft: return kVK_ANSI_Grave;
    default: return -1;
    }
}

class MacHotkeyBackend final : public HotkeyBackend
{
public:
    explicit MacHotkeyBackend(Callback onActivated)
        : HotkeyBackend(std::move(onActivated))
    {
        const EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
        InstallApplicationEventHandler(&MacHotkeyBackend::handler, 1, &spec, this, &m_handler);
    }

    ~MacHotkeyBackend() override
    {
        unregisterHotkey();
        if (m_handler)
            RemoveEventHandler(m_handler);
    }

    QString name() const override
    {
        return QCoreApplication::translate("GlobalHotkey", "macOS Carbon hotkey");
    }

    bool registerHotkey(QKeyCombination combo, QString *error) override
    {
        unregisterHotkey();
        const int keyCode = keyCodeFor(combo.key());
        if (keyCode < 0) {
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey", "That key can't be used on macOS.");
            return false;
        }
        // Qt maps Cmd to ControlModifier and Ctrl to MetaModifier on macOS.
        UInt32 mods = 0;
        const Qt::KeyboardModifiers qtMods = combo.keyboardModifiers();
        if (qtMods & Qt::ControlModifier)
            mods |= cmdKey;
        if (qtMods & Qt::AltModifier)
            mods |= optionKey;
        if (qtMods & Qt::ShiftModifier)
            mods |= shiftKey;
        if (qtMods & Qt::MetaModifier)
            mods |= controlKey;

        const EventHotKeyID id = {HotkeySignature, HotkeyId};
        if (RegisterEventHotKey(UInt32(keyCode), mods, id, GetApplicationEventTarget(), 0, &m_hotkey)
            != noErr) {
            m_hotkey = nullptr;
            if (error)
                *error = QCoreApplication::translate("GlobalHotkey",
                                                     "That shortcut is already taken by another application.");
            return false;
        }
        return true;
    }

    void unregisterHotkey() override
    {
        if (m_hotkey)
            UnregisterEventHotKey(m_hotkey);
        m_hotkey = nullptr;
    }

private:
    static OSStatus handler(EventHandlerCallRef, EventRef event, void *userData)
    {
        EventHotKeyID id = {};
        GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(id),
                          nullptr, &id);
        if (id.signature == HotkeySignature && id.id == HotkeyId) {
            static_cast<MacHotkeyBackend *>(userData)->activate();
            return noErr;
        }
        return eventNotHandledErr;
    }

    EventHandlerRef m_handler = nullptr;
    EventHotKeyRef m_hotkey = nullptr;
};

} // namespace

std::unique_ptr<HotkeyBackend> createHotkeyBackend(HotkeyBackend::Callback onActivated)
{
    return std::make_unique<MacHotkeyBackend>(std::move(onActivated));
}
