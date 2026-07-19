#include "tilesetstore.h"

#include "dmedatadir.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

TilesetStore::TilesetStore(QObject *parent)
    : QObject(parent)
{
}

void TilesetStore::clear()
{
    m_path.clear();
    m_names.clear();
    m_items.clear();
    bump();
}

namespace {
constexpr const char *kCategories[] = { "terrain", "doodad", "item", "raw" };
}

bool TilesetStore::loadJsonInto(const QString &path,
                                 QHash<QString, QStringList> &names,
                                 QHash<QString, QHash<QString, QVariantList>> &items)
{
    if (!QFile::exists(path)) return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    bool any = false;
    for (const char *catC : kCategories) {
        const QString cat = QString::fromLatin1(catC);
        if (!root.contains(cat) || !root.value(cat).isObject()) continue;
        const QJsonObject catObj = root.value(cat).toObject();
        for (auto it = catObj.begin(); it != catObj.end(); ++it) {
            if (!it.value().isArray()) continue;
            QVariantList ids;
            for (const QJsonValue &v : it.value().toArray()) ids.append(v.toInt());
            if (ids.isEmpty()) continue;
            names[cat].append(it.key());
            items[cat][it.key()] = ids;
            any = true;
        }
    }
    return any;
}

void TilesetStore::saveJson() const
{
    if (m_path.isEmpty()) return;   // brak wersji (nic nie wczytano) - nie ma gdzie zapisac

    QJsonObject root;
    for (const char *catC : kCategories) {
        const QString cat = QString::fromLatin1(catC);
        const auto namesIt = m_names.find(cat);
        if (namesIt == m_names.end() || namesIt->isEmpty()) continue;
        QJsonObject catObj;
        const auto &itemsForCat = m_items.value(cat);
        for (const QString &name : *namesIt) {
            QJsonArray arr;
            for (const QVariant &id : itemsForCat.value(name)) arr.append(id.toInt());
            catObj.insert(name, arr);
        }
        root.insert(cat, catObj);
    }

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool TilesetStore::loadForVersion(int clientVersion)
{
    return loadForDir(QString::number(clientVersion));
}

bool TilesetStore::loadForDir(const QString &dirName)
{
    clear();
    m_path = QDir(dmeDataDir()).filePath(QStringLiteral("%1/tilesets.json").arg(dirName));
    const bool has = loadJsonInto(m_path, m_names, m_items);
    bump();
    return has;
}

QStringList TilesetStore::namesFor(const QString &category) const
{
    return m_names.value(category);
}

QVariantList TilesetStore::itemsFor(const QString &category, const QString &name) const
{
    return m_items.value(category).value(name);
}

bool TilesetStore::isCustomOnly(const QString &category, const QString &name) const
{
    return m_names.value(category).contains(name);
}

bool TilesetStore::newTileset(const QString &category, const QString &name)
{
    if (name.isEmpty()) return false;
    if (m_names.value(category).contains(name)) return false;
    m_names[category].append(name);
    m_items[category][name] = {};
    saveJson();
    bump();
    return true;
}

void TilesetStore::deleteTileset(const QString &category, const QString &name)
{
    if (!m_names.value(category).contains(name)) return;
    m_names[category].removeAll(name);
    m_items[category].remove(name);
    saveJson();
    bump();
}

void TilesetStore::addItem(const QString &category, const QString &name, int serverId)
{
    if (name.isEmpty()) return;
    if (!m_names.value(category).contains(name)) m_names[category].append(name);
    QVariantList &list = m_items[category][name];
    if (!list.contains(serverId)) list.append(serverId);
    saveJson();
    bump();
}

void TilesetStore::removeItem(const QString &category, const QString &name, int serverId)
{
    QVariantList &list = m_items[category][name];
    list.removeAll(QVariant(serverId));
    saveJson();
    bump();
}
