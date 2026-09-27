#include "ui/notespage.h"

#include "core/database.h"
#include "core/notesstore.h"
#include "ui/timeformat.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

constexpr int SaveDelayMs = 600;
constexpr int DialogTitleLength = 60;

enum Role {
    IdRole = Qt::UserRole,
    TitleRole,
    PreviewRole,
    UpdatedRole,
};

enum DetailPage {
    EmptyPage,
    EditorPage,
};

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

// Two-line row: bold title with the date on the right, preview below.
class NoteItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        const QFontMetrics metrics(option.font);
        return {option.rect.width(), metrics.height() * 2 + LineGap + Padding * 2};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        const QPalette &palette = option.palette;
        const QRect r = option.rect;
        if (option.state & QStyle::State_Selected) {
            QColor tint = palette.color(QPalette::Highlight);
            tint.setAlpha(38);
            painter->fillRect(r, tint);
            painter->fillRect(QRect(r.left(), r.top(), 3, r.height()), palette.color(QPalette::Highlight));
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(r, palette.color(QPalette::AlternateBase));
        }
        painter->setPen(palette.color(QPalette::Mid));
        painter->drawLine(r.left() + 12, r.bottom(), r.right() - 8, r.bottom());

        const QRect content = r.adjusted(14, Padding, -10, -Padding);
        QFont titleFont = option.font;
        titleFont.setWeight(QFont::DemiBold);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics metrics(option.font);
        const QColor muted = palette.color(QPalette::PlaceholderText);

        const QString date = TimeFormat::compact(index.data(UpdatedRole).toDateTime());
        const int dateWidth = metrics.horizontalAdvance(date);
        painter->setFont(option.font);
        painter->setPen(muted);
        painter->drawText(QRect(content.right() - dateWidth, content.top(), dateWidth, titleMetrics.height()),
                          Qt::AlignRight | Qt::AlignVCenter, date);

        const QRect titleRect(content.left(), content.top(), content.width() - dateWidth - 10,
                              titleMetrics.height());
        painter->setFont(titleFont);
        painter->setPen(palette.color(QPalette::Text));
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          titleMetrics.elidedText(index.data(TitleRole).toString(), Qt::ElideRight,
                                                  titleRect.width()));

        QString preview = index.data(PreviewRole).toString();
        if (preview.isEmpty())
            preview = QCoreApplication::translate("NotesPage", "No additional text");
        const QRect previewRect(content.left(), titleRect.bottom() + 1 + LineGap, content.width(), metrics.height());
        painter->setFont(option.font);
        painter->setPen(muted);
        painter->drawText(previewRect, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(preview, Qt::ElideRight, previewRect.width()));
        painter->restore();
    }

private:
    static constexpr int Padding = 9;
    static constexpr int LineGap = 3;
};

} // namespace

NotesPage::NotesPage(NotesStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    titles->addWidget(makeLabel(tr("Notes"), "PageTitle"));
    titles->addWidget(makeLabel(tr("Quick memos. Changes are saved as you type."), "Muted"));
    header->addLayout(titles, 1);
    m_new = new QPushButton(tr("New note"));
    m_new->setObjectName(QStringLiteral("PrimaryButton"));
    m_new->setToolTip(tr("New note (%1)").arg(QKeySequence(QKeySequence::New).toString(QKeySequence::NativeText)));
    connect(m_new, &QPushButton::clicked, this, &NotesPage::newNote);
    header->addWidget(m_new, 0, Qt::AlignBottom);
    layout->addLayout(header);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(1, 1, 1, 1);
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName(QStringLiteral("NotesSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);
    splitter->addWidget(buildListPanel());
    splitter->addWidget(buildEditor());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({290, 520});
    cardLayout->addWidget(splitter);
    layout->addWidget(card, 1);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &NotesPage::save);
    // Rebuild the list after the current event, never in the middle of a
    // list signal (deleting the item that is being reported).
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(0);
    connect(&m_reloadTimer, &QTimer::timeout, this, &NotesPage::reloadList);
    connect(m_store, &NotesStore::noteSaved, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(m_store, &NotesStore::noteRemoved, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(qApp, &QCoreApplication::aboutToQuit, this, &NotesPage::save);

    auto *newShortcut = new QShortcut(QKeySequence::New, this);
    newShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newShortcut, &QShortcut::activated, this, &NotesPage::newNote);
    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        m_filter->setFocus();
        m_filter->selectAll();
    });
    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, m_list);
    deleteShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &NotesPage::deleteCurrent);

    showEditor(false);
    reloadList();
}

