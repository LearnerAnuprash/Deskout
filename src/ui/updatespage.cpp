#include "ui/updatespage.h"

#include "core/database.h"
#include "core/dailyupdates.h"
#include "ui/timeformat.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int SaveDelayMs = 800;
constexpr int DateCheckMs = 30 * 1000;
constexpr int EditorHeight = 110;

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

QString longDate(const QDate &day)
{
    return QLocale().toString(day, QStringLiteral("dddd, d MMMM yyyy"));
}

// "Yesterday", "3 days ago" (within a week) or nothing, next to the date.
QString relativeDay(const QDate &day, const QDate &today)
{
    const qint64 ago = day.daysTo(today);
    if (ago == 1)
        return QCoreApplication::translate("UpdatesPage", "Yesterday");
    if (ago > 1 && ago < 7)
        return QCoreApplication::translate("UpdatesPage", "%1 days ago").arg(ago);
    return QString();
}

} // namespace

UpdatesPage::UpdatesPage(DailyUpdatesStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_day(QDate::currentDate())
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // The whole page scrolls: today on top, history below.
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("PageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("PageScrollContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(tr("Daily Updates"), "PageTitle"));
    layout->addWidget(makeLabel(tr("Jot down what you did and what's next. Tomorrow, Deskout shows it to "
                                   "you before anything else."),
                                "Muted"));
    layout->addWidget(buildTodayCard());

    layout->addSpacing(6);
    layout->addWidget(makeLabel(tr("Earlier days"), "SectionTitle"));
    m_historyEmpty = makeLabel(tr("Nothing yet. Your past updates will appear here, newest first."), "Muted");
    layout->addWidget(m_historyEmpty);
    m_historyLayout = new QVBoxLayout;
    m_historyLayout->setSpacing(10);
    layout->addLayout(m_historyLayout);
    m_older = new QPushButton(tr("Show older updates"));
    connect(m_older, &QPushButton::clicked, this, &UpdatesPage::loadOlder);
    layout->addWidget(m_older, 0, Qt::AlignHCenter);
    layout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &UpdatesPage::save);
    m_dateCheck.setInterval(DateCheckMs);
    connect(&m_dateCheck, &QTimer::timeout, this, &UpdatesPage::checkDate);
    m_dateCheck.start();
    connect(m_store, &DailyUpdatesStore::changed, this, &UpdatesPage::onStoreChanged);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &UpdatesPage::save);

    loadToday();
    reloadHistory();
}

QWidget *UpdatesPage::buildTodayCard()
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    auto *header = new QHBoxLayout;
    m_todayTitle = makeLabel(QString(), "CardTitle");
    header->addWidget(m_todayTitle, 1);
    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("Muted"));
    header->addWidget(m_status);
    layout->addLayout(header);

    layout->addWidget(makeLabel(tr("What I did today"), "FieldLabel"));
    m_done = new QPlainTextEdit;
    m_done->setObjectName(QStringLiteral("UpdateDone"));
    m_done->setPlaceholderText(tr("Shipped the focus timer, reviewed two PRs, read about attention…"));
    m_done->setTabChangesFocus(true);
    m_done->setFixedHeight(EditorHeight);
    connect(m_done, &QPlainTextEdit::textChanged, this, &UpdatesPage::onEdited);
    layout->addWidget(m_done);

    layout->addSpacing(4);
    layout->addWidget(makeLabel(tr("Todos for tomorrow"), "FieldLabel"));
    m_todo = new QPlainTextEdit;
    m_todo->setObjectName(QStringLiteral("UpdateTodo"));
    m_todo->setPlaceholderText(tr("- Finish the notes page\n- Reply to design feedback"));
    m_todo->setTabChangesFocus(true);
    m_todo->setFixedHeight(EditorHeight);
    connect(m_todo, &QPlainTextEdit::textChanged, this, &UpdatesPage::onEdited);
    layout->addWidget(m_todo);

    m_recap = new QCheckBox(tr("Show my last update first thing each new day"));
    m_recap->setChecked(DailyUpdatesStore::recapEnabled());
    connect(m_recap, &QCheckBox::toggled, this, [](bool on) { DailyUpdatesStore::setRecapEnabled(on); });
    layout->addSpacing(4);
    layout->addWidget(m_recap);
    return card;
}

