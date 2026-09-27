#pragma once

#include <QTextEdit>

class QTextDocument;

// QTextEdit with the handful of word-processor behaviours Topic Docs
// needs: headings, bullet/numbered lists with Tab/Shift+Tab nesting, a
// centred page width, and pasting that keeps structure (headings, bold,
// lists, links) but drops colours, fonts and images, so pasted text looks
// like the rest of the document in light and dark themes.
class RichTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    enum class ListStyle { None, Bullet, Numbered };

    // Maximum line length; wider windows get side margins.
    static constexpr int PageWidth = 760;
    static constexpr int MaxHeadingLevel = 3;

    explicit RichTextEdit(QWidget *parent = nullptr);

    bool isBold() const;
    bool isItalic() const;
    bool isUnderline() const;
    // 0 for normal text.
    int headingLevel() const;
    ListStyle listStyle() const;

    // Apply to the selection, or to what is typed next.
    void setBold(bool on);
    void setItalic(bool on);
    void setUnderline(bool on);
    // Whole paragraphs touched by the selection.
    void setHeadingLevel(int level);
    void setListStyle(ListStyle style);
    // Nest (+1) or un-nest (-1) the current list item.
    void changeListIndent(int delta);

    // Strips colours, backgrounds, font families/sizes and images; keeps
    // structure and emphasis. Public for tests.
    static void cleanPastedFormatting(QTextDocument *document);

Q_SIGNALS:
    // The cursor moved or formatting changed: refresh toolbar state.
    void formatChanged();

protected:
    void insertFromMimeData(const QMimeData *source) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void mergeFormat(const QTextCharFormat &format);
    void updatePageMargins();
};
