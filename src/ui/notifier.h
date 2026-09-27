#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class TrayController;

// Desktop notifications. On Linux they go straight to the freedesktop
// notification service (works without a tray icon and supports action
// buttons); elsewhere, and as a fallback, through the tray icon.
class Notifier : public QObject
{
    Q_OBJECT

public:
    struct Action
    {
        QString key;
        QString label;
    };

    explicit Notifier(QObject *parent = nullptr);

    void setTray(TrayController *tray);

    // Returns an id to match against actionInvoked(), or 0 if the
    // notification could not be shown with actions.
    uint notify(const QString &title, const QString &body, const QList<Action> &actions = {});

    QString backendName() const;

Q_SIGNALS:
    void actionInvoked(uint id, const QString &actionKey);

private Q_SLOTS:
    void onDBusActionInvoked(uint id, const QString &actionKey);

private:
    bool m_dbusAvailable = false;
    QString m_iconPath;
    QPointer<TrayController> m_tray;
};
