#pragma once

#include <QLockFile>
#include <QObject>

#include <memory>

class QLocalServer;

// Ensures one Deskout process per user. The first process becomes the
// primary instance and listens on a local socket; later launches (including
// `deskout --toggle-pause` fired by a desktop keyboard shortcut) forward a
// one-line command to it and exit.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    // Returns true if this process is now the primary instance.
    bool tryBecomePrimary();

    // Sends `command` to the running primary instance. Returns false when no
    // instance is reachable.
    static bool sendToPrimary(const QString &command, int timeoutMs = 1500);

Q_SIGNALS:
    void commandReceived(const QString &command);

private:
    static QString serverName();
    void onNewConnection();

    std::unique_ptr<QLockFile> m_lock;
    QLocalServer *m_server = nullptr;
};
