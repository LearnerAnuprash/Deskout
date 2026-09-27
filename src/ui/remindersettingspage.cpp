#include "ui/remindersettingspage.h"

#include "core/reminderengine.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>

ReminderSettingsPage::ReminderSettingsPage(ReminderEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
{
    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    for (const QString &id : m_engine->ids())
        contentLayout->addWidget(buildEditor(id));
    auto *hint = new QLabel(tr("Tip: set the same start and end time to remind all day. An end "
                               "time before the start time runs past midnight."));
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("Muted"));
    contentLayout->addWidget(hint);
    contentLayout->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);
    load();
}

QGroupBox *ReminderSettingsPage::buildEditor(const QString &id)
{
    Editor editor;
    editor.id = id;
    // "&" would otherwise be read as a keyboard-mnemonic marker.
    editor.box = new QGroupBox(Reminders::texts(id).name.replace(QLatin1Char('&'), QLatin1String("&&")));
    editor.box->setCheckable(true);

    auto *form = new QFormLayout(editor.box);

    editor.interval = new QSpinBox;
    editor.interval->setRange(1, 24 * 60);
    editor.interval->setSuffix(tr(" min"));
    form->addRow(tr("Remind every"), editor.interval);

    auto *daysRow = new QHBoxLayout;
    daysRow->setSpacing(4);
    for (int day = Qt::Monday; day <= Qt::Sunday; ++day) {
        auto *button = new QToolButton;
        button->setObjectName(QStringLiteral("DayToggle"));
        button->setText(QLocale().dayName(day, QLocale::ShortFormat));
        button->setCheckable(true);
        button->setMinimumWidth(44);
        daysRow->addWidget(button);
        editor.days << button;
    }
    daysRow->addStretch(1);
    form->addRow(tr("On days"), daysRow);

    auto *hoursRow = new QHBoxLayout;
    editor.start = new QTimeEdit;
    editor.end = new QTimeEdit;
    editor.start->setDisplayFormat(QStringLiteral("HH:mm"));
    editor.end->setDisplayFormat(QStringLiteral("HH:mm"));
    hoursRow->addWidget(editor.start);
    hoursRow->addWidget(new QLabel(tr("to")));
    hoursRow->addWidget(editor.end);
    hoursRow->addStretch(1);
    form->addRow(tr("Active hours"), hoursRow);

    editor.style = new QComboBox;
    editor.style->addItem(tr("Full-screen alarm"), true);
    editor.style->addItem(tr("Notification only"), false);
    form->addRow(tr("Alert style"), editor.style);

    m_editors << editor;
    return editor.box;
}

void ReminderSettingsPage::load()
{
    for (const Editor &editor : std::as_const(m_editors)) {
        const ReminderConfig config = m_engine->config(editor.id);
        editor.box->setChecked(config.enabled);
        editor.interval->setValue(config.intervalMinutes);
        for (int i = 0; i < editor.days.size(); ++i)
            editor.days[i]->setChecked(config.isDayActive(Qt::Monday + i));
        editor.start->setTime(config.start);
        editor.end->setTime(config.end);
        editor.style->setCurrentIndex(editor.style->findData(config.fullScreen));
    }
}

void ReminderSettingsPage::apply()
{
    for (const Editor &editor : std::as_const(m_editors)) {
        ReminderConfig config = m_engine->config(editor.id);
        config.enabled = editor.box->isChecked();
        config.intervalMinutes = editor.interval->value();
        config.days = 0;
        for (int i = 0; i < editor.days.size(); ++i) {
            if (editor.days[i]->isChecked())
                config.days |= ReminderDays::bit(Qt::Monday + i);
        }
        config.start = editor.start->time();
        config.end = editor.end->time();
        config.fullScreen = editor.style->currentData().toBool();
        m_engine->setConfig(config);
    }
}
