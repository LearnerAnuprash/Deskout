#pragma once

#include <QTimer>
#include <QWidget>

class DocEditor;
class DocsStore;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

// Topic Docs: a file-browser-style list of documents (name, words, last
// edited) and, on click, the document editor with a way back.
class DocsPage : public QWidget
{
    Q_OBJECT

public:
    explicit DocsPage(DocsStore *store, QWidget *parent = nullptr);

    // Opens the document in the editor (e.g. from search).
    void openDoc(qint64 id);
    void newDoc();
    void showList();
    bool isEditing() const;

private:
    QWidget *buildList();
    void reloadList();
    void onItemClicked(QTreeWidgetItem *item);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void showContextMenu(const QPoint &pos);
    void renameSelected();
    void deleteSelected();
    qint64 selectedId() const;

    DocsStore *m_store;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_listView = nullptr;
    QPushButton *m_new = nullptr;
    QLineEdit *m_filter = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_count = nullptr;
    DocEditor *m_editor = nullptr;
    QTimer m_reloadTimer;
};
