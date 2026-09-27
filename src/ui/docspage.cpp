#include "ui/docspage.h"

#include "core/database.h"
#include "core/docsstore.h"
#include "ui/doceditor.h"
#include "ui/timeformat.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int DialogTitleLength = 60;

enum Column {
    NameColumn,
    WordsColumn,
    EditedColumn,
};

enum Role {
    IdRole = Qt::UserRole,
    SortRole,
};

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

// Sorts on the raw value (name, word count, timestamp) rather than on the
// displayed text ("Yesterday 14:05").
class DocItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : NameColumn;
        const QVariant mine = data(column, SortRole);
        const QVariant theirs = other.data(column, SortRole);
        if (column == NameColumn)
            return QString::localeAwareCompare(mine.toString(), theirs.toString()) < 0;
        return mine.toLongLong() < theirs.toLongLong();
    }
};

QString elided(const QString &text)
{
    return text.size() > DialogTitleLength ? text.left(DialogTitleLength - 1) + QChar(0x2026) : text;
}

} // namespace

DocsPage::DocsPage(DocsStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget;
    m_stack->addWidget(buildList());
    m_editor = new DocEditor(m_store);
    connect(m_editor, &DocEditor::backRequested, this, &DocsPage::showList);
    m_stack->addWidget(m_editor);
    layout->addWidget(m_stack);

    // Rebuild after the current event, never in the middle of a signal
    // from the tree (e.g. while an inline rename is committing).
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(0);
    connect(&m_reloadTimer, &QTimer::timeout, this, &DocsPage::reloadList);
    connect(m_store, &DocsStore::docChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(m_store, &DocsStore::docRemoved, &m_reloadTimer, qOverload<>(&QTimer::start));

    auto *newShortcut = new QShortcut(QKeySequence::New, this);
    newShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newShortcut, &QShortcut::activated, this, &DocsPage::newDoc);

    reloadList();
}

QWidget *DocsPage::buildList()
{
    m_listView = new QWidget;
    auto *layout = new QVBoxLayout(m_listView);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    titles->addWidget(makeLabel(tr("Topic Docs"), "PageTitle"));
    titles->addWidget(makeLabel(tr("Longer write-ups, one document per topic. Right-click a document to "
                                   "rename or delete it."),
                                "Muted"));
    header->addLayout(titles, 1);
    m_new = new QPushButton(tr("New document"));
    m_new->setObjectName(QStringLiteral("PrimaryButton"));
    m_new->setToolTip(
        tr("New document (%1)").arg(QKeySequence(QKeySequence::New).toString(QKeySequence::NativeText)));
    connect(m_new, &QPushButton::clicked, this, &DocsPage::newDoc);
    header->addWidget(m_new, 0, Qt::AlignBottom);
    layout->addLayout(header);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 12, 12, 10);
    cardLayout->setSpacing(8);

    m_filter = new QLineEdit;
    m_filter->setObjectName(QStringLiteral("DocsFilter"));
    m_filter->setPlaceholderText(tr("Filter documents"));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &QLineEdit::textChanged, this, &DocsPage::reloadList);
    cardLayout->addWidget(m_filter);

    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("DocsList"));
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({tr("Name"), tr("Words"), tr("Last edited")});
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(EditedColumn, Qt::DescendingOrder);
    m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(WordsColumn, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(EditedColumn, QHeaderView::ResizeToContents);
    m_tree->header()->setStretchLastSection(false);
    m_tree->setMouseTracking(true);
    connect(m_tree, &QTreeWidget::itemClicked, this, &DocsPage::onItemClicked);
    connect(m_tree, &QTreeWidget::itemActivated, this, &DocsPage::onItemClicked);
    connect(m_tree, &QTreeWidget::itemChanged, this, &DocsPage::onItemChanged);
    connect(m_tree, &QWidget::customContextMenuRequested, this, &DocsPage::showContextMenu);
    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, m_tree);
    deleteShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &DocsPage::deleteSelected);
    cardLayout->addWidget(m_tree, 1);

    m_empty = makeLabel(QString(), "Muted");
    m_empty->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_empty, 1);

    m_count = makeLabel(QString(), "Muted");
    cardLayout->addWidget(m_count);
    layout->addWidget(card, 1);
    return m_listView;
}

void DocsPage::openDoc(qint64 id)
{
    if (isEditing() && m_editor->currentId() == id)
        return;
    if (m_editor->open(id))
        m_stack->setCurrentWidget(m_editor);
}

void DocsPage::newDoc()
{
    if (!Database::isOpen())
        return;
    const qint64 id = m_store->create();
    if (id == 0)
        return;
    m_filter->clear();
    openDoc(id);
    m_editor->focusTitle();
}

