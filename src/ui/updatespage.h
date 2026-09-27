#pragma once

#include <QDate>
#include <QTimer>
#include <QWidget>

class DailyUpdatesStore;
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;
struct DailyUpdate;

// Daily Updates: today's entry (what I did, todos for tomorrow), saved as
// you type, above the full history of earlier days, newest first.
class UpdatesPage : public QWidget
{
    Q_OBJECT

public:
    explicit UpdatesPage(DailyUpdatesStore *store, QWidget *parent = nullptr);

    // Writes pending edits now.
    void save();
    void focusToday();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QWidget *buildTodayCard();
    QWidget *buildEntryCard(const DailyUpdate &update) const;
    // Switches the editor to the current date if midnight has passed.
    void checkDate();
    void loadToday();
    void reloadHistory();
    void loadOlder();
    void onEdited();
    void onStoreChanged(const QDate &day);
    void refreshStatus();

    DailyUpdatesStore *m_store;
    QDate m_day;
    QLabel *m_todayTitle = nullptr;
    QLabel *m_status = nullptr;
    QPlainTextEdit *m_done = nullptr;
    QPlainTextEdit *m_todo = nullptr;
    QCheckBox *m_recap = nullptr;
    QVBoxLayout *m_historyLayout = nullptr;
    QLabel *m_historyEmpty = nullptr;
    QPushButton *m_older = nullptr;
    QDate m_oldestShown;
    QTimer m_saveTimer;
    QTimer m_dateCheck;
    bool m_loading = false;
    bool m_dirty = false;
    bool m_saving = false;
};
