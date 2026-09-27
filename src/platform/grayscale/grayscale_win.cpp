#include "platform/grayscale/grayscalebackend.h"

#include <QCoreApplication>

#include <windows.h>
#include <magnification.h>

namespace {

// Colour matrices are applied as [r g b a 1] x M. Grayscale uses the Rec. 601
// luma weights for every output channel.
const MAGCOLOREFFECT Grayscale = {{
    {0.299f, 0.299f, 0.299f, 0.0f, 0.0f},
    {0.587f, 0.587f, 0.587f, 0.0f, 0.0f},
    {0.114f, 0.114f, 0.114f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
}};

const MAGCOLOREFFECT Identity = {{
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
}};

class WinGrayscaleBackend final : public GrayscaleBackend
{
public:
    ~WinGrayscaleBackend() override
    {
        if (m_initialized) {
            MAGCOLOREFFECT identity = Identity;
            MagSetFullscreenColorEffect(&identity);
            MagUninitialize();
        }
    }

    QString name() const override
    {
        return QCoreApplication::translate("ReadingMode", "Windows Magnification colour effect");
    }

    Status status() override { return {Availability::Ready, QString()}; }

    bool apply(bool grayscale, QString *error) override
    {
        if (!m_initialized)
            m_initialized = MagInitialize();
        MAGCOLOREFFECT effect = grayscale ? Grayscale : Identity;
        if (m_initialized && MagSetFullscreenColorEffect(&effect))
            return true;
        if (error)
            *error = QCoreApplication::translate("ReadingMode", "Windows refused the colour effect (error %1).")
                         .arg(GetLastError());
        return false;
    }

private:
    bool m_initialized = false;
};

} // namespace

std::unique_ptr<GrayscaleBackend> createGrayscaleBackend()
{
    return std::make_unique<WinGrayscaleBackend>();
}
