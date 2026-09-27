#pragma once

#include <QWidget>

class FocusLog;
class FocusTimer;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class TimeDisplay;

// Main-window page for the research/focus timer: pick a topic and
// duration, start/pause/stop, pop out the floating timer, and see today's
// sessions.
class FocusPage : public QWidget
{
    Q_OBJECT

public:
    FocusPage(FocusTimer *timer, FocusLog *log, QWidget *parent = nullptr);

Q_SIGNALS:
    void popOutRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QWidget *buildTimerCard();
    QWidget *buildHistoryCard();
    void onStartClicked();
    void onInputsEdited();
    void refreshState();
    void refreshTime();
    void refreshHistory();

    FocusTimer *m_timer;
    FocusLog *m_log;
    QLineEdit *m_topic = nullptr;
    QSpinBox *m_minutes = nullptr;
    QList<QWidget *> m_presets;
    TimeDisplay *m_display = nullptr;
    QLabel *m_caption = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_stop = nullptr;
    QCheckBox *m_fullScreenAlert = nullptr;
    QLabel *m_todaySummary = nullptr;
    QVBoxLayout *m_recentLayout = nullptr;
};
