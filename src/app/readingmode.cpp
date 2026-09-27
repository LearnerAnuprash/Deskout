#include "app/readingmode.h"

#include "core/settingskeys.h"

#include <QSettings>

ReadingMode::ReadingMode(std::unique_ptr<GrayscaleBackend> backend, QObject *parent)
    : QObject(parent)
    , m_backend(std::move(backend))
{
    m_enabled = QSettings().value(SettingsKeys::ReadingModeEnabled, false).toBool();
    m_retry.setInterval(2000);
    connect(&m_retry, &QTimer::timeout, this, &ReadingMode::retry);
}

ReadingMode::~ReadingMode()
{
    shutdown();
}

void ReadingMode::start()
{
    if (m_enabled)
        setEnabled(true);
}

GrayscaleBackend::Status ReadingMode::status() const
{
    return m_backend->status();
}

QString ReadingMode::backendName() const
{
    return m_backend->name();
}

bool ReadingMode::install(QString *error)
{
    return m_backend->install(error);
}

bool ReadingMode::setEnabled(bool on, QString *message)
{
    const auto finish = [this](bool result) {
        save();
        Q_EMIT changed();
        return result;
    };

    m_retry.stop();
    if (!on) {
        m_enabled = false;
        if (m_active)
            m_backend->apply(false, nullptr);
        m_active = false;
        return finish(true);
    }

    m_enabled = true;
    const GrayscaleBackend::Status st = m_backend->status();
    switch (st.availability) {
    case Availability::Ready:
        m_active = m_backend->apply(true, message);
        if (!m_active)
            m_enabled = false;
        return finish(m_active);
    case Availability::NeedsRelogin:
        // Keep the choice; it takes effect once the platform catches up.
        if (message)
            *message = st.message;
        startRetry();
        return finish(false);
    case Availability::NeedsInstall:
    case Availability::Unsupported:
        break;
    }
    m_enabled = false;
    if (message)
        *message = st.message;
    return finish(false);
}

void ReadingMode::shutdown()
{
    m_retry.stop();
    if (m_active)
        m_backend->apply(false, nullptr);
    m_active = false;
}

void ReadingMode::setRetryPolicy(int intervalMs, int attempts)
{
    m_retry.setInterval(intervalMs);
    m_retryAttempts = attempts;
}

void ReadingMode::save() const
{
    QSettings().setValue(SettingsKeys::ReadingModeEnabled, m_enabled);
}

void ReadingMode::startRetry()
{
    m_retriesLeft = m_retryAttempts;
    m_retry.start();
}

void ReadingMode::retry()
{
    if (!m_enabled || m_active) {
        m_retry.stop();
        return;
    }
    if (m_backend->status().availability == Availability::Ready) {
        m_retry.stop();
        m_active = m_backend->apply(true, nullptr);
        Q_EMIT changed();
    } else if (--m_retriesLeft <= 0) {
        // Try again on the next app start.
        m_retry.stop();
    }
}
