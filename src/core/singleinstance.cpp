#include "core/singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

SingleInstance::~SingleInstance() = default;

QString SingleInstance::serverName()
{
    // Unique per user account so two users on one machine don't collide.
    const QByteArray hash = QCryptographicHash::hash(QDir::homePath().toUtf8(),
                                                     QCryptographicHash::Sha1);
    return QStringLiteral("deskout-%1").arg(QString::fromLatin1(hash.toHex().left(12)));
}

bool SingleInstance::tryBecomePrimary()
{
    // The lock file decides who is primary; QLockFile detects and removes
    // stale locks left behind by a crashed process.
    m_lock = std::make_unique<QLockFile>(QDir::temp().filePath(serverName() + QStringLiteral(".lock")));
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(100)) {
        m_lock.reset();
        return false;
    }

    // We own the lock, so any existing socket file is left over from a crash.
    QLocalServer::removeServer(serverName());
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(serverName())) {
        qWarning("Deskout: could not listen on local socket: %s",
                 qPrintable(m_server->errorString()));
        // Still primary: the lock is held, we just can't receive commands.
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

bool SingleInstance::sendToPrimary(const QString &command, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(command.toUtf8() + '\n');
    if (!socket.waitForBytesWritten(timeoutMs))
        return false;
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(timeoutMs);
    return true;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            while (socket->canReadLine()) {
                const QString command = QString::fromUtf8(socket->readLine()).trimmed();
                if (!command.isEmpty())
                    Q_EMIT commandReceived(command);
            }
        });
    }
}
