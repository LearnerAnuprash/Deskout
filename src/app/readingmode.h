#pragma once

#include "platform/grayscale/grayscalebackend.h"

#include <QObject>
#include <QTimer>

#include <memory>

// Reading Mode: whole-screen grayscale. Remembers the user's choice across
// restarts and re-applies it at startup. The filter itself is removed when
// Deskout quits (the saved choice stays), so the screen never stays gray
// without Deskout running to switch it back.
class ReadingMode : public QObject
{
    Q_OBJECT

public:
    using Availability = GrayscaleBackend::Availability;

    explicit ReadingMode(std::unique_ptr<GrayscaleBackend> backend = createGrayscaleBackend(),
                         QObject *parent = nullptr);
    ~ReadingMode() override;

    // Re-applies the saved choice. If the platform isn't ready yet (e.g. GNOME
    // Shell still loading the extension after login), keeps retrying briefly.
    void start();

    // The user's choice (persisted).
    bool isEnabled() const { return m_enabled; }
    // Whether the filter is applied right now.
    bool isActive() const { return m_active; }
    // Enabled but waiting on the platform (e.g. log out/in needed).
    bool isPending() const { return m_enabled && !m_active; }

    GrayscaleBackend::Status status() const;
    QString backendName() const;
    bool install(QString *error);

    // Saves the choice and applies it. Returns false with a user-facing
    // message when the filter couldn't be switched on now; if the platform
    // only needs a re-login, the choice is kept and applied later.
    bool setEnabled(bool on, QString *message = nullptr);

    // Removes the filter without changing the saved choice (app quit).
    void shutdown();

    // Retry schedule, exposed for tests.
    void setRetryPolicy(int intervalMs, int attempts);

Q_SIGNALS:
    void changed();

private:
    void save() const;
    void startRetry();
    void retry();

    std::unique_ptr<GrayscaleBackend> m_backend;
    bool m_enabled = false;
    bool m_active = false;
    QTimer m_retry;
    int m_retryAttempts = 30;
    int m_retriesLeft = 0;
};