QWidget *NotesPage::buildListPanel()
{
    auto *panel = new QWidget;
    panel->setMinimumWidth(220);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 6, 8);
    layout->setSpacing(8);

    m_filter = new QLineEdit;
    m_filter->setObjectName(QStringLiteral("NotesFilter"));
    m_filter->setPlaceholderText(tr("Filter notes"));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &QLineEdit::textChanged, this, &NotesPage::reloadList);
    layout->addWidget(m_filter);

    m_list = new QListWidget;
    m_list->setObjectName(QStringLiteral("NotesList"));
    m_list->setItemDelegate(new NoteItemDelegate(m_list));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMouseTracking(true);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    connect(m_list, &QListWidget::currentItemChanged, this, &NotesPage::onCurrentItemChanged);
    layout->addWidget(m_list, 1);

    m_count = makeLabel(QString(), "Muted");
    layout->addWidget(m_count);
    return panel;
}

QWidget *NotesPage::buildEditor()
{
    m_detail = new QStackedWidget;

    auto *empty = new QWidget;
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(32, 24, 32, 24);
    emptyLayout->addStretch(1);
    m_emptyText = makeLabel(QString(), "Muted");
    m_emptyText->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyText);
    emptyLayout->addStretch(2);
    m_detail->insertWidget(EmptyPage, empty);

    auto *editor = new QWidget;
    auto *layout = new QVBoxLayout(editor);
    layout->setContentsMargins(18, 12, 14, 12);
    layout->setSpacing(6);
    auto *top = new QHBoxLayout;
    m_meta = makeLabel(QString(), "Muted");
    top->addWidget(m_meta, 1);
    m_delete = new QPushButton(tr("Delete"));
    m_delete->setToolTip(tr("Delete this note"));
    connect(m_delete, &QPushButton::clicked, this, &NotesPage::deleteCurrent);
    top->addWidget(m_delete);
    layout->addLayout(top);

    m_title = new QLineEdit;
    m_title->setObjectName(QStringLiteral("NoteTitle"));
    m_title->setPlaceholderText(tr("Title"));
    m_title->setMaxLength(200);
    connect(m_title, &QLineEdit::textEdited, this, &NotesPage::onEdited);
    connect(m_title, &QLineEdit::returnPressed, this, [this] { m_body->setFocus(); });
    layout->addWidget(m_title);

    m_body = new QPlainTextEdit;
    m_body->setObjectName(QStringLiteral("NoteBody"));
    m_body->setPlaceholderText(tr("Write your note…"));
    m_body->setFrameShape(QFrame::NoFrame);
    connect(m_body, &QPlainTextEdit::textChanged, this, &NotesPage::onEdited);
    layout->addWidget(m_body, 1);
    m_detail->insertWidget(EditorPage, editor);
    return m_detail;
}

void NotesPage::openNote(qint64 id)
{
    if (m_editing && id == m_currentId && id != 0) {
        selectInList(id);
        return;
    }
    leaveCurrent();
    const std::optional<Note> note = m_store->get(id);
    if (!note) {
        m_currentId = 0;
        showEditor(false);
        m_reloadTimer.start();
        return;
    }
    m_currentId = id;
    {
        const QSignalBlocker titleBlocker(m_title);
        const QSignalBlocker bodyBlocker(m_body);
        m_title->setText(note->title);
        m_body->setPlainText(note->body);
    }
    m_dirty = false;
    showEditor(true);
    refreshMeta();
    selectInList(id);
}

void NotesPage::newNote()
{
    if (!Database::isOpen())
        return;
    leaveCurrent();
    // Otherwise the new note may not show up in the list once saved.
    m_filter->clear();
    m_currentId = 0;
    {
        const QSignalBlocker titleBlocker(m_title);
        const QSignalBlocker bodyBlocker(m_body);
        m_title->clear();
        m_body->clear();
    }
    m_dirty = false;
    showEditor(true);
    selectInList(0);
    refreshMeta();
    m_title->setFocus();
}

void NotesPage::save()
{
    m_saveTimer.stop();
    if (!m_dirty || !m_editing)
        return;
    const QString title = m_title->text();
    const QString body = m_body->toPlainText();
    if (m_currentId == 0) {
        // Nothing typed yet: nothing to keep.
        if (title.trimmed().isEmpty() && body.trimmed().isEmpty()) {
            m_dirty = false;
            return;
        }
        m_currentId = m_store->create(title, body);
        if (m_currentId == 0) {
            m_meta->setText(tr("Couldn't save this note."));
            return; // still dirty: the next edit retries
        }
    } else if (!m_store->update(m_currentId, title, body)) {
        m_meta->setText(tr("Couldn't save this note."));
        return;
    }
    m_dirty = false;
    refreshMeta();
}

