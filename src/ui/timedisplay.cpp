#include "ui/timedisplay.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>

namespace {

constexpr qreal ReferencePixelSize = 100.0;
constexpr int ProgressHeight = 3;

// Width is measured on the text with every digit replaced by "0", so the
// font size doesn't jitter as the seconds change.
QString widthSample(QString text)
{
    for (QChar &c : text) {
        if (c.isDigit())
            c = QLatin1Char('0');
    }
    return text;
}

} // namespace

TimeDisplay::TimeDisplay(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TimeDisplay::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    update();
}

void TimeDisplay::setProgress(double progress)
{
    if (qFuzzyCompare(m_progress, progress))
        return;
    m_progress = progress;
    update();
}

void TimeDisplay::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void TimeDisplay::setFillRatio(double ratio)
{
    m_fillRatio = std::clamp(ratio, 0.1, 1.0);
    update();
}

QSize TimeDisplay::sizeHint() const
{
    return {240, 96};
}

QSize TimeDisplay::minimumSizeHint() const
{
    return {60, 24};
}

void TimeDisplay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QRectF area = rect();
    if (m_progress >= 0.0)
        area.setBottom(area.bottom() - ProgressHeight - 2);

    if (!m_text.isEmpty() && area.width() > 0 && area.height() > 0) {
        QFont font = this->font();
        font.setPixelSize(int(ReferencePixelSize));
        font.setWeight(QFont::Light);
        const QFontMetricsF metrics(font);
        const qreal textWidth = metrics.horizontalAdvance(widthSample(m_text));
        // Digits have no descenders; the cap height is what the eye sees.
        const qreal textHeight = metrics.capHeight() > 0 ? metrics.capHeight() : metrics.ascent();
        const qreal scale = std::min(area.width() * 0.92 / textWidth, area.height() * m_fillRatio / textHeight);
        font.setPixelSize(std::max(8, int(ReferencePixelSize * scale)));
        painter.setFont(font);
        painter.setPen(m_color.isValid() ? m_color : palette().color(QPalette::WindowText));
        // Centre the digits themselves; AlignCenter would centre the line
        // box, leaving the unused descender space below them.
        const QFontMetricsF scaled(font);
        const qreal visibleHeight = scaled.capHeight() > 0 ? scaled.capHeight() : scaled.ascent();
        const QPointF baseline(area.center().x() - scaled.horizontalAdvance(m_text) / 2,
                               area.center().y() + visibleHeight / 2);
        painter.drawText(baseline, m_text);
    }

    if (m_progress >= 0.0) {
        const QRectF track(0, height() - ProgressHeight, width(), ProgressHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Mid));
        painter.drawRoundedRect(track, 1.5, 1.5);
        QRectF filled = track;
        filled.setWidth(track.width() * std::clamp(m_progress, 0.0, 1.0));
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawRoundedRect(filled, 1.5, 1.5);
    }
}
