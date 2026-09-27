#pragma once

#include <QColor>
#include <QWidget>

// Big "12:34" readout that scales its font to whatever size it is given,
// with an optional thin progress bar along the bottom.
class TimeDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit TimeDisplay(QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const { return m_text; }
    // Negative hides the bar.
    void setProgress(double progress);
    // Invalid colour means the palette's text colour.
    void setColor(const QColor &color);
    // Fraction of the height the digits may use (the rest is margin).
    void setFillRatio(double ratio);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    double m_progress = -1.0;
    double m_fillRatio = 0.7;
    QColor m_color;
};