void NotesPage::hideEvent(QHideEvent *event)
{
    save();
    QWidget::hideEvent(event);
}

void NotesPage::reloadList()
{
    m_reloadTimer.stop();
    const int scroll = m_list->verticalScrollBar()->value();
    const QList<NoteSummary> notes = m_store->list(m_filter->text());
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const NoteSummary &note : notes) {
            auto *item = new QListWidgetItem;
            item->setData(IdRole, note.id);
            item->setData(TitleRole, NotesStore::displayTitle(note.title, note.preview));
            item->setData(PreviewRole, NotesStore::displayPreview(note.title, note.preview));
            item->setData(UpdatedRole, note.updatedAt);
            m_list->addItem(item);
        }
    }
    m_list->verticalScrollBar()->setValue(scroll);
    if (m_editing && m_currentId != 0)
        selectInList(m_currentId);

    const int total = m_store->count();
    const bool filtering = !m_filter->text().trimmed().isEmpty();
    if (!Database::isOpen())
        m_count->setText(QString());
    else if (filtering && notes.isEmpty())
        m_count->setText(tr("No notes match “%1”.").arg(m_filter->text().trimmed()));
    else if (filtering)
        m_count->setText(tr("%1 of %2 notes").arg(notes.size()).arg(total));
    else
        m_count->setText(total == 1 ? tr("1 note") : tr("%1 notes").arg(total));

    m_new->setEnabled(Database::isOpen());
    if (!Database::isOpen())
        m_emptyText->setText(tr("Notes can't be loaded: %1").arg(Database::openError()));
    else if (total == 0)
        m_emptyText->setText(tr("No notes yet.\nClick New note to write your first one."));
    else
        m_emptyText->setText(tr("Select a note, or press %1 for a new one.")
                                 .arg(QKeySequence(QKeySequence::New).toString(QKeySequence::NativeText)));
}

void NotesPage::onCurrentItemChanged(QListWidgetItem *current)
{
    if (current)
        openNote(current->data(IdRole).toLongLong());
}

void NotesPage::onEdited()
{
    m_dirty = true;
    m_meta->setText(tr("Editing…"));
    m_saveTimer.start();
}

void NotesPage::leaveCurrent()
{
    save();
    // A note emptied completely isn't worth keeping.
    if (m_editing && m_currentId != 0 && m_title->text().trimmed().isEmpty()
        && m_body->toPlainText().trimmed().isEmpty())
        m_store->remove(m_currentId);
}

void NotesPage::deleteCurrent()
{
    if (!m_editing)
        return;
    const bool hasContent = !m_title->text().trimmed().isEmpty() || !m_body->toPlainText().trimmed().isEmpty();
    if (hasContent) {
        QString title = NotesStore::displayTitle(m_title->text(), m_body->toPlainText());
        if (title.size() > DialogTitleLength)
            title = title.left(DialogTitleLength - 1) + QChar(0x2026);
        QMessageBox box(QMessageBox::Question, tr("Delete note"),
                        tr("Delete “%1”? This can't be undone.").arg(title), QMessageBox::Cancel, this);
        QPushButton *remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != remove)
            return;
    }

    // Show the neighbour next, like a mail client.
    qint64 next = 0;
    const int row = m_list->currentRow();
    if (row >= 0) {
        if (QListWidgetItem *below = m_list->item(row + 1))
            next = below->data(IdRole).toLongLong();
        else if (QListWidgetItem *above = m_list->item(row - 1))
            next = above->data(IdRole).toLongLong();
    }

    m_saveTimer.stop();
    m_dirty = false;
    if (m_currentId != 0)
        m_store->remove(m_currentId);
    m_currentId = 0;
    showEditor(false);
    if (next != 0)
        openNote(next);
}

void NotesPage::showEditor(bool show)
{
    m_editing = show;
    m_detail->setCurrentIndex(show ? EditorPage : EmptyPage);
}

void NotesPage::refreshMeta()
{
    if (m_currentId == 0) {
        m_meta->setText(tr("New note"));
        return;
    }
    const std::optional<Note> note = m_store->get(m_currentId);
    m_meta->setText(note ? tr("Edited %1").arg(TimeFormat::dateTime(note->updatedAt)) : QString());
}

void NotesPage::selectInList(qint64 id)
{
    const QSignalBlocker blocker(m_list);
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        if (id != 0 && item->data(IdRole).toLongLong() == id) {
            m_list->setCurrentItem(item);
            m_list->scrollToItem(item);
            return;
        }
    }
    m_list->setCurrentItem(nullptr);
    m_list->clearSelection();
}
