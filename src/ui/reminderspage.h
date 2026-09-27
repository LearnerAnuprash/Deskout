#pragma once

#include <QList>
#include <QTimer>
#include <QWidget>

class QCheckBox;
class QLabel;
class ReminderEngine;

// Main-window page: one card per reminder with live countdown, on/off
// switch, "Test now" and a shortcut to its settings.
class RemindersPage : public QWidget
{
    Q_OBJECT

public:
    explicit RemindersPage(ReminderEngine *engine, QWidget *parent = nullptr);

Q_SIGNALS:
    void editRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Card
    {
        QString id;
        QCheckBox *enabled = nullptr;
        QLabel *summary = nullptr;
        QLabel *status = nullptr;
    };

    QWidget *buildCard(const QString &id);
    void refreshConfig(const QString &id);
    void refreshStatus();

    ReminderEngine *m_engine;
    QList<Card> m_cards;
    QTimer m_refresh;
};
