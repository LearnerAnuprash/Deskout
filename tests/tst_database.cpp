#include "core/database.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

namespace {

int userVersion()
{
    QSqlQuery query(Database::connection());
    return query.exec(QStringLiteral("PRAGMA user_version")) && query.next() ? query.value(0).toInt() : -1;
}

bool tableExists(const QString &name)
{
    QSqlQuery query(Database::connection());
    query.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?"));
    query.addBindValue(name);
    return query.exec() && query.next();
}

} // namespace

class TestDatabase : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString path() const { return m_dir.filePath(QStringLiteral("%1.db").arg(QTest::currentTestFunction())); }

private Q_SLOTS:
    void cleanup()
    {
        Database::close();
    }

    void createsLatestSchema()
    {
        QString error;
        QVERIFY2(Database::open(path(), &error), qPrintable(error));
        QCOMPARE(userVersion(), Database::schemaVersion());
        QVERIFY(tableExists(QStringLiteral("focus_sessions")));
        QVERIFY(tableExists(QStringLiteral("notes")));
    }

    void reopeningKeepsSchema()
    {
        QVERIFY(Database::open(path()));
        Database::close();
        QVERIFY(Database::open(path()));
        QCOMPARE(userVersion(), Database::schemaVersion());
    }

    // A database from Phase 3 (version 1, focus history only) gains the
    // notes table and keeps its data.
    void upgradesVersion1()
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("v1"));
            db.setDatabaseName(path());
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral(
                "CREATE TABLE focus_sessions (id INTEGER PRIMARY KEY AUTOINCREMENT, started_at INTEGER NOT NULL,"
                " day TEXT NOT NULL, planned_seconds INTEGER NOT NULL, focused_seconds INTEGER NOT NULL,"
                " completed INTEGER NOT NULL, topic TEXT NOT NULL DEFAULT '')")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO focus_sessions (started_at, day, planned_seconds,"
                                              " focused_seconds, completed, topic)"
                                              " VALUES (0, '2026-09-27', 1500, 1500, 1, 'kept')")));
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 1")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("v1"));

        QString error;
        QVERIFY2(Database::open(path(), &error), qPrintable(error));
        QCOMPARE(userVersion(), Database::schemaVersion());
        QVERIFY(tableExists(QStringLiteral("notes")));
        QSqlQuery query(Database::connection());
        QVERIFY(query.exec(QStringLiteral("SELECT topic FROM focus_sessions")) && query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("kept"));
    }

    void refusesNewerSchema()
    {
        QVERIFY(Database::open(path()));
        QSqlQuery(Database::connection()).exec(QStringLiteral("PRAGMA user_version = 99"));
        Database::close();

        QString error;
        QVERIFY(!Database::open(path(), &error));
        QVERIFY(error.contains(QLatin1String("newer version")));
        QVERIFY(!Database::isOpen());
        QCOMPARE(Database::openError(), error);
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
