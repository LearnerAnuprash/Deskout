#pragma once

#include <QDialog>

struct DailyUpdate;

// "Here's what you noted yesterday": the first thing shown on a new day,
// before the rest of Deskout.
class RecapDialog : public QDialog
{
    Q_OBJECT

public:
    RecapDialog(const DailyUpdate &update, const QDate &today, QWidget *parent = nullptr);

    // "Good morning" / "Good afternoon" / "Good evening".
    static QString greeting(const QTime &now);
    // "Here's what you noted yesterday" or "... on Friday, 26 September".
    static QString subtitle(const QDate &day, const QDate &today);

Q_SIGNALS:
    void openUpdatesRequested();
};
