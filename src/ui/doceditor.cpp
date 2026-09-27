#include "ui/doceditor.h"

#include "core/docsstore.h"
#include "ui/richtextedit.h"
#include "ui/timeformat.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int SaveDelayMs = 800;

QToolButton *formatButton(const QString &text, const QString &toolTip, bool checkable = true)
{
    auto *button = new QToolButton;
    button->setObjectName(QStringLiteral("FormatButton"));
    button->setText(text);
    button->setToolTip(toolTip);
    button->setCheckable(checkable);
    button->setFocusPolicy(Qt::NoFocus); // keep the caret in the page
    button->setAutoRaise(true);
    return button;
}

QString shortcutText(QKeySequence::StandardKey key)
{
    return QKeySequence(key).toString(QKeySequence::NativeText);
}

QFrame *separator()
{
    auto *line = new QFrame;
    line->setObjectName(QStringLiteral("ToolbarSeparator"));
    line->setFrameShape(QFrame::VLine);
    line->setFixedWidth(1);
    return line;
}

} // namespace

DocEditor::DocEditor(DocsStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    top->setSpacing(10);
    auto *back = new QPushButton(tr("← All documents"));
    back->setObjectName(QStringLiteral("BackButton"));
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(tr("Back to the document list (Alt+Left)"));
    back->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    connect(back, &QPushButton::clicked, this, [this] {
        save();
        Q_EMIT backRequested();
    });
    top->addWidget(back);

    m_title = new QLineEdit;
    m_title->setObjectName(QStringLiteral("DocTitle"));
    m_title->setPlaceholderText(tr("Document name"));
    m_title->setToolTip(tr("Click to rename"));
    m_title->setMaxLength(200);
    connect(m_title, &QLineEdit::editingFinished, this, &DocEditor::commitTitle);
    connect(m_title, &QLineEdit::returnPressed, this, [this] { m_edit->setFocus(); });
    top->addWidget(m_title, 1);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("Muted"));
    top->addWidget(m_status);
    layout->addLayout(top);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(1, 1, 1, 1);
    cardLayout->setSpacing(0);
    cardLayout->addWidget(buildToolbar());
    auto *rule = new QFrame;
    rule->setObjectName(QStringLiteral("ToolbarRule"));
    rule->setFixedHeight(1);
    cardLayout->addWidget(rule);

    m_edit = new RichTextEdit;
    m_edit->setObjectName(QStringLiteral("DocPage"));
    m_edit->setPlaceholderText(tr("Start writing…"));
    connect(m_edit->document(), &QTextDocument::contentsChanged, this, &DocEditor::onContentsChanged);
    connect(m_edit, &RichTextEdit::formatChanged, this, &DocEditor::refreshToolbar);
    connect(m_edit, &QTextEdit::undoAvailable, m_undo, &QWidget::setEnabled);
    connect(m_edit, &QTextEdit::redoAvailable, m_redo, &QWidget::setEnabled);
    cardLayout->addWidget(m_edit, 1);
    layout->addWidget(card, 1);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &DocEditor::save);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &DocEditor::save);
}

