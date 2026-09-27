#include "core/database.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>

#include <iterator>

namespace {

constexpr char ConnectionName[] = "deskout";

QString g_openError;

QString tr(const char *text)
{
    return QCoreApplication::translate("Database", text);
}

// Each entry upgrades the schema by one version (PRAGMA user_version).
// Never edit a released step; append a new one instead.
const QStringList Migrations[] = {
    // 1: focus timer history
    {
        QStringLiteral("CREATE TABLE focus_sessions ("
                       " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       " started_at INTEGER NOT NULL,"  // ms since epoch, UTC
                       " day TEXT NOT NULL,"            // local date the session started, yyyy-MM-dd
                       " planned_seconds INTEGER NOT NULL,"
                       " focused_seconds INTEGER NOT NULL,"
                       " completed INTEGER NOT NULL,"
                       " topic TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE INDEX focus_sessions_day ON focus_sessions(day)"),
    },
};

bool fail(QString *error, const QString &text)
{
    g_openError = text;
    if (error)
        *error = text;
    return false;
}

bool migrate(QSqlDatabase &db, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return fail(error, query.lastError().text());
    const int current = query.value(0).toInt();
    const int latest = int(std::size(Migrations));
    if (current > latest)
        return fail(error, tr("The database was created by a newer version of Deskout."));

    for (int version = current + 1; version <= latest; ++version) {
        if (!db.transaction())
            return fail(error, db.lastError().text());
        for (const QString &statement : Migrations[version - 1]) {
            if (!query.exec(statement)) {
                const QString text = query.lastError().text();
                db.rollback();
                return fail(error, text);
            }
        }
        if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)) || !db.commit()) {
            const QString text = query.lastError().text();
            db.rollback();
            return fail(error, text);
        }
    }
    return true;
}

} // namespace

namespace Database {

QString defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/deskout.db");
}

bool open(const QString &path, QString *error)
{
    close();
    g_openError.clear();

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        return fail(error, tr("The SQLite driver for Qt is missing."));
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return fail(error, tr("Could not create %1").arg(QFileInfo(path).absolutePath()));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(ConnectionName));
    db.setDatabaseName(path);
    bool ok = db.open();
    if (!ok) {
        fail(error, db.lastError().text());
    } else {
        QSqlQuery(db).exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        ok = migrate(db, error);
    }
    if (!ok) {
        db = QSqlDatabase(); // release the handle so close() can remove it
        close();
    }
    return ok;
}

void close()
{
    {
        QSqlDatabase db = QSqlDatabase::database(QLatin1String(ConnectionName), false);
        if (db.isOpen())
            db.close();
    } // the handle must be gone before removeDatabase()
    if (QSqlDatabase::contains(QLatin1String(ConnectionName)))
        QSqlDatabase::removeDatabase(QLatin1String(ConnectionName));
}

bool isOpen()
{
    return QSqlDatabase::database(QLatin1String(ConnectionName), false).isOpen();
}

QString openError()
{
    return g_openError;
}

QSqlDatabase connection()
{
    return QSqlDatabase::database(QLatin1String(ConnectionName), false);
}

} // namespace Database