void DocsPage::showList()
{
    const qint64 last = m_editor->currentId();
    m_editor->save();
    m_stack->setCurrentWidget(m_listView);
    reloadList();
    // Keep the document just edited selected, for keyboard users.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        if (item->data(NameColumn, IdRole).toLongLong() == last) {
            m_tree->setCurrentItem(item);
            break;
        }
    }
    m_tree->setFocus();
}

bool DocsPage::isEditing() const
{
    return m_stack->currentWidget() == m_editor;
}

void DocsPage::reloadList()
{
    m_reloadTimer.stop();
    const qint64 selected = selectedId();
    const QList<DocSummary> docs = m_store->list(m_filter->text());
    {
        const QSignalBlocker blocker(m_tree);
        m_tree->setSortingEnabled(false);
        m_tree->clear();
        const QIcon icon = style()->standardIcon(QStyle::SP_FileIcon);
        for (const DocSummary &doc : docs) {
            auto *item = new DocItem(m_tree);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            item->setIcon(NameColumn, icon);
            item->setText(NameColumn, doc.title);
            item->setData(NameColumn, IdRole, doc.id);
            item->setData(NameColumn, SortRole, doc.title);
            item->setText(WordsColumn, QLocale().toString(doc.wordCount));
            item->setData(WordsColumn, SortRole, doc.wordCount);
            item->setTextAlignment(WordsColumn, Qt::AlignRight | Qt::AlignVCenter);
            item->setText(EditedColumn, TimeFormat::dateTime(doc.updatedAt));
            item->setData(EditedColumn, SortRole, doc.updatedAt.toMSecsSinceEpoch());
            item->setToolTip(NameColumn, tr("Created %1").arg(TimeFormat::dateTime(doc.createdAt)));
            if (doc.id == selected)
                m_tree->setCurrentItem(item);
        }
        m_tree->setSortingEnabled(true);
    }

    const int total = m_store->count();
    const bool filtering = !m_filter->text().trimmed().isEmpty();
    m_tree->setVisible(!docs.isEmpty());
    m_empty->setVisible(docs.isEmpty());
    m_filter->setVisible(total > 0 || filtering);
    m_new->setEnabled(Database::isOpen());
    if (!Database::isOpen())
        m_empty->setText(tr("Documents can't be loaded: %1").arg(Database::openError()));
    else if (total == 0)
        m_empty->setText(tr("No documents yet.\nClick New document to start one."));
    else
        m_empty->setText(tr("No documents match “%1”.").arg(m_filter->text().trimmed()));

    if (total == 0)
        m_count->clear();
    else if (filtering)
        m_count->setText(tr("%1 of %2 documents").arg(docs.size()).arg(total));
    else
        m_count->setText(total == 1 ? tr("1 document") : tr("%1 documents").arg(total));
}

void DocsPage::onItemClicked(QTreeWidgetItem *item)
{
    if (item)
        openDoc(item->data(NameColumn, IdRole).toLongLong());
}

void DocsPage::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != NameColumn)
        return;
    const qint64 id = item->data(NameColumn, IdRole).toLongLong();
    const QString title = item->text(NameColumn).simplified();
    if (title.isEmpty() || !m_store->rename(id, title))
        m_reloadTimer.start(); // put the old name back
}

void DocsPage::showContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item) {
        m_tree->setCurrentItem(item);
        const qint64 id = item->data(NameColumn, IdRole).toLongLong();
        menu.addAction(tr("Open"), this, [this, id] { openDoc(id); });
        menu.addAction(tr("Rename"), this, &DocsPage::renameSelected);
        menu.addSeparator();
        menu.addAction(tr("Delete…"), this, &DocsPage::deleteSelected);
    } else {
        menu.addAction(tr("New document"), this, &DocsPage::newDoc)->setEnabled(Database::isOpen());
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void DocsPage::renameSelected()
{
    if (QTreeWidgetItem *item = m_tree->currentItem())
        m_tree->editItem(item, NameColumn);
}

void DocsPage::deleteSelected()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return;
    const qint64 id = item->data(NameColumn, IdRole).toLongLong();
    QMessageBox box(QMessageBox::Question, tr("Delete document"),
                    tr("Delete “%1”? This can't be undone.").arg(elided(item->text(NameColumn))),
                    QMessageBox::Cancel, this);
    QPushButton *remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == remove)
        m_store->remove(id);
}

qint64 DocsPage::selectedId() const
{
    const QTreeWidgetItem *item = m_tree->currentItem();
    return item ? item->data(NameColumn, IdRole).toLongLong() : 0;
}
