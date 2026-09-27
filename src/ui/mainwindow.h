#pragma once

#include <QMainWindow>

class PauseManager;
class QFrame;
class QLabel;
class QListWidget;
class QStackedWidget;

// Shell window: sidebar navigation + page stack. Feature pages are
// placeholders until their phase lands.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(PauseManager *pause, QWidget *parent = nullptr);

    // When true (tray available), closing the window hides it instead of
    // quitting.
    void setHideOnClose(bool hide) { m_hideOnClose = hide; }

    void setHotkeyStatus(const QString &text);
    void setAutoStartStatus(bool enabled);
    void setTrayStatus(const QString &text);

Q_SIGNALS:
    void settingsRequested();
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
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    QFrame *m_pauseBanner = nullptr;
    QLabel *m_pauseBannerText = nullptr;
    QLabel *m_reminderStatus = nullptr;
    QLabel *m_hotkeyStatus = nullptr;
    QLabel *m_autoStartStatus = nullptr;
    QLabel *m_trayStatus = nullptr;
    bool m_hideOnClose = true;
};
