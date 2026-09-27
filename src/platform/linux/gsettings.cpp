#include "platform/linux/gsettings.h"

#include <QProcess>
#include <QRegularExpression>

namespace GSettings {

Result run(const QStringList &args)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), args);
    Result result;
    if (!process.waitForFinished(3000)) {
        process.kill();
        result.error = process.errorString();
        return result;
    }
    result.ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    result.error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    return result;
}

QString quote(QString value)
{
    value.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    value.replace(QLatin1Char('\''), QLatin1String("\\'"));
    return QLatin1Char('\'') + value + QLatin1Char('\'');
}

QStringList parseStringArray(const QString &text, bool *ok)
{
    QString body = text.trimmed();
    if (body.startsWith(QLatin1String("@as")))
        body = body.mid(3).trimmed();
    *ok = body.startsWith(QLatin1Char('[')) && body.endsWith(QLatin1Char(']'));
    QStringList items;
    if (!*ok)
        return items;
    static const QRegularExpression item(QStringLiteral(R"('((?:[^'\\]|\\.)*)')"));
    auto it = item.globalMatch(body);
    while (it.hasNext()) {
        QString value = it.next().captured(1);
        value.replace(QLatin1String("\\'"), QLatin1String("'"));
        value.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        items << value;
    }
    return items;
}

QString serializeStringArray(const QStringList &items)
{
    QStringList quoted;
    for (const QString &value : items)
        quoted << quote(value);
    return QLatin1Char('[') + quoted.join(QLatin1String(", ")) + QLatin1Char(']');
}

bool readStringList(const QString &schema, const QString &key, QStringList *out, QString *error)
{
    const Result r = run({QStringLiteral("get"), schema, key});
    if (!r.ok) {
        if (error)
            *error = r.error.isEmpty() ? QStringLiteral("gsettings failed") : r.error;
        return false;
    }
    bool parsed = false;
    *out = parseStringArray(r.output, &parsed);
    if (!parsed && error)
        *error = QStringLiteral("unexpected value \"%1\"").arg(r.output);
    return parsed;
}

bool writeStringList(const QString &schema, const QString &key, const QStringList &items, QString *error)
{
    const Result r = run({QStringLiteral("set"), schema, key, serializeStringArray(items)});
    if (!r.ok && error)
        *error = r.error.isEmpty() ? QStringLiteral("gsettings failed") : r.error;
    return r.ok;
}

} // namespace GSettings
