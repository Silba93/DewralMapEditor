#ifndef POSITIONTEXT_H
#define POSITIONTEXT_H

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringView>
#include <QtGlobal>

// Position strings exchanged through the clipboard by the map context menu.
// Kept free of Qt GUI/QML dependencies so the formats can be unit tested and
// stay in sync between "Copy Position As" and every paste entry point (the
// "Paste Teleport Location" menu action and the Go To Position dialog).
namespace PositionText {

// Supported "Copy Position As" styles. Unknown styles fall back to plain.
inline QString formatText(const QString &format, int x, int y, int z)
{
    if (format == QLatin1String("tuple"))
        return QStringLiteral("(%1, %2, %3)").arg(x).arg(y).arg(z);
    if (format == QLatin1String("position"))
        return QStringLiteral("Position(%1, %2, %3)").arg(x).arg(y).arg(z);
    if (format == QLatin1String("lua"))
        return QStringLiteral("{x = %1, y = %2, z = %3}").arg(x).arg(y).arg(z);
    if (format == QLatin1String("json"))
        return QStringLiteral("{\"x\":%1,\"y\":%2,\"z\":%3}").arg(x).arg(y).arg(z);
    return QStringLiteral("%1, %2, %3").arg(x).arg(y).arg(z);
}

namespace Detail {

inline QList<int> integersIn(QStringView text)
{
    QList<int> values;
    qsizetype index = 0;
    const qsizetype size = text.size();
    while (index < size) {
        const QChar character = text.at(index);
        const bool negative = character == QLatin1Char('-') && index + 1 < size
                              && text.at(index + 1).isDigit();
        if (character.isDigit() || negative) {
            if (negative) ++index;
            qint64 value = 0;
            while (index < size && text.at(index).isDigit()) {
                // Clamp instead of overflowing on garbage input; the result is
                // rejected by the range check anyway.
                value = qMin<qint64>(value * 10 + text.at(index).digitValue(),
                                     1000000000);
                ++index;
            }
            values.append(static_cast<int>(negative ? -value : value));
        } else {
            ++index;
        }
    }
    return values;
}

} // namespace Detail

// Accepts every style produced by formatText(): plain "x, y, z", "(x, y, z)",
// "Position(x, y, z)", "{x = ..., y = ..., z = ...}" and
// {"x":...,"y":...,"z":...} - including reordered/uppercase keys and arbitrary
// whitespace. The values are validated against the map bounds.
inline bool parseText(const QString &text, int *outX, int *outY, int *outZ)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return false;

    const auto accept = [outX, outY, outZ](const QList<int> &values) {
        const int x = values.at(0);
        const int y = values.at(1);
        const int z = values.at(2);
        if (x < 0 || x > 65535 || y < 0 || y > 65535 || z < 0 || z > 15)
            return false;
        if (outX) *outX = x;
        if (outY) *outY = y;
        if (outZ) *outZ = z;
        return true;
    };

    // Lua tables, JSON objects and "x=..., y=..., z=..." style logs.
    static const QRegularExpression keyed(
        QStringLiteral("\\b([xyz])[\"']?\\s*[:=]\\s*(-?\\d+)"),
        QRegularExpression::CaseInsensitiveOption);
    int keyedValues[3] = {0, 0, 0};
    bool keyedSeen[3] = {false, false, false};
    int keyedCount = 0;
    QRegularExpressionMatchIterator matches = keyed.globalMatch(trimmed);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const int index = match.captured(1).at(0).toLower().unicode() - u'x';
        if (index < 0 || index > 2) continue;
        keyedValues[index] = match.captured(2).toInt();
        if (!keyedSeen[index]) {
            keyedSeen[index] = true;
            ++keyedCount;
        }
    }
    if (keyedCount == 3)
        return accept({keyedValues[0], keyedValues[1], keyedValues[2]});
    if (keyedCount >= 2)
        return false; // Keyed text with a coordinate missing.

    // "Position(x, y, z)" and "(x, y, z)".
    const qsizetype open = trimmed.indexOf(QLatin1Char('('));
    if (open >= 0) {
        const qsizetype close = trimmed.indexOf(QLatin1Char(')'), open + 1);
        const qsizetype end = close < 0 ? trimmed.size() : close;
        const QList<int> values =
            Detail::integersIn(QStringView(trimmed).sliced(open + 1, end - open - 1));
        if (values.size() >= 3) return accept(values);
    }

    const QList<int> values = Detail::integersIn(QStringView(trimmed));
    if (values.size() >= 3) return accept(values);
    return false;
}

} // namespace PositionText

#endif
