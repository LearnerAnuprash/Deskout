#include "ui/appicon.h"

#include <QDir>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QSvgRenderer>

namespace {

QIcon renderIcon(const QString &resource)
{
    QSvgRenderer renderer(resource);
    QIcon icon;
    if (!renderer.isValid())
        return icon;
    for (int size : {16, 22, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

namespace AppIcon {

QIcon icon(bool paused)
{
    static const QIcon normal = renderIcon(QStringLiteral(":/icons/deskout.svg"));
    static const QIcon pausedIcon = renderIcon(QStringLiteral(":/icons/deskout-paused.svg"));
    return paused ? pausedIcon : normal;
}

QString exportPng()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty() || !QDir().mkpath(dir))
        return QString();
    const QString path = dir + QStringLiteral("/deskout.png");
    const QPixmap pixmap = icon(false).pixmap(256, 256);
    if (pixmap.isNull() || !pixmap.save(path, "PNG"))
        return QString();
    return path;
}

} // namespace AppIcon
