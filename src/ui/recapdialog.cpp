#include "ui/recapdialog.h"

#include "core/dailyupdates.h"
#include "ui/appicon.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QTime>
#include <QVBoxLayout>

namespace {

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

} // namespace

RecapDialog::RecapDialog(const DailyUpdate &update, const QDate &today, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Deskout — your last update"));
    setWindowIcon(AppIcon::icon());
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->setSpacing(14);
    auto *icon = new QLabel;
    icon->setPixmap(AppIcon::icon().pixmap(48, 48));
    header->addWidget(icon, 0, Qt::AlignTop);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(makeLabel(greeting(QTime::currentTime()), "PageTitle"));
    titles->addWidget(makeLabel(subtitle(update.day, today), "Muted"));
    header->addLayout(titles, 1);
    layout->addLayout(header);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 14, 18, 16);
    cardLayout->setSpacing(4);
    const bool yesterday = update.day.daysTo(today) == 1;
    const auto addSection = [cardLayout](const QString &title, const QString &text) {
        if (text.trimmed().isEmpty())
            return;
        if (cardLayout->count() > 0)
            cardLayout->addSpacing(8);
        cardLayout->addWidget(makeLabel(title, "FieldLabel"));
        QLabel *body = makeLabel(text.trimmed(), "RecapText");
        body->setTextFormat(Qt::PlainText);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cardLayout->addWidget(body);
    };
    addSection(tr("What you did"), update.done);
    addSection(yesterday ? tr("Todos for today") : tr("Todos you planned"), update.todo);

    // Long entries scroll instead of growing the window off-screen.
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("PageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(card);
    scroll->setMaximumHeight(420);
    layout->addWidget(scroll, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *open = new QPushButton(tr("Write today's update"));
    connect(open, &QPushButton::clicked, this, [this] {
        Q_EMIT openUpdatesRequested();
        accept();
    });
    buttons->addWidget(open);
    auto *start = new QPushButton(tr("Start the day"));
    start->setObjectName(QStringLiteral("PrimaryButton"));
    start->setDefault(true);
    connect(start, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(start);
    layout->addLayout(buttons);
}

QString RecapDialog::greeting(const QTime &now)
{
    if (now.hour() < 12)
        return tr("Good morning");
    if (now.hour() < 17)
        return tr("Good afternoon");
    return tr("Good evening");
}

QString RecapDialog::subtitle(const QDate &day, const QDate &today)
{
    if (day.daysTo(today) == 1)
        return tr("Here's what you noted yesterday.");
    return tr("Here's what you noted on %1.").arg(QLocale().toString(day, QStringLiteral("dddd, d MMMM")));
}
