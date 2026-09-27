#pragma once

#include <QDialog>

class ActivityMonitor;
class GlobalHotkey;
class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class ReminderEngine;
class ReminderSettingsPage;

// Central settings dialog, reachable from the tray and the main window.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    struct Context
    {
        const GlobalHotkey *hotkey = nullptr;
        ReminderEngine *reminders = nullptr;
        const ActivityMonitor *activity = nullptr;
    };

    enum class Tab { General, Reminders, Detection };

    explicit SettingsDialog(const Context &context, QWidget *parent = nullptr);

    void showTab(Tab tab);

Q_SIGNALS:
    // Emitted after new values are written to QSettings / the OS, so the
    // app can re-apply them (theme, hotkey registration, detection).
    void applied();

private:
    QWidget *buildGeneralTab();
    QWidget *buildDetectionTab();
    void load();
    bool apply();
    void validateShortcut();
    void refreshHotkeyStatus();
    void refreshDetectionStatus();

    Context m_context;
    QTabWidget *m_tabs = nullptr;
    QCheckBox *m_autoStart = nullptr;
    QCheckBox *m_hotkeyEnabled = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QPushButton *m_hotkeyReset = nullptr;
    QLabel *m_hotkeyStatus = nullptr;
    QComboBox *m_theme = nullptr;
    ReminderSettingsPage *m_reminderPage = nullptr;
    QCheckBox *m_idleEnabled = nullptr;
    QSpinBox *m_idleMinutes = nullptr;
    QLabel *m_idleStatus = nullptr;
    QCheckBox *m_fullscreenEnabled = nullptr;
    QLabel *m_fullscreenStatus = nullptr;
};
