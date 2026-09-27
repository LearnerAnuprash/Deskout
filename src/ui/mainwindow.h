#pragma once

#include <QMap>
#include <QMainWindow>

class DocsStore;
class FocusLog;
class FocusTimer;
class NotesStore;
class PauseManager;
class QFrame;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class ReminderEngine;
class DailyUpdatesStore;
class UpdatesPage;

// Shell window: sidebar navigation + page stack. Feature pages are
// placeholders until their phase lands.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Sidebar entries, in order.
    enum class Page {
        Home,
        Reminders,
        Focus,
        Notes,
        Docs,
        Updates,
        Stats,
    };

    // Rows of the "Status" card on the Home page.
    enum class StatusRow {
        Hotkey,
        AutoStart,
        Tray,
        Notifications,
        IdleDetection,
        FullscreenDetection,
        ReadingMode,
    };

    // The app-level objects the pages show and control.
    struct Context
    {
        PauseManager *pause;
        ReminderEngine *reminders;
        FocusTimer *focus;
        FocusLog *focusLog;
        NotesStore *notes;
        DocsStore *docs;
        DailyUpdatesStore *updates;
    };

    explicit MainWindow(const Context &context, QWidget *parent = nullptr);

    // When true (tray available), closing the window hides it instead of
    // quitting.
    void setHideOnClose(bool hide) { m_hideOnClose = hide; }

    void setStatus(StatusRow row, const QString &text);
    // Switches the sidebar to `page`; Updates also puts the cursor in
    // today's entry.
    void showPage(Page page);
    void setReadingMode(bool checked, const QString &toolTip);

Q_SIGNALS:
    void settingsRequested();
    void reminderSettingsRequested();
    void readingModeToggled(bool on);
    void focusWindowRequested();
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
    UpdatesPage *m_updatesPage = nullptr;
    QFrame *m_pauseBanner = nullptr;
    QLabel *m_pauseBannerText = nullptr;
    QLabel *m_reminderStatus = nullptr;
    QPushButton *m_readingModeButton = nullptr;
    QMap<StatusRow, QLabel *> m_statusRows;
    bool m_hideOnClose = true;
};
