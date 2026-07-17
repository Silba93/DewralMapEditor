#include "creaturestore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

#include <algorithm>

CreatureStore::CreatureStore(QObject *parent)
    : QAbstractListModel(parent)
{
}

bool CreatureStore::loadForVersion(int version)
{
    beginResetModel();
    m_creatures.clear();
    const QString path = QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("data/%1/creatures.xml").arg(version));
    const bool ok = QFile::exists(path) && loadFile(path);
    std::sort(m_creatures.begin(), m_creatures.end(),
              [](const CreatureType &a, const CreatureType &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    endResetModel();
    emit countChanged();
    return ok;
}

bool CreatureStore::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QXmlStreamReader xml(&f);
    // Kontekst sekcji <monsters>/<npcs> (format RME dopuszcza tez plaskie wpisy
    // <creature type="npc"> i gole <monster>/<npc> - patrz naglowek).
    bool inNpcsSection = false;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        const auto tag = xml.name();
        if (token == QXmlStreamReader::StartElement) {
            if (tag == QLatin1String("npcs")) { inNpcsSection = true; continue; }
            if (tag == QLatin1String("monsters")) { inNpcsSection = false; continue; }

            const bool isCreature = tag == QLatin1String("creature")
                                    || tag == QLatin1String("monster")
                                    || tag == QLatin1String("npc");
            if (!isCreature) continue;

            const auto a = xml.attributes();
            CreatureType c;
            c.name = a.value(QLatin1String("name")).toString();
            if (c.name.isEmpty()) continue;
            c.isNpc = inNpcsSection || tag == QLatin1String("npc")
                      || a.value(QLatin1String("type")) == QLatin1String("npc");
            c.lookType = a.value(QLatin1String("looktype")).toInt();
            c.lookItem = a.value(QLatin1String("lookitem")).toInt();
            c.lookHead = a.value(QLatin1String("lookhead")).toInt();
            c.lookBody = a.value(QLatin1String("lookbody")).toInt();
            c.lookLegs = a.value(QLatin1String("looklegs")).toInt();
            c.lookFeet = a.value(QLatin1String("lookfeet")).toInt();
            m_creatures.push_back(std::move(c));
        } else if (token == QXmlStreamReader::EndElement) {
            if (tag == QLatin1String("npcs")) inNpcsSection = false;
        }
    }
    return !xml.hasError();
}

const CreatureStore::CreatureType *CreatureStore::byName(const QString &name) const
{
    for (const CreatureType &c : m_creatures)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return &c;
    return nullptr;
}

int CreatureStore::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_creatures.size());
}

QVariant CreatureStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_creatures.size())
        return {};
    const CreatureType &c = m_creatures[index.row()];
    switch (role) {
    case NameRole:     return c.name;
    case IsNpcRole:    return c.isNpc;
    case LookTypeRole: return c.lookType;
    case LookItemRole: return c.lookItem;
    default:           return {};
    }
}

QHash<int, QByteArray> CreatureStore::roleNames() const
{
    return {
        { NameRole,     QByteArrayLiteral("name") },
        { IsNpcRole,    QByteArrayLiteral("isNpc") },
        { LookTypeRole, QByteArrayLiteral("lookType") },
        { LookItemRole, QByteArrayLiteral("lookItem") },
    };
}
