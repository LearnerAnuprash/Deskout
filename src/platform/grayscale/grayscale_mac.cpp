#include "platform/grayscale/grayscalebackend.h"

#include <QCoreApplication>

#include <dlfcn.h>

namespace {

// macOS has no public API for a system-wide colour filter. Use the private
// Universal Access switch behind System Settings > Accessibility > Display >
// Color Filters, falling back to the older CoreGraphics call.
using SetGrayscaleFn = void (*)(bool);

class MacGrayscaleBackend final : public GrayscaleBackend
{
public:
    MacGrayscaleBackend()
    {
        m_universalAccess = dlopen(
            "/System/Library/PrivateFrameworks/UniversalAccess.framework/UniversalAccess", RTLD_LAZY);
        if (m_universalAccess)
            m_set = reinterpret_cast<SetGrayscaleFn>(dlsym(m_universalAccess, "UAGrayscaleSetEnabled"));
        if (!m_set)
            m_set = reinterpret_cast<SetGrayscaleFn>(dlsym(RTLD_DEFAULT, "CGDisplayForceToGray"));
    }

    ~MacGrayscaleBackend() override
    {
        if (m_universalAccess)
            dlclose(m_universalAccess);
    }

    QString name() const override
    {
        return QCoreApplication::translate("ReadingMode", "macOS grayscale colour filter");
    }

    Status status() override
    {
        if (m_set)
            return {Availability::Ready, QString()};
        return {Availability::Unsupported,
                QCoreApplication::translate("ReadingMode",
                                            "This macOS version doesn't expose a grayscale switch. Use "
                                            "System Settings > Accessibility > Display > Color Filters.")};
    }

    bool apply(bool grayscale, QString *error) override
    {
        if (!m_set) {
            if (error)
                *error = status().message;
            return false;
        }
        m_set(grayscale);
        return true;
    }

private:
    void *m_universalAccess = nullptr;
    SetGrayscaleFn m_set = nullptr;
};

} // namespace

std::unique_ptr<GrayscaleBackend> createGrayscaleBackend()
{
    return std::make_unique<MacGrayscaleBackend>();
}
