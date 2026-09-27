#include "platform/hotkey/globalhotkey.h"

#include "platform/hotkey/hotkeybackend.h"
#include "platform/hotkey/keyformat.h"

#include <QMetaObject>

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
    // Backends may call back from a native event filter; queue the signal so
    // slots never run inside the platform's event dispatch.
    m_backend = createHotkeyBackend([this] {
        QMetaObject::invokeMethod(this, [this] { Q_EMIT activated(); }, Qt::QueuedConnection);
    });
}

GlobalHotkey::~GlobalHotkey()
{
    clear();
}

bool GlobalHotkey::setShortcut(const QKeySequence &sequence)
{
    clear();
    m_shortcut = sequence;
    m_lastError.clear();

    if (!m_backend) {
        m_lastError = tr("Global shortcuts are not supported on this platform.");
        return false;
    }
    if (sequence.isEmpty()) {
        m_lastError = tr("No shortcut set.");
        return false;
    }
    const QKeyCombination combo = sequence[0];
    if (!KeyFormat::isUsableGlobalShortcut(combo, &m_lastError))
        return false;

    m_registered = m_backend->registerHotkey(combo, &m_lastError);
    return m_registered;
}

void GlobalHotkey::clear()
{
    if (m_backend && m_registered)
        m_backend->unregisterHotkey();
    m_registered = false;
}

QString GlobalHotkey::backendName() const
{
    return m_backend ? m_backend->name() : tr("Unavailable");
}
