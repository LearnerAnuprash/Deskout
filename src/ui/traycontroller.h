#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class FocusTimer;
class PauseManager;
class QAction;
class QMenu;

// The tray icon is Deskout's persistent home: the app keeps running here
// after the main window is closed.
class TrayController : public QObject
{
    Q_OBJECT

public:
    TrayController(PauseManager *pause, FocusTimer *focus, QObject *parent = nullptr);
    ~TrayController() override;

    void show();
    bool isVisible() const;
    void notify(const QString &title, const QString &message);
    void setReadingModeChecked(bool checked);

Q_SIGNALS:
    void openRequested();
    void settingsRequested();
    void readingModeToggled(bool on);
    void focusWindowRequested();
    void quitRequested();

private:
    void refresh();
    void refreshFocus();
    void onFocusActionTriggered();

    PauseManager *m_pause;
    FocusTimer *m_focus;
    QSystemTrayIcon m_tray;
    QMenu *m_menu;
    QAction *m_statusAction;
    QAction *m_resumeAction;
    QMenu *m_pauseMenu;
    QAction *m_readingModeAction;
    QAction *m_focusAction;
    QAction *m_focusStopAction;
};
