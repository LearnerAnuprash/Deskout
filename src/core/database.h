#pragma once

#include <QSqlDatabase>
#include <QString>

// Deskout's local SQLite database, shared by every feature that keeps
// history or documents. One connection, used from the GUI thread.
namespace Database {

// <app data dir>/deskout.db
QString defaultPath();

// The schema version open() upgrades to.
int schemaVersion();

// Opens (creating if needed) the database at `path` and brings the schema
// up to date. Safe to call again with another path (tests); the previous
// connection is closed first.
bool open(const QString &path, QString *error = nullptr);
void close();
bool isOpen();

// Last error from open(), for the UI.
QString openError();

QSqlDatabase connection();

// "%text%" for `LIKE ? ESCAPE '\'`, with % and _ in `text` matched
// literally.
QString likePattern(const QString &text);

} // namespace Database
