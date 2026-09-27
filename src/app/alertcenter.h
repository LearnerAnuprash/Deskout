#pragma once

#include "core/alert.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>

class ActivityMonitor;
class FullScreenAlarm;
class Notifier;
class PauseManager;

// The one place that decides how an alert reaches the user, for reminders
// and the focus timer alike:
//  - nothing while everything is paused,
//  - a notification when the alert is notification-only or another app is
//    full-screen (call, presentation, video),
//  - otherwise a full-screen alarm; alarms queue up one at a time.
// Sources learn the outcome through the signals, matched by Alert::key.
class AlertCenter : public QObject
{
    Q_OBJECT

public:
    AlertCenter(ActivityMonitor *activity, Notifier *notifier, PauseManager *pause,
                QObject *parent = nullptr);
    ~AlertCenter() override;

    // Ignored if an alert with the same key is already showing or queued.
    void raise(const Alert &alert);

    bool isShowing(const QString &key) const;

Q_SIGNALS:
    // A button on the alarm or notification: Alert::ConfirmKey or an
    // AlertAction key.
    void responded(const QString &key, const QString &actionKey);
    // Delivered as a notification, which needs no answer.
    void notified(const QString &key);
    // The user asked for notifications instead of this alarm from now on.
    void fullScreenDisabled(const QString &key);
    // Dropped unanswered because everything was (or got) paused.
    void interrupted(const QString &key);

private:
    void showNext();
    void notify(const Alert &alert, const QString &note = QString());
    void onPausedChanged(bool paused);
    void onNotificationAction(uint notificationId, const QString &actionKey);

    ActivityMonitor *m_activity;
    Notifier *m_notifier;
    PauseManager *m_pause;
    QList<Alert> m_queue;
    QPointer<FullScreenAlarm> m_alarm;
    QHash<uint, QString> m_notifications; // notification id -> alert key
};
