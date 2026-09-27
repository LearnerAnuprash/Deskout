#pragma once

#include <QTimer>
#include <QWidget>

class DocsStore;
class QComboBox;
class QLabel;
class QLineEdit;
class QToolButton;
class RichTextEdit;

// One open topic document: back button and editable title on top, a
// formatting toolbar, and the page. Content saves automatically a moment
// after typing stops, and when leaving the document or quitting.
class DocEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DocEditor(DocsStore *store, QWidget *parent = nullptr);

    // False if the document doesn't exist.
    bool open(qint64 id);
    qint64 currentId() const { return m_id; }
    // Writes pending edits now.
    void save();
    // Selects the title so a new document can be named straight away.
    void focusTitle();

Q_SIGNALS:
    void backRequested();

protected:
    void hideEvent(QHideEvent *event) override;

private:
    QWidget *buildToolbar();
    void onContentsChanged();
    void commitTitle();
    void refreshToolbar();
    void refreshStatus();

    DocsStore *m_store;
    QLineEdit *m_title = nullptr;
    QLabel *m_status = nullptr;
    QComboBox *m_style = nullptr;
    QToolButton *m_bold = nullptr;
    QToolButton *m_italic = nullptr;
    QToolButton *m_underline = nullptr;
    QToolButton *m_bullets = nullptr;
    QToolButton *m_numbers = nullptr;
    QToolButton *m_undo = nullptr;
    QToolButton *m_redo = nullptr;
    RichTextEdit *m_edit = nullptr;
    QTimer m_saveTimer;
    qint64 m_id = 0;
    bool m_loading = false;
    bool m_dirty = false;
};
