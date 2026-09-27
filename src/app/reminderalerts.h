#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

class ActivityMonitor;
class FullScreenAlarm;
class Notifier;
class PauseManager;
class ReminderEngine;

// Decides how a due reminder reaches the user and feeds the response back
// to the engine:
//  - full-screen alarm when the reminder wants one and nothing forbids it,
//  - a notification when it is notification-only or another app is
//    full-screen (call, presentation, video),
//  - alarms queue up one at a time; a global pause clears them.
class ReminderAlertController : public QObject
{
    Q_OBJECT

public:
    ReminderAlertController(ReminderEngine *engine, ActivityMonitor *activity, Notifier *notifier,
                            PauseManager *pause, QObject *parent = nullptr);
    ~ReminderAlertController() override;

private:
    void onDue(const QString &id);
    void showNextAlarm();
    void notify(const QString &id, const QString &note = QString());
    void onPausedChanged(bool paused);
    void onNotificationAction(uint notificationId, const QString &actionKey);
    void disableFullScreen(const QString &id);

    ReminderEngine *m_engine;
    ActivityMonitor *m_activity;
    Notifier *m_notifier;
    QStringList m_queue;
    QPointer<FullScreenAlarm> m_alarm;
    QHash<uint, QString> m_notifications; // notification id -> reminder id
};
