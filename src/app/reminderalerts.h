#pragma once

#include <QObject>

class AlertCenter;
class Notifier;
class ReminderEngine;

// Turns due reminders into alerts and feeds the user's response back to
// the engine. How the alert is shown is AlertCenter's decision.
class ReminderAlertController : public QObject
{
    Q_OBJECT

public:
    ReminderAlertController(ReminderEngine *engine, AlertCenter *alerts, Notifier *notifier,
                            QObject *parent = nullptr);

private:
    void onDue(const QString &id);
    void onResponded(const QString &key, const QString &actionKey);
    void onNotified(const QString &key);
    void onInterrupted(const QString &key);
    void disableFullScreen(const QString &key);

    ReminderEngine *m_engine;
    AlertCenter *m_alerts;
    Notifier *m_notifier;
};