QWidget *DocEditor::buildToolbar()
{
    auto *bar = new QWidget;
    bar->setObjectName(QStringLiteral("DocToolbar"));
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(4);

    m_style = new QComboBox;
    m_style->setFocusPolicy(Qt::NoFocus);
    m_style->addItem(tr("Normal text"), 0);
    for (int level = 1; level <= RichTextEdit::MaxHeadingLevel; ++level)
        m_style->addItem(tr("Heading %1").arg(level), level);
    m_style->setToolTip(tr("Paragraph style (Ctrl+Alt+0 to 3)"));
    connect(m_style, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        m_edit->setHeadingLevel(m_style->itemData(index).toInt());
        m_edit->setFocus();
    });
    layout->addWidget(m_style);
    layout->addWidget(separator());

    m_bold = formatButton(tr("B"), tr("Bold (%1)").arg(shortcutText(QKeySequence::Bold)));
    QFont boldFont = m_bold->font();
    boldFont.setBold(true);
    m_bold->setFont(boldFont);
    connect(m_bold, &QToolButton::clicked, this, [this](bool on) { m_edit->setBold(on); });
    layout->addWidget(m_bold);

    m_italic = formatButton(tr("I"), tr("Italic (%1)").arg(shortcutText(QKeySequence::Italic)));
    QFont italicFont = m_italic->font();
    italicFont.setItalic(true);
    m_italic->setFont(italicFont);
    connect(m_italic, &QToolButton::clicked, this, [this](bool on) { m_edit->setItalic(on); });
    layout->addWidget(m_italic);

    m_underline = formatButton(tr("U"), tr("Underline (%1)").arg(shortcutText(QKeySequence::Underline)));
    QFont underlineFont = m_underline->font();
    underlineFont.setUnderline(true);
    m_underline->setFont(underlineFont);
    connect(m_underline, &QToolButton::clicked, this, [this](bool on) { m_edit->setUnderline(on); });
    layout->addWidget(m_underline);
    layout->addWidget(separator());

    m_bullets = formatButton(tr("• List"), tr("Bulleted list (or type \"- \" at the start of a line)"));
    connect(m_bullets, &QToolButton::clicked, this, [this](bool on) {
        m_edit->setListStyle(on ? RichTextEdit::ListStyle::Bullet : RichTextEdit::ListStyle::None);
    });
    layout->addWidget(m_bullets);
    m_numbers = formatButton(tr("1. List"), tr("Numbered list (or type \"1. \" at the start of a line)"));
    connect(m_numbers, &QToolButton::clicked, this, [this](bool on) {
        m_edit->setListStyle(on ? RichTextEdit::ListStyle::Numbered : RichTextEdit::ListStyle::None);
    });
    layout->addWidget(m_numbers);
    layout->addWidget(separator());

    m_undo = formatButton(tr("Undo"), tr("Undo (%1)").arg(shortcutText(QKeySequence::Undo)), false);
    m_undo->setEnabled(false);
    connect(m_undo, &QToolButton::clicked, this, [this] { m_edit->undo(); });
    layout->addWidget(m_undo);
    m_redo = formatButton(tr("Redo"), tr("Redo (%1)").arg(shortcutText(QKeySequence::Redo)), false);
    m_redo->setEnabled(false);
    connect(m_redo, &QToolButton::clicked, this, [this] { m_edit->redo(); });
    layout->addWidget(m_redo);
    layout->addStretch(1);
    return bar;
}

bool DocEditor::open(qint64 id)
{
    save();
    const std::optional<Doc> doc = m_store->get(id);
    if (!doc)
        return false;
    m_id = id;
    m_title->setText(doc->title);
    m_title->setCursorPosition(0);
    m_loading = true;
    m_edit->setHtml(doc->html);
    m_loading = false;
    m_edit->document()->clearUndoRedoStacks();
    m_edit->moveCursor(QTextCursor::Start);
    m_dirty = false;
    refreshToolbar();
    refreshStatus();
    m_edit->setFocus();
    return true;
}

void DocEditor::save()
{
    m_saveTimer.stop();
    if (!m_dirty || m_id == 0)
        return;
    if (!m_store->saveContent(m_id, m_edit->toHtml(), m_edit->toPlainText())) {
        m_status->setText(tr("Couldn't save"));
        return; // still dirty: the next edit retries
    }
    m_dirty = false;
    refreshStatus();
}

void DocEditor::focusTitle()
{
    m_title->setFocus();
    m_title->selectAll();
}

void DocEditor::hideEvent(QHideEvent *event)
{
    commitTitle();
    save();
    QWidget::hideEvent(event);
}

void DocEditor::onContentsChanged()
{
    if (m_loading || m_id == 0)
        return;
    m_dirty = true;
    m_status->setText(tr("Editing…"));
    m_saveTimer.start();
}

void DocEditor::commitTitle()
{
    if (m_id == 0)
        return;
    const std::optional<Doc> doc = m_store->get(m_id);
    if (!doc)
        return;
    const QString title = m_title->text().simplified();
    if (title.isEmpty()) {
        m_title->setText(doc->title); // a document always has a name
        return;
    }
    if (title != doc->title)
        m_store->rename(m_id, title);
}

void DocEditor::refreshToolbar()
{
    const int level = m_edit->headingLevel();
    m_style->setCurrentIndex(m_style->findData(level));
    m_bold->setChecked(m_edit->isBold());
    m_italic->setChecked(m_edit->isItalic());
    m_underline->setChecked(m_edit->isUnderline());
    const RichTextEdit::ListStyle list = m_edit->listStyle();
    m_bullets->setChecked(list == RichTextEdit::ListStyle::Bullet);
    m_numbers->setChecked(list == RichTextEdit::ListStyle::Numbered);
}

void DocEditor::refreshStatus()
{
    const std::optional<Doc> doc = m_store->get(m_id);
    if (!doc) {
        m_status->clear();
        return;
    }
    const QString words = doc->wordCount == 1 ? tr("1 word") : tr("%1 words").arg(doc->wordCount);
    m_status->setText(tr("%1 · Edited %2").arg(words, TimeFormat::dateTime(doc->updatedAt)));
}
