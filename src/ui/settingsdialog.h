#pragma once

#include <QDialog>

class GlobalHotkey;
class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QPushButton;

// Central settings. Phase 0 covers the "General" tab; later phases add
// reminder, detection and backup tabs here.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const GlobalHotkey *hotkey, QWidget *parent = nullptr);

Q_SIGNALS:
    // Emitted after new values are written to QSettings / the OS, so the
    // app can re-apply them (theme, hotkey registration).
    void applied();

private:
    QWidget *buildGeneralTab();
    void load();
    bool apply();
    void validateShortcut();
    void refreshHotkeyStatus();

    const GlobalHotkey *m_hotkey;
    QCheckBox *m_autoStart = nullptr;
    QCheckBox *m_hotkeyEnabled = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QPushButton *m_hotkeyReset = nullptr;
    QLabel *m_hotkeyStatus = nullptr;
    QComboBox *m_theme = nullptr;
};
