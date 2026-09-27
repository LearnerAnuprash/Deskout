#include "ui/notifier.h"

#include "ui/appicon.h"
#include "ui/traycontroller.h"

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
#define DESKOUT_DBUS_NOTIFICATIONS 1
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#endif

namespace {
#ifdef DESKOUT_DBUS_NOTIFICATIONS
constexpr char Service[] = "org.freedesktop.Notifications";
constexpr char Path[] = "/org/freedesktop/Notifications";
constexpr char Interface[] = "org.freedesktop.Notifications";
#endif
} // namespace

Notifier::Notifier(QObject *parent)
    : QObject(parent)
{
#ifdef DESKOUT_DBUS_NOTIFICATIONS
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_dbusAvailable = bus.isConnected() && bus.interface()
                      && bus.interface()->isServiceRegistered(QLatin1String(Service));
    if (m_dbusAvailable) {
        bus.connect(QLatin1String(Service), QLatin1String(Path), QLatin1String(Interface),
                    QStringLiteral("ActionInvoked"), this, SLOT(onDBusActionInvoked(uint,QString)));
        m_iconPath = AppIcon::exportPng();
    }
#endif
}

void Notifier::setTray(TrayController *tray)
{
    m_tray = tray;
}

uint Notifier::notify(const QString &title, const QString &body, const QList<Action> &actions)
{
#ifdef DESKOUT_DBUS_NOTIFICATIONS
    if (m_dbusAvailable) {
        QStringList actionList;
        for (const Action &action : actions)
            actionList << action.key << action.label;
        QVariantMap hints;
        hints.insert(QStringLiteral("urgency"), QVariant::fromValue(uchar(1)));
        hints.insert(QStringLiteral("category"), QStringLiteral("presence"));

        QDBusMessage message = QDBusMessage::createMethodCall(
            QLatin1String(Service), QLatin1String(Path), QLatin1String(Interface), QStringLiteral("Notify"));
        message.setArguments({QStringLiteral("Deskout"), uint(0), m_iconPath, title, body, actionList,
                              hints, int(-1)});
        const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 1000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            return reply.arguments().constFirst().toUInt();
    }
#endif
    Q_UNUSED(actions)
    if (m_tray)
        m_tray->notify(title, body);
    return 0;
}

QString Notifier::backendName() const
{
    if (m_dbusAvailable)
        return tr("Desktop notifications (with buttons)");
    return m_tray ? tr("Tray notifications") : tr("Unavailable");
}

void Notifier::onDBusActionInvoked(uint id, const QString &actionKey)
{
    Q_EMIT actionInvoked(id, actionKey);
}