QWidget *UpdatesPage::buildEntryCard(const DailyUpdate &update) const
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 12, 18, 14);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout;
    header->addWidget(makeLabel(longDate(update.day), "CardTitle"), 1);
    const QString relative = relativeDay(update.day, QDate::currentDate());
    if (!relative.isEmpty())
        header->addWidget(makeLabel(relative, "Muted"));
    layout->addLayout(header);

    const auto addSection = [layout](const QString &title, const QString &text) {
        if (text.trimmed().isEmpty())
            return;
        layout->addSpacing(4);
        layout->addWidget(makeLabel(title, "FieldLabel"));
        QLabel *body = makeLabel(text.trimmed());
        body->setTextFormat(Qt::PlainText);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(body);
    };
    addSection(tr("Did"), update.done);
    addSection(tr("Todos"), update.todo);
    return card;
}

void UpdatesPage::save()
{
    m_saveTimer.stop();
    if (!m_dirty)
        return;
    m_saving = true;
    const bool saved = m_store->save(m_day, m_done->toPlainText(), m_todo->toPlainText());
    m_saving = false;
    if (!saved) {
        m_status->setText(Database::isOpen() ? tr("Couldn't save") : tr("Can't save: database unavailable"));
        return; // still dirty: the next edit retries
    }
    m_dirty = false;
    refreshStatus();
}

void UpdatesPage::focusToday()
{
    checkDate();
    m_done->setFocus();
    m_done->moveCursor(QTextCursor::End);
}

void UpdatesPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    checkDate();
    const QSignalBlocker blocker(m_recap);
    m_recap->setChecked(DailyUpdatesStore::recapEnabled());
}

void UpdatesPage::hideEvent(QHideEvent *event)
{
    save();
    QWidget::hideEvent(event);
}

void UpdatesPage::checkDate()
{
    const QDate today = QDate::currentDate();
    if (today == m_day)
        return;
    // Past midnight: what was typed belongs to the day it was typed for.
    save();
    m_day = today;
    loadToday();
    reloadHistory();
}

void UpdatesPage::loadToday()
{
    m_todayTitle->setText(tr("Today · %1").arg(longDate(m_day)));
    const std::optional<DailyUpdate> today = m_store->get(m_day);
    m_loading = true;
    m_done->setPlainText(today ? today->done : QString());
    m_todo->setPlainText(today ? today->todo : QString());
    m_loading = false;
    m_dirty = false;
    refreshStatus();
}

void UpdatesPage::reloadHistory()
{
    while (QLayoutItem *item = m_historyLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_oldestShown = m_day;
    loadOlder();
}

void UpdatesPage::loadOlder()
{
    const QList<DailyUpdate> page = m_store->history(DailyUpdatesStore::HistoryPageSize, m_oldestShown);
    for (const DailyUpdate &update : page)
        m_historyLayout->addWidget(buildEntryCard(update));
    if (!page.isEmpty())
        m_oldestShown = page.constLast().day;
    m_historyEmpty->setVisible(m_historyLayout->count() == 0);
    // A full page means there may be more.
    m_older->setVisible(page.size() == DailyUpdatesStore::HistoryPageSize);
}

void UpdatesPage::onEdited()
{
    if (m_loading)
        return;
    m_dirty = true;
    m_status->setText(tr("Editing…"));
    m_saveTimer.start();
}

void UpdatesPage::onStoreChanged(const QDate &day)
{
    // Today's own saves don't affect the history below.
    if (m_saving || day == m_day)
        return;
    reloadHistory();
}

void UpdatesPage::refreshStatus()
{
    if (!Database::isOpen()) {
        m_status->setText(tr("Database unavailable"));
        return;
    }
    const std::optional<DailyUpdate> today = m_store->get(m_day);
    m_status->setText(today ? tr("Saved %1").arg(TimeFormat::dateTime(today->updatedAt)) : QString());
}
