#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class PauseManager;
class QAction;
class QMenu;

// The tray icon is Deskout's persistent home: the app keeps running here
// after the main window is closed.
class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(PauseManager *pause, QObject *parent = nullptr);
    ~TrayController() override;

    void show();
    bool isVisible() const;
    void notify(const QString &title, const QString &message);

Q_SIGNALS:
    void openRequested();
    void settingsRequested();
    void quitRequested();

private:
    void refresh();

    PauseManager *m_pause;
    QSystemTrayIcon m_tray;
    QMenu *m_menu;
    QAction *m_statusAction;
    QAction *m_resumeAction;
    QMenu *m_pauseMenu;
};
