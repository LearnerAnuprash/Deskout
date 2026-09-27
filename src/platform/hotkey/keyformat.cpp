#include "platform/hotkey/keyformat.h"

#include <QCoreApplication>

namespace KeyFormat {

QString keysymName(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return QString(QChar(char16_t(u'a' + (key - Qt::Key_A))));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return QString(QChar(char16_t(u'0' + (key - Qt::Key_0))));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);

    switch (key) {
    case Qt::Key_Space: return QStringLiteral("space");
    case Qt::Key_Return: return QStringLiteral("Return");
    case Qt::Key_Escape: return QStringLiteral("Escape");
    case Qt::Key_Backspace: return QStringLiteral("BackSpace");
    case Qt::Key_Insert: return QStringLiteral("Insert");
    case Qt::Key_Delete: return QStringLiteral("Delete");
    case Qt::Key_Home: return QStringLiteral("Home");
    case Qt::Key_End: return QStringLiteral("End");
    case Qt::Key_PageUp: return QStringLiteral("Page_Up");
    case Qt::Key_PageDown: return QStringLiteral("Page_Down");
    case Qt::Key_Left: return QStringLiteral("Left");
    case Qt::Key_Right: return QStringLiteral("Right");
    case Qt::Key_Up: return QStringLiteral("Up");
    case Qt::Key_Down: return QStringLiteral("Down");
    case Qt::Key_Pause: return QStringLiteral("Pause");
    case Qt::Key_Print: return QStringLiteral("Print");
    case Qt::Key_ScrollLock: return QStringLiteral("Scroll_Lock");
    case Qt::Key_Minus: return QStringLiteral("minus");
    case Qt::Key_Equal: return QStringLiteral("equal");
    case Qt::Key_Comma: return QStringLiteral("comma");
    case Qt::Key_Period: return QStringLiteral("period");
    case Qt::Key_Slash: return QStringLiteral("slash");
    case Qt::Key_Backslash: return QStringLiteral("backslash");
    case Qt::Key_Semicolon: return QStringLiteral("semicolon");
    case Qt::Key_Apostrophe: return QStringLiteral("apostrophe");
    case Qt::Key_BracketLeft: return QStringLiteral("bracketleft");
    case Qt::Key_BracketRight: return QStringLiteral("bracketright");
    case Qt::Key_QuoteLeft: return QStringLiteral("grave");
    default: return QString();
    }
}

QString gtkAccelerator(QKeyCombination combo)
{
    const QString key = keysymName(combo.key());
    if (key.isEmpty())
        return QString();

    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    QString accel;
    if (mods & Qt::ControlModifier)
        accel += QStringLiteral("<Control>");
    if (mods & Qt::AltModifier)
        accel += QStringLiteral("<Alt>");
    if (mods & Qt::ShiftModifier)
        accel += QStringLiteral("<Shift>");
    if (mods & Qt::MetaModifier)
        accel += QStringLiteral("<Super>");
    return accel + key;
}

bool isUsableGlobalShortcut(QKeyCombination combo, QString *reason)
{
    const auto fail = [reason](const char *text) {
        if (reason)
            *reason = QCoreApplication::translate("KeyFormat", text);
        return false;
    };

    if (combo.key() == Qt::Key_unknown || combo.toCombined() == 0)
        return fail("No shortcut set.");
    const Qt::KeyboardModifiers required = Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    if (!(combo.keyboardModifiers() & required))
        return fail("Use at least one of Ctrl, Alt or Super/Cmd.");
    if (keysymName(combo.key()).isEmpty())
        return fail("That key can't be used for a global shortcut. Try a letter, digit or F-key.");
    return true;
}

} // namespace KeyFormat
