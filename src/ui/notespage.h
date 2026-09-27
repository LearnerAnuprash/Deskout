#pragma once

#include <QTimer>
#include <QWidget>

class NotesStore;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

// Notes: a list (title, preview, last edited) next to an editor. Edits
// save automatically a moment after typing stops, and when leaving the
// note or quitting.
class NotesPage : public QWidget
{
    Q_OBJECT

public:
    explicit NotesPage(NotesStore *store, QWidget *parent = nullptr);

    // Shows the note in the editor (e.g. from search).
    void openNote(qint64 id);
    void newNote();
    // Writes pending edits now.
    void save();

protected:
    void hideEvent(QHideEvent *event) override;

private:
    QWidget *buildListPanel();
    QWidget *buildEditor();
    void reloadList();
    void onCurrentItemChanged(QListWidgetItem *current);
    void onEdited();
    // Saves, and drops the note if it was left completely empty.
    void leaveCurrent();
    void deleteCurrent();
    void showEditor(bool show);
    void refreshMeta();
    void selectInList(qint64 id);

    NotesStore *m_store;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_count = nullptr;
    QPushButton *m_new = nullptr;
    QStackedWidget *m_detail = nullptr;
    QLabel *m_emptyText = nullptr;
    QLabel *m_meta = nullptr;
    QLineEdit *m_title = nullptr;
    QPlainTextEdit *m_body = nullptr;
    QPushButton *m_delete = nullptr;
    QTimer m_saveTimer;
    QTimer m_reloadTimer;
    // 0 while a new note hasn't been saved yet.
    qint64 m_currentId = 0;
    bool m_editing = false;
    bool m_dirty = false;
};
