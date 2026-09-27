#pragma once

#include <QPoint>
#include <QWidget>

class FocusTimer;
class QLabel;
class QPushButton;
class QToolButton;
class TimeDisplay;

// Floating focus timer: a normal resizable window with controls, or a
// frameless mini view that shows only the remaining time. Either can stay
// on top of other windows. Double-click switches views; right-click opens
// the full menu. Closing it only hides it; the timer keeps running.
class FocusTimerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit FocusTimerWindow(FocusTimer *timer, QWidget *parent = nullptr);

    // Shows the window in its saved view and position, and raises it.
    void present();
    // Saves geometry and view. Called on hide and when the app quits.
    void saveState() const;

    bool isMini() const { return m_mini; }
    void setMini(bool mini);
    bool isAlwaysOnTop() const { return m_alwaysOnTop; }
    void setAlwaysOnTop(bool onTop);

Q_SIGNALS:
    void openAppRequested();

protected:
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Edges edgesAt(const QPoint &pos) const;
    void applyWindowFlags();
    void restoreGeometryFor(bool mini);
    void refreshState();
    void refreshTime();

    FocusTimer *m_timer;
    QWidget *m_normalChrome = nullptr;
    QLabel *m_caption = nullptr;
    TimeDisplay *m_display = nullptr;
    QPushButton *m_startPause = nullptr;
    QPushButton *m_stop = nullptr;
    QToolButton *m_pin = nullptr;
    QToolButton *m_miniButton = nullptr;
    bool m_mini = false;
    bool m_alwaysOnTop = true;
    QPoint m_pressPos;
    bool m_dragPending = false;
};
