#include "ui/richtextedit.h"

#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTextList>

#include <algorithm>
#include <functional>

namespace {

constexpr int DocumentMargin = 24;

bool isBulletStyle(QTextListFormat::Style style)
{
    return style == QTextListFormat::ListDisc || style == QTextListFormat::ListCircle
           || style == QTextListFormat::ListSquare;
}

// Nested levels alternate markers, like word processors do.
QTextListFormat::Style styleForLevel(bool bullet, int indent)
{
    static const QTextListFormat::Style bullets[] = {QTextListFormat::ListDisc, QTextListFormat::ListCircle,
                                                     QTextListFormat::ListSquare};
    static const QTextListFormat::Style numbers[] = {QTextListFormat::ListDecimal, QTextListFormat::ListLowerAlpha,
                                                     QTextListFormat::ListLowerRoman};
    const int i = std::max(0, indent - 1) % 3;
    return bullet ? bullets[i] : numbers[i];
}

// Calls `fn` for every block the cursor's selection touches (or the
// cursor's block).
void forEachBlock(const QTextCursor &cursor, const std::function<void(const QTextBlock &)> &fn)
{
    QTextDocument *document = cursor.document();
    QTextBlock block = document->findBlock(cursor.selectionStart());
    const QTextBlock last = document->findBlock(cursor.selectionEnd());
    while (block.isValid()) {
        fn(block);
        if (block == last)
            break;
        block = block.next();
    }
}

// Space above headings, as in word processors.
void setHeadingSpacing(QTextBlockFormat *format, int level)
{
    format->setTopMargin(level > 0 ? 14 - 2 * level : 0);
    format->setBottomMargin(level > 0 ? 4 : 0);
}

QTextCharFormat headingCharFormat(int level)
{
    QTextCharFormat format;
    format.setFontWeight(level > 0 ? QFont::Bold : QFont::Normal);
    // Relative sizes (large, x-large, xx-large) survive the HTML round trip
    // and follow the editor's base font.
    format.setProperty(QTextFormat::FontSizeAdjustment, level > 0 ? RichTextEdit::MaxHeadingLevel + 1 - level : 0);
    return format;
}

} // namespace

RichTextEdit::RichTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptRichText(true);
    setFrameShape(QFrame::NoFrame);
    document()->setDocumentMargin(DocumentMargin);
    connect(this, &QTextEdit::currentCharFormatChanged, this, &RichTextEdit::formatChanged);
    connect(this, &QTextEdit::cursorPositionChanged, this, &RichTextEdit::formatChanged);
}

bool RichTextEdit::isBold() const
{
    return currentCharFormat().fontWeight() >= QFont::DemiBold;
}

bool RichTextEdit::isItalic() const
{
    return currentCharFormat().fontItalic();
}

bool RichTextEdit::isUnderline() const
{
    return currentCharFormat().fontUnderline();
}

int RichTextEdit::headingLevel() const
{
    return textCursor().blockFormat().headingLevel();
}

RichTextEdit::ListStyle RichTextEdit::listStyle() const
{
    const QTextList *list = textCursor().currentList();
    if (!list)
        return ListStyle::None;
    return isBulletStyle(list->format().style()) ? ListStyle::Bullet : ListStyle::Numbered;
}

void RichTextEdit::setBold(bool on)
{
    QTextCharFormat format;
    format.setFontWeight(on ? QFont::Bold : QFont::Normal);
    mergeFormat(format);
}

void RichTextEdit::setItalic(bool on)
{
    QTextCharFormat format;
    format.setFontItalic(on);
    mergeFormat(format);
}

void RichTextEdit::setUnderline(bool on)
{
    QTextCharFormat format;
    format.setFontUnderline(on);
    mergeFormat(format);
}

