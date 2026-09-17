#include "positiontext.h"

#include <QStringList>

#include <cstdlib>
#include <iostream>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
}

bool parses(const QString &text, int expectedX, int expectedY, int expectedZ)
{
    int x = -1;
    int y = -1;
    int z = -1;
    if (!PositionText::parseText(text, &x, &y, &z)) return false;
    return x == expectedX && y == expectedY && z == expectedZ;
}

bool rejects(const QString &text)
{
    int x = 0;
    int y = 0;
    int z = 0;
    return !PositionText::parseText(text, &x, &y, &z);
}

}

int main()
{
    const int x = 32369;
    const int y = 32241;
    const int z = 7;

    // "Copy Position As" keeps emitting exactly these strings.
    if (!require(PositionText::formatText(QStringLiteral("plain"), x, y, z)
                     == QStringLiteral("32369, 32241, 7"),
                 "The plain format changed"))
        return EXIT_FAILURE;
    if (!require(PositionText::formatText(QStringLiteral("tuple"), x, y, z)
                     == QStringLiteral("(32369, 32241, 7)"),
                 "The tuple format changed"))
        return EXIT_FAILURE;
    if (!require(PositionText::formatText(QStringLiteral("position"), x, y, z)
                     == QStringLiteral("Position(32369, 32241, 7)"),
                 "The OTClient format changed"))
        return EXIT_FAILURE;
    if (!require(PositionText::formatText(QStringLiteral("lua"), x, y, z)
                     == QStringLiteral("{x = 32369, y = 32241, z = 7}"),
                 "The Lua format changed"))
        return EXIT_FAILURE;
    if (!require(PositionText::formatText(QStringLiteral("json"), x, y, z)
                     == QStringLiteral("{\"x\":32369,\"y\":32241,\"z\":7}"),
                 "The JSON format changed"))
        return EXIT_FAILURE;
    if (!require(PositionText::formatText(QStringLiteral("unknown"), x, y, z)
                     == QStringLiteral("32369, 32241, 7"),
                 "An unknown format did not fall back to plain"))
        return EXIT_FAILURE;

    // Every copied style is pasted back by "Paste Teleport Location".
    const QStringList formats = {
        QStringLiteral("plain"), QStringLiteral("tuple"),
        QStringLiteral("position"), QStringLiteral("lua"),
        QStringLiteral("json")};
    for (const QString &format : formats) {
        if (!require(parses(PositionText::formatText(format, x, y, z), x, y, z),
                     "A copied position did not round-trip"))
            return EXIT_FAILURE;
    }

    // Variations of the supported styles.
    if (!require(parses(QStringLiteral("{z = 7, y = 32241, x = 32369}"), x, y, z),
                 "A reordered Lua table was rejected"))
        return EXIT_FAILURE;
    if (!require(parses(QStringLiteral("{\"Z\":7,\"Y\":32241,\"X\":32369}"), x, y, z),
                 "An uppercase JSON object was rejected"))
        return EXIT_FAILURE;
    if (!require(parses(QStringLiteral("{ \"x\" : 32369 , \"y\" : 32241 , \"z\" : 7 }"),
                        x, y, z),
                 "A spaced JSON object was rejected"))
        return EXIT_FAILURE;
    if (!require(parses(QStringLiteral("Position(32369,32241,7)"), x, y, z),
                 "A compact OTClient position was rejected"))
        return EXIT_FAILURE;
    if (!require(parses(QStringLiteral("  ( 32369 , 32241 , 7 )  "), x, y, z),
                 "A spaced tuple was rejected"))
        return EXIT_FAILURE;
    if (!require(parses(QStringLiteral(" 32369 ,\n\t32241 , 7 "), x, y, z),
                 "A multi-line plain position was rejected"))
        return EXIT_FAILURE;
    if (!require(parses(QStringLiteral("moved to 32369, 32241, 7"), x, y, z),
                 "A plain position inside surrounding text was rejected"))
        return EXIT_FAILURE;

    // Malformed, keyed-incomplete or out of range clipboard content.
    const QStringList invalid = {
        QString(),
        QStringLiteral("   "),
        QStringLiteral("hello"),
        QStringLiteral("32369, 32241"),
        QStringLiteral("65536, 32241, 7"),
        QStringLiteral("32369, 32241, 16"),
        QStringLiteral("-1, 32241, 7"),
        QStringLiteral("{\"x\":32369,\"y\":32241}"),
        QStringLiteral("x = 32369, y = 32241")};
    for (const QString &text : invalid) {
        if (!require(rejects(text), "An invalid position was accepted"))
            return EXIT_FAILURE;
    }

    std::cout << "position text formats verified\n";
    return EXIT_SUCCESS;
}
