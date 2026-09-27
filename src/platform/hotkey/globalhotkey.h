#pragma once

#include <QKeySequence>
#include <QObject>

#include <memory>

class HotkeyBackend;

// System-wide keyboard shortcut that works while Deskout is unfocused or
// hidden in the tray. Wraps the platform backend chosen at startup.
class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // Registers `sequence` (only its first key combination is used),
    // replacing any previous registration. Returns false and sets
    // lastError() on failure.
    bool setShortcut(const QKeySequence &sequence);
    void clear();

    QKeySequence shortcut() const { return m_shortcut; }
    bool isRegistered() const { return m_registered; }
    QString backendName() const;
    QString lastError() const { return m_lastError; }

Q_SIGNALS:
    void activated();

private:
    std::unique_ptr<HotkeyBackend> m_backend;
    QKeySequence m_shortcut;
    bool m_registered = false;
    QString m_lastError;
};
