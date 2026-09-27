#pragma once

#include <QString>

#include <memory>

// Whole-screen grayscale filter provided by the OS or compositor. An overlay
// window can't desaturate what's beneath it, so each platform uses its own
// colour-filter mechanism:
//   GNOME:   a bundled GNOME Shell extension (grayscale_gnome.cpp)
//   Windows: Magnification API colour effect (grayscale_win.cpp)
//   macOS:   Universal Access grayscale (grayscale_mac.cpp)
class GrayscaleBackend
{
public:
    enum class Availability {
        Ready,        // apply() works now
        NeedsInstall, // install() first (GNOME extension missing/outdated)
        NeedsRelogin, // installed, active after the next login
        Unsupported,  // not possible on this desktop; see message
    };

    struct Status
    {
        Availability availability = Availability::Unsupported;
        QString message; // user-facing explanation when not Ready
    };

    virtual ~GrayscaleBackend() = default;

    virtual QString name() const = 0;
    virtual Status status() = 0;
    virtual bool install(QString *error)
    {
        if (error)
            *error = QStringLiteral("Nothing to install on this platform.");
        return false;
    }
    virtual bool apply(bool grayscale, QString *error) = 0;
};

std::unique_ptr<GrayscaleBackend> createGrayscaleBackend();
