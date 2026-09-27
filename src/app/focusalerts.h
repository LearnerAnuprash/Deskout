#pragma once

#include <QObject>

class AlertCenter;
class FocusTimer;
class Notifier;
struct FocusSession;

// Announces a finished focus session through AlertCenter, so it follows
// the same rules as reminders (paused, another app full-screen, ...).
class FocusAlertController : public QObject
{
    Q_OBJECT

public:
    static constexpr char AlertKey[] = "focus/complete";
    static constexpr char StartAgainKey[] = "start-again";

    FocusAlertController(FocusTimer *timer, AlertCenter *alerts, Notifier *notifier,
                         QObject *parent = nullptr);

    static bool fullScreenEnabled();
    static void setFullScreenEnabled(bool enabled);

private:
    void onSessionEnded(const FocusSession &session);
    void onResponded(const QString &key, const QString &actionKey);
    void onFullScreenDisabled(const QString &key);

    FocusTimer *m_timer;
    AlertCenter *m_alerts;
    Notifier *m_notifier;
};
