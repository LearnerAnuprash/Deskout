#pragma once

#include <QList>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QSpinBox;
class QTimeEdit;
class QToolButton;
class ReminderEngine;

// Settings tab: interval, active days, active hours and alert style for
// every reminder type.
class ReminderSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReminderSettingsPage(ReminderEngine *engine, QWidget *parent = nullptr);

    void load();
    void apply();

private:
    struct Editor
    {
        QString id;
        QGroupBox *box = nullptr;
        QSpinBox *interval = nullptr;
        QList<QToolButton *> days; // Monday first
        QTimeEdit *start = nullptr;
        QTimeEdit *end = nullptr;
        QComboBox *style = nullptr;
    };

    QGroupBox *buildEditor(const QString &id);

    ReminderEngine *m_engine;
    QList<Editor> m_editors;
};
