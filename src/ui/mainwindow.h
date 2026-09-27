#pragma once

#include <QMap>
#include <QMainWindow>

class PauseManager;
class QFrame;
class QLabel;
class QListWidget;
class QStackedWidget;
class ReminderEngine;

// Shell window: sidebar navigation + page stack. Feature pages are
// placeholders until their phase lands.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Rows of the "Status" card on the Home page.
    enum class StatusRow { Hotkey, AutoStart, Tray, Notifications, IdleDetection, FullscreenDetection };

    MainWindow(PauseManager *pause, ReminderEngine *reminders, QWidget *parent = nullptr);

    // When true (tray available), closing the window hides it instead of
    // quitting.
    void setHideOnClose(bool hide) { m_hideOnClose = hide; }

    void setStatus(StatusRow row, const QString &text);

Q_SIGNALS:
    void settingsRequested();
    void reminderSettingsRequested();
    void hiddenToTray();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildSidebar();
    QWidget *buildHomePage();
    QWidget *buildPlaceholderPage(const QString &title, const QString &description, int phase);
    QFrame *buildPauseBanner();
    void refreshPauseState();

    PauseManager *m_pause;
    ReminderEngine *m_reminders;
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    QFrame *m_pauseBanner = nullptr;
    QLabel *m_pauseBannerText = nullptr;
    QLabel *m_reminderStatus = nullptr;
    QMap<StatusRow, QLabel *> m_statusRows;
    bool m_hideOnClose = true;
};
