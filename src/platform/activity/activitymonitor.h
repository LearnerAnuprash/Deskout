#pragma once

#include <QObject>
#include <QTimer>

#include <memory>

class ActivityBackend;

// Polls the platform for idle time and answers "is another app
// full-screen?". Settings (thresholds, on/off) come from QSettings.
class ActivityMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ActivityMonitor(QObject *parent = nullptr);
    ~ActivityMonitor() override;

    // Re-read detection settings.
    void reload();

    bool isIdle() const { return m_idle; }
    qint64 lastIdleMs() const { return m_lastIdleMs; }
    bool idleSupported() const { return m_idleSupported; }
    bool idleDetectionEnabled() const { return m_idleEnabled; }
    int idleThresholdMinutes() const { return m_thresholdMinutes; }
    QString idleBackendName() const;

    bool fullscreenDetectionEnabled() const { return m_fullscreenEnabled; }
    // Checks now; returns false when detection is disabled.
    bool shouldAvoidFullscreen();
    QString fullscreenBackendName() const;

Q_SIGNALS:
    void idleChanged(bool idle);

private:
    void poll();

    std::unique_ptr<ActivityBackend> m_backend;
    QTimer m_poll;
    bool m_idle = false;
    bool m_idleSupported = true;
    qint64 m_lastIdleMs = 0;
    bool m_idleEnabled = true;
    int m_thresholdMinutes = 5;
    bool m_fullscreenEnabled = true;
};