void RichTextEdit::setHeadingLevel(int level)
{
    level = std::clamp(level, 0, MaxHeadingLevel);
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    const QTextCharFormat charFormat = headingCharFormat(level);
    forEachBlock(cursor, [&](const QTextBlock &block) {
        QTextCursor blockCursor(block);
        QTextBlockFormat format = block.blockFormat();
        format.setHeadingLevel(level);
        setHeadingSpacing(&format, level);
        blockCursor.setBlockFormat(format);
        blockCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        blockCursor.mergeCharFormat(charFormat);
        blockCursor.mergeBlockCharFormat(charFormat);
    });
    cursor.endEditBlock();
    mergeCurrentCharFormat(charFormat);
    Q_EMIT formatChanged();
}

void RichTextEdit::setListStyle(ListStyle style)
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    if (style == ListStyle::None) {
        forEachBlock(cursor, [](const QTextBlock &block) {
            if (QTextList *list = block.textList()) {
                list->remove(block);
                QTextBlockFormat format = block.blockFormat();
                format.setIndent(0);
                QTextCursor(block).setBlockFormat(format);
            }
        });
    } else {
        const bool bullet = style == ListStyle::Bullet;
        QTextList *current = cursor.currentList();
        if (current && !cursor.hasSelection()) {
            // Switching bullets <-> numbers: the whole list follows.
            QTextListFormat format = current->format();
            format.setStyle(styleForLevel(bullet, format.indent()));
            current->setFormat(format);
        } else {
            QTextListFormat format;
            format.setIndent(1);
            format.setStyle(styleForLevel(bullet, 1));
            cursor.createList(format);
        }
    }
    cursor.endEditBlock();
    Q_EMIT formatChanged();
}

void RichTextEdit::changeListIndent(int delta)
{
    QTextCursor cursor = textCursor();
    QTextList *list = cursor.currentList();
    if (!list || delta == 0)
        return;
    QTextListFormat format = list->format();
    const int target = format.indent() + delta;
    const QTextBlock block = cursor.block();

    cursor.beginEditBlock();
    if (target < 1) {
        list->remove(block);
        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setIndent(0);
        cursor.setBlockFormat(blockFormat);
    } else {
        const bool bullet = isBulletStyle(format.style());
        // Rejoin the list already at that level just above, so numbering
        // carries on (1, 2, a, b, 3) instead of restarting.
        QTextList *join = nullptr;
        for (QTextBlock previous = block.previous(); previous.isValid(); previous = previous.previous()) {
            QTextList *candidate = previous.textList();
            if (!candidate)
                break;
            const int indent = candidate->format().indent();
            if (indent == target && isBulletStyle(candidate->format().style()) == bullet) {
                join = candidate;
                break;
            }
            if (indent < target)
                break;
        }
        if (join) {
            join->add(block);
        } else {
            format.setIndent(target);
            format.setStyle(styleForLevel(bullet, target));
            cursor.createList(format);
        }
    }
    cursor.endEditBlock();
    Q_EMIT formatChanged();
}

void RichTextEdit::cleanPastedFormatting(QTextDocument *document)
{
    struct Span
    {
        int position;
        int length;
        QTextCharFormat format;
    };
    QList<Span> spans;
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        // Web pages bring their own spacing and colours; keep headings,
        // alignment and lists only.
        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.clearBackground();
        blockFormat.clearForeground();
        for (int property : {QTextFormat::BlockTopMargin, QTextFormat::BlockBottomMargin,
                             QTextFormat::BlockLeftMargin, QTextFormat::BlockRightMargin,
                             QTextFormat::TextIndent, QTextFormat::LineHeight, QTextFormat::LineHeightType})
            blockFormat.clearProperty(property);
        setHeadingSpacing(&blockFormat, blockFormat.headingLevel());
        QTextCursor(block).setBlockFormat(blockFormat);
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid())
                spans << Span{fragment.position(), fragment.length(), fragment.charFormat()};
        }
    }
    // Back to front, so removing an image doesn't shift the spans still to do.
    for (auto it = spans.crbegin(); it != spans.crend(); ++it) {
        cursor.setPosition(it->position);
        cursor.setPosition(it->position + it->length, QTextCursor::KeepAnchor);
        if (it->format.isImageFormat()) {
            // Usually a web URL the editor couldn't load anyway.
            cursor.removeSelectedText();
            continue;
        }
        QTextCharFormat format = it->format;
        format.clearForeground();
        format.clearBackground();
        for (int property : {QTextFormat::FontFamilies, QTextFormat::FontStyleName, QTextFormat::FontPointSize,
                             QTextFormat::FontPixelSize, QTextFormat::FontLetterSpacing,
                             QTextFormat::FontWordSpacing, QTextFormat::TextOutline})
            format.clearProperty(property);
        cursor.setCharFormat(format);
    }
    cursor.endEditBlock();
}

void RichTextEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasHtml()) {
        QTextDocument pasted;
        pasted.setHtml(source->html());
        cleanPastedFormatting(&pasted);
        textCursor().insertFragment(QTextDocumentFragment(&pasted));
        ensureCursorVisible();
        return;
    }
    QTextEdit::insertFromMimeData(source);
}

void RichTextEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Bold)) {
        setBold(!isBold());
        return;
    }
    if (event->matches(QKeySequence::Italic)) {
        setItalic(!isItalic());
        return;
    }
    if (event->matches(QKeySequence::Underline)) {
        setUnderline(!isUnderline());
        return;
    }
    // Ctrl+Alt+0..3: normal text / heading 1-3 (as in Google Docs).
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (modifiers == (Qt::ControlModifier | Qt::AltModifier) && event->key() >= Qt::Key_0
        && event->key() <= Qt::Key_0 + MaxHeadingLevel) {
        setHeadingLevel(event->key() - Qt::Key_0);
        return;
    }

    QTextCursor cursor = textCursor();
    if ((event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) && cursor.currentList()) {
        const bool outdent = event->key() == Qt::Key_Backtab || (modifiers & Qt::ShiftModifier);
        changeListIndent(outdent ? -1 : 1);
        return;
    }

    // Enter at the end of a heading continues with normal text.
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && modifiers == Qt::NoModifier
        && !cursor.hasSelection() && cursor.atBlockEnd() && cursor.blockFormat().headingLevel() > 0) {
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setHeadingLevel(0);
        setHeadingSpacing(&blockFormat, 0);
        cursor.insertBlock(blockFormat, headingCharFormat(0));
        setTextCursor(cursor);
        setCurrentCharFormat(headingCharFormat(0));
        ensureCursorVisible();
        return;
    }

    // "- " or "* " starts a bullet list, "1. " a numbered one.
    if (event->key() == Qt::Key_Space && modifiers == Qt::NoModifier && !cursor.hasSelection()
        && !cursor.currentList()) {
        const QString typed = cursor.block().text().left(cursor.positionInBlock());
        const bool bullet = typed == QLatin1String("-") || typed == QLatin1String("*");
        const bool numbered = typed == QLatin1String("1.");
        if (bullet || numbered) {
            cursor.beginEditBlock();
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            QTextListFormat format;
            format.setIndent(1);
            format.setStyle(styleForLevel(bullet, 1));
            cursor.createList(format);
            cursor.endEditBlock();
            setTextCursor(cursor);
            Q_EMIT formatChanged();
            return;
        }
    }
    QTextEdit::keyPressEvent(event);
}

void RichTextEdit::resizeEvent(QResizeEvent *event)
{
    // Margins first: QTextEdit re-flows the text to the viewport width in
    // its own resizeEvent, which must see the final width.
    updatePageMargins();
    QTextEdit::resizeEvent(event);
}

void RichTextEdit::mergeFormat(const QTextCharFormat &format)
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        cursor.mergeCharFormat(format);
    mergeCurrentCharFormat(format);
    Q_EMIT formatChanged();
}

void RichTextEdit::updatePageMargins()
{
    // Centre a page-width column in wide windows. Based on the widget
    // width, not the viewport's, so a scrollbar appearing can't make the
    // margins oscillate.
    const int side = std::max(0, (width() - PageWidth) / 2);
    if (viewportMargins().left() != side)
        setViewportMargins(side, 0, side, 0);
}
