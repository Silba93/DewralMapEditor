#include "datreader.h"
#include "otbreader.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QByteArray minimalDat()
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(0x12345678) << quint16(100)
           << quint16(0) << quint16(0) << quint16(0);
    stream << quint8(12) << quint8(0xff);
    stream << quint8(1) << quint8(1) << quint8(1)
           << quint8(1) << quint8(1) << quint8(1) << quint8(1);
    stream << quint16(1);
    return bytes;
}

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::fprintf(stderr, "%s\n", message);
    return false;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create test directory")) return 1;

    const QString datPath = directory.filePath(QStringLiteral("Tibia.dat"));
    const QString tomlPath = directory.filePath(QStringLiteral("items.toml"));
    if (!require(writeFile(datPath, minimalDat()), "Could not write synthetic DAT")) return 1;
    if (!require(writeFile(tomlPath,
                           QByteArrayLiteral("[[items]]\n"
                                             "id = 100\n"
                                             "name = \"BlackTek teleporter\"\n"
                                             "description = \"test destination\"\n"
                                             "type = \"teleport\"\n"
                                             "rotateTo = 101\n"
                                             "floorchange = \"down\"\n"
                                             "blockprojectile = true\n")),
                 "Could not write TOML")) return 1;

    DatReader dat;
    dat.setClientVersion(772);
    if (!require(dat.loadFile(datPath, 0), "Synthetic DAT could not be loaded")) return 1;

    OtbReader otb;
    otb.setDatReader(&dat);
    if (!require(otb.loadTomlFile(tomlPath, 772), "TOML item loading failed")) return 1;
    if (!require(otb.clientIdForServerId(100) == 100,
                 "DAT server/client identity mapping failed")) return 1;
    if (!require(otb.nameForServerId(100) == QStringLiteral("BlackTek teleporter"),
                 "TOML name overlay failed")) return 1;
    if (!require(otb.isTeleportItem(100) && otb.rotateToForServerId(100) == 101,
                 "TOML type or rotation overlay failed")) return 1;
    if (!require(otb.blocksPathForServerId(100),
                 "DAT/TOML blocking behavior failed")) return 1;
    if (!require(otb.detailsAt(otb.rowForServerId(100))
                     .value(QStringLiteral("description")).toString()
                     == QStringLiteral("test destination"),
                 "TOML description overlay failed")) return 1;

    if (!require(writeFile(tomlPath, QByteArrayLiteral("items = true\n")),
                 "Could not write invalid TOML shape")) return 1;
    if (!require(!otb.loadTomlFile(tomlPath, 772)
                 && otb.errorString().contains(QStringLiteral("items")),
                 "Missing TOML items array was accepted")) return 1;

    if (!require(writeFile(tomlPath, QByteArrayLiteral("[[items]\nid = 100\n")),
                 "Could not write malformed TOML")) return 1;
    if (!require(!otb.loadTomlFile(tomlPath, 772)
                 && otb.errorString().contains(QStringLiteral("parse"), Qt::CaseInsensitive),
                 "Malformed TOML was accepted")) return 1;

    return 0;
}
