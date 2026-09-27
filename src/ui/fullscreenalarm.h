#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

class AlarmWindow;
class QLabel;
class QPushButton;
class QStackedWidget;

// Alarm-style takeover covering every monitor. Only its own buttons close
// it: clicking elsewhere, Escape and Alt+F4 do nothing. The main screen
// shows the controls; other screens show a dimmed "break time" notice.
class FullScreenAlarm : public QObject
{
    Q_OBJECT

public:
    explicit FullScreenAlarm(const QString &reminderId, QObject *parent = nullptr);
    ~FullScreenAlarm() override;

    QString reminderId() const { return m_reminderId; }
    void show();
    // Close without a user response (e.g. global pause). Emits finished().
    void dismiss();

Q_SIGNALS:
    void confirmed();
    void snoozed(int minutes);
    void fullScreenDisabled();
    // Always emitted exactly once, after any of the above.
    void finished();

private:
    QWidget *buildPromptPage();
    QWidget *buildExercisePage();
    QWidget *buildSecondaryContent();
    void onConfirm();
    void startExercise();
    void tickExercise();
    void finish();

    QString m_reminderId;
    QList<QPointer<AlarmWindow>> m_windows;
    QStackedWidget *m_stack = nullptr;
    QList<QPushButton *> m_promptButtons;
    QLabel *m_exerciseTitle = nullptr;
    QLabel *m_exerciseText = nullptr;
    QLabel *m_countdown = nullptr;
    QPushButton *m_exerciseButton = nullptr;
    QTimer m_exerciseTimer;
    QTimer m_autoClose;
    int m_secondsLeft = 0;
    bool m_finished = false;
};
