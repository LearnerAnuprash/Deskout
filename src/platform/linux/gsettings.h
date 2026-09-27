#pragma once

#include <QString>
#include <QStringList>

// Thin wrapper around the `gsettings` CLI (avoids linking GIO) plus the bits
// of GVariant text syntax Deskout needs.
namespace GSettings {

struct Result
{
    bool ok = false;
    QString output;
    QString error;
};

Result run(const QStringList &args);

// GVariant text form of a string: 'it\'s'.
QString quote(QString value);

// Parses "@as []" or "['a', 'b']". *ok is false for anything else.
QStringList parseStringArray(const QString &text, bool *ok);
QString serializeStringArray(const QStringList &items);

// Read/write an array-of-strings key. On failure, *error explains why.
// A value that can't be parsed is reported as an error so callers never
// overwrite the user's list with a misread one.
bool readStringList(const QString &schema, const QString &key, QStringList *out, QString *error);
bool writeStringList(const QString &schema, const QString &key, const QStringList &items, QString *error);

} // namespace GSettings
