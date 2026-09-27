#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

// Human-friendly timestamps, relative to `today`.
namespace TimeFormat {

// "14:05", "Yesterday 14:05", "Mon 28 Sep 14:05", "28 Sep 2025 14:05"
QString dateTime(const QDateTime &when, const QDate &today = QDate::currentDate());

// Compact form for lists: "14:05", "Yesterday", "Mon", "28 Sep", "28 Sep 2025"
QString compact(const QDateTime &when, const QDate &today = QDate::currentDate());

} // namespace TimeFormat
