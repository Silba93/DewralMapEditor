#include "otbmreader.h"

#include "nodefilereader.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <utility>

namespace {

// Zapis drzewa node'ow OTBM. Markery 0xFE/0xFF surowe; bajty danych z escapem
// 0xFD (gdy rowne 0xFD/0xFE/0xFF) - odwrotnosc NodeFileReader.
struct NodeWriter {
    QByteArray buf;
    void raw(uint8_t b) { buf.append(static_cast<char>(b)); }
    void data(uint8_t b) {
        if (b == 0xFD || b == 0xFE || b == 0xFF) buf.append(static_cast<char>(0xFD));
        buf.append(static_cast<char>(b));
    }
    void u16(uint16_t v) { data(v & 0xFF); data((v >> 8) & 0xFF); }
    void u32(uint32_t v) { data(v & 0xFF); data((v >> 8) & 0xFF); data((v >> 16) & 0xFF); data((v >> 24) & 0xFF); }
    void str(const QString &s) {
        const QByteArray b = s.toLatin1();
        u16(static_cast<uint16_t>(b.size()));
        for (char c : b) data(static_cast<uint8_t>(c));
    }
    void start(uint8_t type) { raw(0xFE); data(type); }
    void end() { raw(0xFF); }
};

void writeMapItem(NodeWriter &w, const OtbmMapItem &item)
{
    w.start(static_cast<uint8_t>(OtbmNode::Item));
    w.u16(item.server_id);
    if (item.count > 1) {
        w.data(static_cast<uint8_t>(OtbmAttribute::Count));
        w.data(static_cast<uint8_t>(item.count > 255 ? 255 : item.count));
    }
    if (item.action_id) { w.data(static_cast<uint8_t>(OtbmAttribute::ActionId)); w.u16(static_cast<uint16_t>(item.action_id)); }
    if (item.unique_id) { w.data(static_cast<uint8_t>(OtbmAttribute::UniqueId)); w.u16(static_cast<uint16_t>(item.unique_id)); }
    if (item.depot_id)  { w.data(static_cast<uint8_t>(OtbmAttribute::DepotId));  w.u16(item.depot_id); }
    // Odtwarzane 1:1 przy zapisie (patrz komentarz przy OtbmItemExtra) - bez tego
    // kazdy load+save w DME kasowal teleporty/znaki/drzwi domow/tier/podia.
    // extra == null dla wiekszosci itemow -> zero pracy w typowym przypadku.
    if (item.extra) {
        const OtbmItemExtra &e = *item.extra;
        if (!e.text.isEmpty()) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Text));
            w.str(e.text);
        }
        if (!e.description.isEmpty()) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Desc));
            w.str(e.description);
        }
        if (e.has_teleport) {
            w.data(static_cast<uint8_t>(OtbmAttribute::TeleportDest));
            w.u16(e.tele_x);
            w.u16(e.tele_y);
            w.data(e.tele_z);
        }
        if (e.door_id) {
            w.data(static_cast<uint8_t>(OtbmAttribute::HouseDoorId));
            w.data(e.door_id);
        }
        if (e.tier) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Tier));
            w.data(e.tier);
        }
        if (e.podium_raw.size() == 15) {
            w.data(static_cast<uint8_t>(OtbmAttribute::PodiumOutfit));
            for (char b : e.podium_raw) {
                w.data(static_cast<uint8_t>(b));
            }
        }
    }
    for (const OtbmMapItem &child : item.children) {
        writeMapItem(w, child);
    }
    w.end();
}

} // namespace

namespace {

// Atrybuty itemu, ktore potrafimy bezpiecznie przeczytac z poziomu wezla.
// Obejmuje juz wszystkie atrybuty o STALEJ dlugosci, jakie realnie wystepuja
// na mapach RME/TFS (Count/Charges/ActionId/UniqueId/DepotId/Text/Desc/
// TeleportDest/HouseDoorId/Tier/PodiumOutfit) - te sa odczytywane I zapisywane
// z powrotem (patrz writeMapItem), wiec load+save jest dla nich bezstratny.
// Przy NAPRAWDE nieznanym atrybucie (np. OTBM_ATTR_ATTRIBUTE_MAP z OTBM v4)
// przerywamy parsowanie dalszych atrybutow tego itemu - zawartosc kontenerow
// i tak siedzi w dzieciach wezla, wiec ona sie nie gubi; ginie tylko ten
// pojedynczy, nierozpoznany atrybut.
bool readItemAttribute(BinaryNode &node, OtbmAttribute attr, OtbmMapItem &item)
{
    switch (attr) {
    case OtbmAttribute::Count:
    case OtbmAttribute::RuneCharges: {
        uint8_t value = 0;
        if (!node.getU8(value)) return false;
        item.count = value;
        return true;
    }
    case OtbmAttribute::Charges: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.count = value;
        return true;
    }
    case OtbmAttribute::ActionId: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.action_id = value;
        return true;
    }
    case OtbmAttribute::UniqueId: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.unique_id = value;
        return true;
    }
    case OtbmAttribute::DepotId: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.depot_id = value;
        return true;
    }
    // Ponizsze byly kiedys "nieznane" i przerywaly parsowanie atrybutow itemu -
    // co przy zapisie kasowalo je BEZPOWROTNIE (teleporty/znaki/drzwi domow).
    // Uklad bajtow 1:1 z RME (OTAcademy/RME, iomap_otbm.cpp).
    case OtbmAttribute::Text: {
        QString value;
        if (!node.getString(value)) return false;
        item.ensureExtra().text = value;
        return true;
    }
    case OtbmAttribute::Desc: {
        QString value;
        if (!node.getString(value)) return false;
        item.ensureExtra().description = value;
        return true;
    }
    case OtbmAttribute::TeleportDest: {
        uint16_t x = 0, y = 0;
        uint8_t z = 0;
        if (!node.getU16(x) || !node.getU16(y) || !node.getU8(z)) return false;
        OtbmItemExtra &e = item.ensureExtra();
        e.has_teleport = true;
        e.tele_x = x;
        e.tele_y = y;
        e.tele_z = z;
        return true;
    }
    case OtbmAttribute::HouseDoorId: {
        uint8_t value = 0;
        if (!node.getU8(value)) return false;
        item.ensureExtra().door_id = value;
        return true;
    }
    case OtbmAttribute::Tier: {
        uint8_t value = 0;
        if (!node.getU8(value)) return false;
        item.ensureExtra().tier = value;
        return true;
    }
    case OtbmAttribute::PodiumOutfit: {
        // 15 surowych bajtow (flagi, kierunek, outfit, mount) - patrz komentarz
        // przy OtbmItemExtra::podium_raw. Trzymane 1:1, bez interpretacji.
        QByteArray raw;
        if (!node.readBytes(15, raw)) return false;
        item.ensureExtra().podium_raw = raw;
        return true;
    }
    default:
        // Naprawde nieznana/zmiennej dlugosci (np. OTBM_ATTR_ATTRIBUTE_MAP z
        // OTBM v4, ktorego DME nie zapisuje) - nie ryzykujemy zlego offsetu.
        return false;
    }
}

QVariantMap itemToVariant(const OtbmMapItem &item)
{
    QVariantMap map;
    map.insert(QStringLiteral("serverId"), item.server_id);
    map.insert(QStringLiteral("count"), item.count);
    map.insert(QStringLiteral("isGround"), item.is_ground);
    if (item.action_id) map.insert(QStringLiteral("actionId"), item.action_id);
    if (item.unique_id) map.insert(QStringLiteral("uniqueId"), item.unique_id);
    if (item.depot_id) map.insert(QStringLiteral("depotId"), item.depot_id);
    // Wystawione dla przyszlego UI (np. Item Properties) - dane sa teraz
    // zachowywane przy zapisie (patrz OtbmItemExtra), wiec warto je juz udostepnic.
    if (item.extra) {
        const OtbmItemExtra &e = *item.extra;
        if (!e.text.isEmpty()) map.insert(QStringLiteral("text"), e.text);
        if (!e.description.isEmpty()) map.insert(QStringLiteral("description"), e.description);
        if (e.has_teleport) {
            map.insert(QStringLiteral("teleportX"), e.tele_x);
            map.insert(QStringLiteral("teleportY"), e.tele_y);
            map.insert(QStringLiteral("teleportZ"), e.tele_z);
        }
        if (e.door_id) map.insert(QStringLiteral("doorId"), e.door_id);
        if (e.tier) map.insert(QStringLiteral("tier"), e.tier);
    }
    return map;
}

} // namespace

OtbmReader::OtbmReader(QObject *parent)
    : QObject(parent)
{
    // Kazda realna zmiana mapy emituje mapChanged - dokument sam sledzi swoja flage
    // niezapisanych zmian, zamiast globalnego "dirty" w QML (system kart map).
    connect(this, &OtbmReader::mapChanged, this, [this] { setDirty(true); });
}

void OtbmReader::setDirty(bool d)
{
    if (m_dirty == d) return;
    m_dirty = d;
    emit dirtyChanged();
}

OtbmReader::~OtbmReader() = default;

void OtbmReader::reset()
{
    m_tiles.clear();
    m_posIndex.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_currentGroup = UndoAction{};
    m_groupRecorded.clear();
    m_undoGrouping = false;
    m_towns.clear();
    m_waypoints.clear();
    m_houses.clear();
    m_otbmVersion = 0;
    m_width = 0;
    m_height = 0;
    m_otbItemsMajor = 0;
    m_otbItemsMinor = 0;
    m_description.clear();
    m_spawnFile.clear();
    m_houseFile.clear();
    m_itemCount = 0;
    m_loaded = false;
    m_errorString.clear();
    m_filePath.clear();   // porazka load nie moze zostawic sciezki po starej mapie
    setDirty(false);

    emit loadedChanged();
    emit errorChanged();
    emit filePathChanged();
}

void OtbmReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool OtbmReader::loadFile(const QString &path)
{
    reset();

    NodeFileReader file;
    // OTBM zaczyna sie zwykle od 4 bajtow 0x00 (wersja), czasem od "OTBM".
    if (!file.loadFile(path, {QByteArrayLiteral("OTBM"), QByteArray(4, '\0')})) {
        setError(file.errorString());
        emit loadedChanged();
        return false;
    }

    BinaryNode &root = file.rootNode();
    if (!parseRootHeader(root)) {
        return false;
    }

    if (root.children().isEmpty()) {
        setError(QStringLiteral("Brak wezla MapData w pliku OTBM"));
        emit loadedChanged();
        return false;
    }

    // Pierwsze (i jedyne) dziecko korzenia to MapData.
    BinaryNode mapData = root.children().first();
    if (!parseMapData(mapData)) {
        return false;
    }

    rebuildPosIndex();

    // Spawny/domy z zewnetrznych XML (ExtSpawnFile/ExtHouseFile) - dokladane PRZED
    // loadedChanged, zeby indeksy MapView od razu widzialy kafle spawnow.
    loadSpawnsXml(path);
    loadHousesXml(path);

    m_loaded = true;
    m_filePath = path;
    emit filePathChanged();
    setDirty(false);   // swiezo wczytana mapa = brak niezapisanych zmian
    emit loadedChanged();
    return true;
}

void OtbmReader::applyClientVersions(int clientVersion, int otbMajor, int otbMinor)
{
    Q_UNUSED(clientVersion);
    // Surowa wersja w naglowku = 2, jak zapisuje RME (dowod: world.otbm od nekiro
    // 7.72 zapisany RME 3.5 ma raw u32 = 2). Nasz writer emituje wspolczesny format
    // wezlow (Count jako atrybut itemu), wiec oznaczanie pliku nizsza wersja
    // (OTBM1 ma INNE kodowanie itemow) myliloby loader TFS/RME.
    m_otbmVersion = 2;
    // Wersje items wg AKTUALNIE zaladowanego items.otb - nie przepisane z wczytanej
    // mapy (mogla byc zapisana starszym klientem/OTB). Jak RME.
    if (otbMajor > 0) m_otbItemsMajor = static_cast<uint32_t>(otbMajor);
    if (otbMinor > 0) m_otbItemsMinor = static_cast<uint32_t>(otbMinor);
}

bool OtbmReader::newMap(int width, int height, int clientVersion,
                        int otbMajor, int otbMinor)
{
    if (width <= 0 || height <= 0) return false;

    reset();
    m_width = static_cast<uint16_t>(std::clamp(width, 256, 65535));
    m_height = static_cast<uint16_t>(std::clamp(height, 256, 65535));
    applyClientVersions(clientVersion, otbMajor, otbMinor);
    m_description = QStringLiteral("Created with Dewral Map Editor");

    m_loaded = true;
    emit loadedChanged();
    return true;
}

bool OtbmReader::parseRootHeader(BinaryNode &root)
{
    root.skip(1); // bajt typu wezla (RootHeader)

    if (!root.getU32(m_otbmVersion)
        || !root.getU16(m_width)
        || !root.getU16(m_height)
        || !root.getU32(m_otbItemsMajor)
        || !root.getU32(m_otbItemsMinor)) {
        setError(QStringLiteral("Uszkodzony naglowek OTBM"));
        emit loadedChanged();
        return false;
    }

    // Wersje 1-4 (0..3). Nowsze tylko ostrzegamy, ale probujemy czytac dalej.
    if (m_otbmVersion > static_cast<uint32_t>(OtbmVersion::V4)) {
        // brak twardego bledu - format wezlow jest zgodny
    }
    return true;
}

bool OtbmReader::parseMapData(BinaryNode &mapData)
{
    mapData.skip(1); // bajt typu wezla (MapData)

    // Atrybuty mapy: description, spawn file, house file...
    while (mapData.bytesRemaining() > 0) {
        uint8_t attrType = 0;
        if (!mapData.getU8(attrType)) {
            break;
        }

        QString value;
        switch (static_cast<OtbmAttribute>(attrType)) {
        case OtbmAttribute::Description:
            if (!mapData.getString(value)) return true;
            if (!m_description.isEmpty()) m_description.append(QLatin1Char('\n'));
            m_description.append(value);
            break;
        case OtbmAttribute::ExtSpawnFile:
            if (!mapData.getString(m_spawnFile)) return true;
            break;
        case OtbmAttribute::ExtHouseFile:
            if (!mapData.getString(m_houseFile)) return true;
            break;
        case OtbmAttribute::ExtSpawnNpcFile:
            if (!mapData.getString(value)) return true;
            break;
        default:
            // Nieznany atrybut na poziomie mapy - przerywamy petle atrybutow.
            mapData.skip(mapData.bytesRemaining());
            break;
        }
    }

    // Dzieci MapData: TileArea / Towns / Waypoints.
    for (const BinaryNode &sourceChild : mapData.children()) {
        BinaryNode child = sourceChild;
        uint8_t nodeType = 0;
        if (!child.getU8(nodeType)) {
            continue;
        }

        switch (static_cast<OtbmNode>(nodeType)) {
        case OtbmNode::TileArea:
            parseTileArea(child);
            break;
        case OtbmNode::Towns:
            parseTowns(child);
            break;
        case OtbmNode::Waypoints:
            parseWaypoints(child);
            break;
        default:
            break;
        }
    }

    return true;
}

void OtbmReader::parseTileArea(BinaryNode &area)
{
    // bajt typu juz zostal pobrany przez getU8 w parseMapData.
    uint16_t baseX = 0;
    uint16_t baseY = 0;
    uint8_t baseZ = 0;
    if (!area.getU16(baseX) || !area.getU16(baseY) || !area.getU8(baseZ)) {
        return;
    }

    for (const BinaryNode &sourceTile : area.children()) {
        BinaryNode tile = sourceTile;
        parseTile(tile, baseX, baseY, baseZ);
    }
}

void OtbmReader::parseTile(BinaryNode &tile, uint16_t baseX, uint16_t baseY, uint8_t baseZ)
{
    uint8_t nodeType = 0;
    if (!tile.getU8(nodeType)) {
        return;
    }

    const bool isHouse = static_cast<OtbmNode>(nodeType) == OtbmNode::HouseTile;
    if (!isHouse && static_cast<OtbmNode>(nodeType) != OtbmNode::Tile) {
        return;
    }

    uint8_t dx = 0;
    uint8_t dy = 0;
    if (!tile.getU8(dx) || !tile.getU8(dy)) {
        return;
    }

    OtbmTile result;
    result.x = static_cast<uint16_t>(baseX + dx);
    result.y = static_cast<uint16_t>(baseY + dy);
    result.z = baseZ;
    result.is_house = isHouse;

    if (isHouse) {
        tile.getU32(result.house_id);
    }

    // Atrybuty plytki: TileFlags oraz Item (ground w formie kompaktowej).
    while (tile.bytesRemaining() > 0) {
        uint8_t attrType = 0;
        if (!tile.getU8(attrType)) {
            break;
        }

        if (static_cast<OtbmAttribute>(attrType) == OtbmAttribute::TileFlags) {
            tile.getU32(result.flags);
        } else if (static_cast<OtbmAttribute>(attrType) == OtbmAttribute::Item) {
            uint16_t serverId = 0;
            if (!tile.getU16(serverId)) {
                break;
            }
            OtbmMapItem ground;
            ground.server_id = serverId;
            ground.is_ground = true;
            m_itemCount += countItems(ground);
            result.items.push_back(std::move(ground));
        } else {
            // Nieznany atrybut plytki - nie znamy dlugosci, przerywamy.
            break;
        }
    }

    // Dzieci plytki to pelne wezly Item (np. stosy, kontenery).
    for (const BinaryNode &sourceItem : tile.children()) {
        BinaryNode itemNode = sourceItem;
        uint8_t itemType = 0;
        if (!itemNode.getU8(itemType)
            || static_cast<OtbmNode>(itemType) != OtbmNode::Item) {
            continue;
        }
        OtbmMapItem item = parseItem(itemNode);
        if (item.server_id == 0) {
            continue;
        }
        m_itemCount += countItems(item);
        result.items.push_back(std::move(item));
    }

    m_tiles.push_back(std::move(result));
}

OtbmMapItem OtbmReader::parseItem(BinaryNode &itemNode)
{
    // bajt typu (Item) juz pobrany przez wolajacego.
    OtbmMapItem item;
    if (!itemNode.getU16(item.server_id)) {
        item.server_id = 0;
        return item;
    }

    // Atrybuty itemu - czytamy znane, przy nieznanym przerywamy.
    while (itemNode.bytesRemaining() > 0) {
        uint8_t attrType = 0;
        if (!itemNode.getU8(attrType)) {
            break;
        }
        if (!readItemAttribute(itemNode, static_cast<OtbmAttribute>(attrType), item)) {
            break;
        }
    }

    // Zawartosc kontenera = dzieci wezla Item.
    for (const BinaryNode &sourceChild : itemNode.children()) {
        BinaryNode childNode = sourceChild;
        uint8_t childType = 0;
        if (!childNode.getU8(childType)
            || static_cast<OtbmNode>(childType) != OtbmNode::Item) {
            continue;
        }
        OtbmMapItem child = parseItem(childNode);
        if (child.server_id != 0) {
            item.children.push_back(std::move(child));
        }
    }

    return item;
}

void OtbmReader::parseTowns(BinaryNode &townsNode)
{
    for (const BinaryNode &sourceTown : townsNode.children()) {
        BinaryNode townNode = sourceTown;
        uint8_t nodeType = 0;
        if (!townNode.getU8(nodeType)
            || static_cast<OtbmNode>(nodeType) != OtbmNode::Town) {
            continue;
        }

        OtbmTown town;
        if (!townNode.getU32(town.id) || !townNode.getString(town.name)) {
            continue;
        }
        townNode.getU16(town.temple_x);
        townNode.getU16(town.temple_y);
        townNode.getU8(town.temple_z);
        m_towns.push_back(std::move(town));
    }
}

void OtbmReader::parseWaypoints(BinaryNode &waypointsNode)
{
    for (const BinaryNode &sourceWaypoint : waypointsNode.children()) {
        BinaryNode wpNode = sourceWaypoint;
        uint8_t nodeType = 0;
        if (!wpNode.getU8(nodeType)
            || static_cast<OtbmNode>(nodeType) != OtbmNode::Waypoint) {
            continue;
        }

        OtbmWaypoint wp;
        if (!wpNode.getString(wp.name)) {
            continue;
        }
        wpNode.getU16(wp.x);
        wpNode.getU16(wp.y);
        wpNode.getU8(wp.z);
        m_waypoints.push_back(std::move(wp));
    }
}

int OtbmReader::countItems(const OtbmMapItem &item) const
{
    int total = 1;
    for (const OtbmMapItem &child : item.children) {
        total += countItems(child);
    }
    return total;
}

void OtbmReader::rebuildPosIndex()
{
    m_posIndex.clear();
    m_posIndex.reserve(static_cast<int>(m_tiles.size()));
    for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i) {
        const OtbmTile &t = m_tiles[static_cast<size_t>(i)];
        m_posIndex.insert(posKey3d(t.x, t.y, t.z), i);
    }
}

const OtbmTile *OtbmReader::tileAt(int x, int y, int z) const
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return nullptr;
    return &m_tiles[static_cast<size_t>(it.value())];
}

bool OtbmReader::addItem(int x, int y, int z, uint16_t serverId)
{
    if (serverId == 0 || x < 0 || y < 0 || z < 0 || z > 15) {
        return false;
    }

    OtbmMapItem item;
    item.server_id = serverId;

    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it != m_posIndex.end()) {
        OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
        item.is_ground = tile.items.empty();
        tile.items.push_back(item);
    } else {
        // Nowy kafelek - push_back moze przealokowac m_tiles (patrz naglowek).
        OtbmTile tile;
        tile.x = static_cast<uint16_t>(x);
        tile.y = static_cast<uint16_t>(y);
        tile.z = static_cast<uint8_t>(z);
        item.is_ground = true;
        tile.items.push_back(item);
        m_posIndex.insert(posKey3d(x, y, z), static_cast<int>(m_tiles.size()));
        m_tiles.push_back(std::move(tile));
    }

    m_itemCount += 1;
    emit mapChanged();
    return true;
}

bool OtbmReader::placeItem(int x, int y, int z, uint16_t serverId,
                           int index, bool replace, bool isGround)
{
    OtbmMapItem item;
    item.server_id = serverId;
    return placeItem(x, y, z, item, index, replace, isGround);
}

bool OtbmReader::placeItem(int x, int y, int z, const OtbmMapItem &src,
                           int index, bool replace, bool isGround)
{
    if (src.server_id == 0 || x < 0 || y < 0 || z < 0 || z > 15) {
        return false;
    }

    recordTile(x, y, z); // undo: stan przed zmiana

    OtbmMapItem item = src;      // kopia: count/action_id/unique_id/zawartosc kontenera
    item.is_ground = isGround;
    // Item moze byc kontenerem z zawartoscia - m_itemCount liczy kazdy wezel z osobna.
    const int nodes = countItems(item);

    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it != m_posIndex.end()) {
        OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
        if (replace && index >= 0 && index < static_cast<int>(tile.items.size())) {
            m_itemCount += nodes - countItems(tile.items[static_cast<size_t>(index)]);
            tile.items[static_cast<size_t>(index)] = std::move(item);
        } else {
            const int at = std::clamp(index, 0, static_cast<int>(tile.items.size()));
            tile.items.insert(tile.items.begin() + at, std::move(item));
            m_itemCount += nodes;
        }
    } else {
        // Nowy kafelek - push_back moze przealokowac m_tiles.
        OtbmTile tile;
        tile.x = static_cast<uint16_t>(x);
        tile.y = static_cast<uint16_t>(y);
        tile.z = static_cast<uint8_t>(z);
        tile.items.push_back(std::move(item));
        m_posIndex.insert(posKey3d(x, y, z), static_cast<int>(m_tiles.size()));
        m_tiles.push_back(std::move(tile));
        m_itemCount += nodes;
    }

    // W trybie grupowym (malowanie/multi-edycja) undoCount i tak nie zmienia sie
    // az do endUndoGroup (dopiero tam trafia na stos) - emit tutaj bylby fikcyjny
    // (zero realnej zmiany), a przy duzym pedzlu = setki zbednych sygnalow ->
    // QML re-evaluuje bindingi (np. Undo.enabled) i FPS drastycznie spada.
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

// Itemy nie leza wylacznie na wierzchu kafla - OTBM trzyma zawartosc pojemnikow w
// OtbmMapItem::children (torba w skrzyni w depocie, rekurencyjnie). Find/Replace/Remove
// musi schodzic w glab, inaczej count jest zanizony o wszystko, co siedzi w kontenerach.
namespace {

int countMatches(const OtbmMapItem &item, uint16_t sid)
{
    int n = (item.server_id == sid) ? 1 : 0;
    for (const OtbmMapItem &child : item.children) n += countMatches(child, sid);
    return n;
}

int countMatches(const std::vector<OtbmMapItem> &items, uint16_t sid)
{
    int n = 0;
    for (const OtbmMapItem &item : items) n += countMatches(item, sid);
    return n;
}

bool hasMatch(const std::vector<OtbmMapItem> &items, uint16_t sid)
{
    for (const OtbmMapItem &item : items) {
        if (item.server_id == sid) return true;
        if (hasMatch(item.children, sid)) return true;
    }
    return false;
}

int replaceMatches(std::vector<OtbmMapItem> &items, uint16_t fromId, uint16_t toId)
{
    int n = 0;
    for (OtbmMapItem &item : items) {
        // is_ground zostaje - podmiana w miejscu nie zmienia roli itemu w stosie.
        if (item.server_id == fromId) { item.server_id = toId; ++n; }
        n += replaceMatches(item.children, fromId, toId);
    }
    return n;
}

// Liczba wezlow w poddrzewie (item + cala zawartosc). Odpowiednik OtbmReader::countItems,
// powtorzony tu, bo helpery sa wolnymi funkcjami.
int countNodes(const OtbmMapItem &item)
{
    int total = 1;
    for (const OtbmMapItem &child : item.children) total += countNodes(child);
    return total;
}

// Zwraca liczbe usunietych dopasowan. Do removedNodes dolicza CALE poddrzewa: kasujac
// kontener kasujemy tez jego zawartosc, a m_itemCount liczy kazdy wezel z osobna.
int removeMatches(std::vector<OtbmMapItem> &items, const std::vector<uint16_t> &ids,
                  int &removedNodes)
{
    int n = 0;
    for (auto it = items.begin(); it != items.end(); ) {
        if (std::find(ids.begin(), ids.end(), it->server_id) != ids.end()) {
            ++n;
            removedNodes += countNodes(*it);
            it = items.erase(it);   // zawartosc kontenera znika razem z nim
        } else {
            n += removeMatches(it->children, ids, removedNodes);
            ++it;
        }
    }
    return n;
}

} // namespace

int OtbmReader::replaceItemsById(int x, int y, int z, uint16_t fromId, uint16_t toId)
{
    if (fromId == 0 || toId == 0 || fromId == toId) return 0;
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return 0;
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];

    if (!hasMatch(tile.items, fromId)) return 0;   // nic do podmiany - nie smiec undo

    recordTile(x, y, z); // undo: stan przed zmiana
    const int n = replaceMatches(tile.items, fromId, toId);
    if (!m_undoGrouping) emit mapChanged();   // patrz komentarz w placeItem()
    return n;
}

// Istniejacy kafel albo swiezo utworzony pusty - BEZ snapshotu undo (uzywane przy
// wczytywaniu spawns.xml, gdzie undo nie ma prawa dostac wpisow).
OtbmTile *OtbmReader::getOrCreateTileRaw(int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 || z > 15) return nullptr;
    const quint64 key = posKey3d(x, y, z);
    auto it = m_posIndex.find(key);
    if (it != m_posIndex.end()) return &m_tiles[static_cast<size_t>(it.value())];

    OtbmTile tile;
    tile.x = static_cast<uint16_t>(x);
    tile.y = static_cast<uint16_t>(y);
    tile.z = static_cast<uint8_t>(z);
    m_posIndex.insert(key, static_cast<int>(m_tiles.size()));
    m_tiles.push_back(std::move(tile));   // deque - wskazniki pozostaja wazne
    return &m_tiles.back();
}

// Kafel pod spawn/potwora - istniejacy albo swiezo utworzony (spawn moze stac na
// pustym terenie; snapshot undo przed utworzeniem zapisuje "brak kafla").
OtbmTile *OtbmReader::tileForSpawnEdit(int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 || z > 15) return nullptr;
    recordTile(x, y, z);   // undo: stan przed zmiana (takze "nie istnial")
    return getOrCreateTileRaw(x, y, z);
}

void OtbmReader::loadSpawnsXml(const QString &mapPath)
{
    if (m_spawnFile.isEmpty()) return;
    const QString path = QFileInfo(mapPath).dir().filePath(m_spawnFile);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;   // brak pliku = brak spawnow (nie blad)

    QXmlStreamReader xml(&f);
    int cx = 0, cy = 0, cz = 0;
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement) continue;
        const auto tag = xml.name();
        if (tag == QLatin1String("spawn")) {
            const auto a = xml.attributes();
            cx = a.value(QLatin1String("centerx")).toInt();
            cy = a.value(QLatin1String("centery")).toInt();
            cz = a.value(QLatin1String("centerz")).toInt();
            int radius = a.value(QLatin1String("radius")).toInt();
            if (radius < 1) radius = 1;
            if (OtbmTile *t = getOrCreateTileRaw(cx, cy, cz))
                t->spawn_radius = radius;
        } else if (tag == QLatin1String("monster") || tag == QLatin1String("npc")) {
            const auto a = xml.attributes();
            // x/y w XML to OFFSETY od centrum spawnu (format RME/TFS).
            const int x = cx + a.value(QLatin1String("x")).toInt();
            const int y = cy + a.value(QLatin1String("y")).toInt();
            const int st = a.value(QLatin1String("spawntime")).toInt();
            if (OtbmTile *t = getOrCreateTileRaw(x, y, cz)) {
                t->creature_name = a.value(QLatin1String("name")).toString();
                t->creature_spawntime = st > 0 ? st : 60;
                t->creature_is_npc = (tag == QLatin1String("npc"));
            }
        }
    }
}

bool OtbmReader::saveSpawnsXml(const QString &mapPath)
{
    // Czy w ogole cos jest? Bez spawnow nie tworzymy pliku (i nie ruszamy naglowka).
    bool any = false;
    for (const OtbmTile &t : m_tiles)
        if (t.spawn_radius > 0 || !t.creature_name.isEmpty()) { any = true; break; }
    if (!any) return true;

    // Nowa mapa bez wpisu ExtSpawnFile: przyjmij konwencje RME "<mapa>-spawn.xml".
    if (m_spawnFile.isEmpty())
        m_spawnFile = QFileInfo(mapPath).completeBaseName() + QStringLiteral("-spawn.xml");

    const QString path = QFileInfo(mapPath).dir().filePath(m_spawnFile);
    // QSaveFile jak w saveFile: awaria w trakcie nie niszczy istniejacego pliku.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("Nie mozna zapisac spawnow: %1").arg(path));
        return false;
    }

    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(-1);   // taby jak RME
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("spawns"));

    for (const OtbmTile &c : m_tiles) {
        if (c.spawn_radius <= 0) continue;
        xml.writeStartElement(QStringLiteral("spawn"));
        xml.writeAttribute(QStringLiteral("centerx"), QString::number(c.x));
        xml.writeAttribute(QStringLiteral("centery"), QString::number(c.y));
        xml.writeAttribute(QStringLiteral("centerz"), QString::number(c.z));
        xml.writeAttribute(QStringLiteral("radius"), QString::number(c.spawn_radius));

        // Potwory w promieniu centrum - offsety wzgledem niego (format RME/TFS).
        // Lookup po m_posIndex per kafel: promien jest maly (zwykle 1-10).
        for (int dy = -c.spawn_radius; dy <= c.spawn_radius; ++dy)
            for (int dx = -c.spawn_radius; dx <= c.spawn_radius; ++dx) {
                auto it = m_posIndex.find(posKey3d(c.x + dx, c.y + dy, c.z));
                if (it == m_posIndex.end()) continue;
                const OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
                if (t.creature_name.isEmpty()) continue;
                xml.writeStartElement(t.creature_is_npc ? QStringLiteral("npc")
                                                        : QStringLiteral("monster"));
                xml.writeAttribute(QStringLiteral("name"), t.creature_name);
                xml.writeAttribute(QStringLiteral("x"), QString::number(dx));
                xml.writeAttribute(QStringLiteral("y"), QString::number(dy));
                xml.writeAttribute(QStringLiteral("z"), QString::number(c.z));
                xml.writeAttribute(QStringLiteral("spawntime"),
                                   QString::number(t.creature_spawntime));
                xml.writeEndElement();
            }
        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndDocument();
    if (!f.commit()) {
        setError(QStringLiteral("Blad zapisu spawnow: %1").arg(path));
        return false;
    }
    return true;
}

void OtbmReader::loadHousesXml(const QString &mapPath)
{
    if (m_houseFile.isEmpty()) return;
    const QString path = QFileInfo(mapPath).dir().filePath(m_houseFile);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;   // brak pliku = brak domow (nie blad)

    QXmlStreamReader xml(&f);
    while (!xml.atEnd()) {
        if (xml.readNext() != QXmlStreamReader::StartElement) continue;
        if (xml.name() != QLatin1String("house")) continue;
        const auto a = xml.attributes();
        OtbmHouse h;
        h.id = a.value(QLatin1String("houseid")).toUInt();
        h.name = a.value(QLatin1String("name")).toString();
        h.rent = a.value(QLatin1String("rent")).toInt();
        h.townId = a.value(QLatin1String("townid")).toInt();
        h.guildhall = a.value(QLatin1String("guildhall")).toInt() != 0;
        h.entryX = a.value(QLatin1String("entryx")).toInt();
        h.entryY = a.value(QLatin1String("entryy")).toInt();
        h.entryZ = a.value(QLatin1String("entryz")).toInt();
        if (h.id > 0) m_houses.push_back(std::move(h));
    }
}

bool OtbmReader::saveHousesXml(const QString &mapPath)
{
    if (m_houses.empty()) return true;   // bez domow nie tworzymy pliku

    if (m_houseFile.isEmpty())
        m_houseFile = QFileInfo(mapPath).completeBaseName() + QStringLiteral("-house.xml");

    // "size" (atrybut RME/TFS) = liczba kafli domu - policz raz dla wszystkich.
    QHash<uint32_t, int> sizes;
    for (const OtbmTile &t : m_tiles)
        if (t.is_house && t.house_id > 0) sizes[t.house_id]++;

    const QString path = QFileInfo(mapPath).dir().filePath(m_houseFile);
    // QSaveFile jak w saveFile: awaria w trakcie nie niszczy istniejacego pliku.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("Nie mozna zapisac domow: %1").arg(path));
        return false;
    }

    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(-1);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("houses"));
    for (const OtbmHouse &h : m_houses) {
        xml.writeStartElement(QStringLiteral("house"));
        xml.writeAttribute(QStringLiteral("name"), h.name);
        xml.writeAttribute(QStringLiteral("houseid"), QString::number(h.id));
        xml.writeAttribute(QStringLiteral("entryx"), QString::number(h.entryX));
        xml.writeAttribute(QStringLiteral("entryy"), QString::number(h.entryY));
        xml.writeAttribute(QStringLiteral("entryz"), QString::number(h.entryZ));
        xml.writeAttribute(QStringLiteral("rent"), QString::number(h.rent));
        if (h.guildhall) xml.writeAttribute(QStringLiteral("guildhall"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("townid"), QString::number(h.townId));
        xml.writeAttribute(QStringLiteral("size"), QString::number(sizes.value(h.id, 0)));
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    if (!f.commit()) {
        setError(QStringLiteral("Blad zapisu domow: %1").arg(path));
        return false;
    }
    return true;
}

OtbmHouse *OtbmReader::houseById(int id)
{
    for (OtbmHouse &h : m_houses)
        if (static_cast<int>(h.id) == id) return &h;
    return nullptr;
}

QVariantList OtbmReader::housesList() const
{
    QVariantList out;
    QHash<uint32_t, int> sizes;
    for (const OtbmTile &t : m_tiles)
        if (t.is_house && t.house_id > 0) sizes[t.house_id]++;
    for (const OtbmHouse &h : m_houses) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), h.id);
        m.insert(QStringLiteral("name"), h.name);
        m.insert(QStringLiteral("rent"), h.rent);
        m.insert(QStringLiteral("townId"), h.townId);
        m.insert(QStringLiteral("guildhall"), h.guildhall);
        m.insert(QStringLiteral("entryX"), h.entryX);
        m.insert(QStringLiteral("entryY"), h.entryY);
        m.insert(QStringLiteral("entryZ"), h.entryZ);
        m.insert(QStringLiteral("size"), sizes.value(h.id, 0));
        out.push_back(m);
    }
    return out;
}

int OtbmReader::addHouse(int townId)
{
    uint32_t maxId = 0;
    for (const OtbmHouse &h : m_houses) maxId = std::max(maxId, h.id);
    OtbmHouse h;
    h.id = maxId + 1;
    h.name = QStringLiteral("Unnamed House #%1").arg(h.id);
    h.townId = townId;
    m_houses.push_back(std::move(h));
    emit mapChanged();
    return static_cast<int>(maxId + 1);
}

void OtbmReader::setHouseTownId(int id, int townId)
{
    OtbmHouse *h = houseById(id);
    if (!h || h->townId == townId) return;
    h->townId = townId;
    emit mapChanged();
}

void OtbmReader::removeHouse(int id)
{
    auto it = std::find_if(m_houses.begin(), m_houses.end(),
                           [id](const OtbmHouse &h) { return static_cast<int>(h.id) == id; });
    if (it == m_houses.end()) return;
    m_houses.erase(it);
    // Kafle domu tracia przypisanie (jak RME przy kasowaniu domu) - jedno cofniecie.
    beginUndoGroup();
    for (OtbmTile &t : m_tiles)
        if (t.is_house && static_cast<int>(t.house_id) == id)
            clearHouseTileAt(t.x, t.y, t.z);
    endUndoGroup();
    emit mapChanged();
}

void OtbmReader::setHouseName(int id, const QString &name)
{
    OtbmHouse *h = houseById(id);
    if (!h || h->name == name || name.isEmpty()) return;
    h->name = name;
    emit mapChanged();
}

void OtbmReader::setHouseRent(int id, int rent)
{
    OtbmHouse *h = houseById(id);
    if (!h || h->rent == rent || rent < 0) return;
    h->rent = rent;
    emit mapChanged();
}

void OtbmReader::setHouseEntry(int id, int x, int y, int z)
{
    OtbmHouse *h = houseById(id);
    if (!h) return;
    if (h->entryX == x && h->entryY == y && h->entryZ == z) return;
    h->entryX = x; h->entryY = y; h->entryZ = z;
    emit mapChanged();
}

bool OtbmReader::setHouseTileAt(int x, int y, int z, uint32_t houseId)
{
    if (houseId == 0) return clearHouseTileAt(x, y, z);
    OtbmTile *t = tileForSpawnEdit(x, y, z);   // recordTile + kafel (moze powstac)
    if (!t) return false;
    if (t->is_house && t->house_id == houseId) return true;
    t->is_house = true;
    t->house_id = houseId;
    t->flags |= static_cast<uint32_t>(OtbmTileFlag::TileProtection);   // RME: dom = PZ
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::clearHouseTileAt(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
    if (!t.is_house) return false;
    recordTile(x, y, z);
    t.is_house = false;
    t.house_id = 0;
    t.flags &= ~static_cast<uint32_t>(OtbmTileFlag::TileProtection);   // RME: undraw zdejmuje PZ
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setSpawnAt(int x, int y, int z, int radius)
{
    if (radius <= 0) return clearSpawnAt(x, y, z);
    OtbmTile *t = tileForSpawnEdit(x, y, z);
    if (!t) return false;
    if (t->spawn_radius == radius) return true;   // bez zmian (snapshot juz zdjety - grupa go zdedupuje)
    t->spawn_radius = radius;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setCreatureAt(int x, int y, int z, const QString &name, int spawntime, bool isNpc)
{
    if (name.isEmpty()) return clearCreatureAt(x, y, z);
    OtbmTile *t = tileForSpawnEdit(x, y, z);
    if (!t) return false;
    t->creature_name = name;
    t->creature_spawntime = spawntime > 0 ? spawntime : 60;
    t->creature_is_npc = isNpc;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::clearSpawnAt(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
    if (t.spawn_radius == 0) return false;
    recordTile(x, y, z);
    t.spawn_radius = 0;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::clearCreatureAt(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
    if (t.creature_name.isEmpty()) return false;
    recordTile(x, y, z);
    t.creature_name.clear();
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setTopItemCount(int x, int y, int z, uint16_t count)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;
    }
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.items.empty()) {
        return false;
    }
    OtbmMapItem &top = tile.items.back();
    if (top.count == count) {
        return false;   // bez zmian - nie smiec undo
    }
    recordTile(x, y, z);   // undo: stan przed zmiana
    top.count = count;
    if (!m_undoGrouping) emit mapChanged();   // patrz komentarz w placeItem()
    return true;
}

int OtbmReader::countItemsOnTile(int x, int y, int z, int serverId) const
{
    if (serverId <= 0) return 0;
    const OtbmTile *t = tileAt(x, y, z);
    return t ? countMatches(t->items, static_cast<uint16_t>(serverId)) : 0;
}

int OtbmReader::countItemsOnMap(int serverId) const
{
    if (serverId <= 0) return 0;
    const uint16_t sid = static_cast<uint16_t>(serverId);
    int n = 0;
    for (const OtbmTile &t : m_tiles) n += countMatches(t.items, sid);
    return n;
}

QVariantMap OtbmReader::findFirstItemOnMap(int serverId) const
{
    QVariantMap out;
    if (serverId <= 0) return out;
    const uint16_t sid = static_cast<uint16_t>(serverId);
    for (const OtbmTile &t : m_tiles) {
        if (!hasMatch(t.items, sid)) continue;
        out.insert(QStringLiteral("x"), t.x);
        out.insert(QStringLiteral("y"), t.y);
        out.insert(QStringLiteral("z"), t.z);
        return out;
    }
    return out;
}

int OtbmReader::replaceItemsOnMap(uint16_t fromId, uint16_t toId)
{
    if (fromId == 0 || toId == 0 || fromId == toId) return 0;
    int n = 0;
    m_lastAffected.clear();
    beginUndoGroup();               // cala mapa = jedno cofniecie
    for (OtbmTile &t : m_tiles) {
        if (!hasMatch(t.items, fromId)) continue;
        recordTile(t.x, t.y, t.z);  // undo: stan przed zmiana
        n += replaceMatches(t.items, fromId, toId);
        m_lastAffected.push_back({ t.x, t.y, t.z });
    }
    endUndoGroup();
    if (n > 0) emit mapChanged();
    return n;
}

int OtbmReader::removeItemsOnMap(uint16_t serverId)
{
    if (serverId == 0) return 0;
    int n = 0;
    m_lastAffected.clear();
    const std::vector<uint16_t> ids{ serverId };
    beginUndoGroup();
    for (OtbmTile &t : m_tiles) {
        if (!hasMatch(t.items, serverId)) continue;
        recordTile(t.x, t.y, t.z);
        int removedNodes = 0;
        n += removeMatches(t.items, ids, removedNodes);
        m_itemCount -= removedNodes;
        m_lastAffected.push_back({ t.x, t.y, t.z });
    }
    endUndoGroup();
    if (n > 0) emit mapChanged();
    return n;
}

uint32_t OtbmReader::tileFlags(int x, int y, int z) const
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return 0;
    return m_tiles[static_cast<size_t>(it.value())].flags;
}

bool OtbmReader::setTileFlags(int x, int y, int z, uint32_t flags)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;   // strefy tylko na istniejacych kafelkach (jak RME)
    }
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.flags == flags) {
        return false;   // bez zmian - nie smiec undo
    }
    recordTile(x, y, z); // undo: stan przed zmiana
    tile.flags = flags;
    if (!m_undoGrouping) emit mapChanged();   // patrz komentarz w placeItem()
    return true;
}

bool OtbmReader::removeTopItem(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;
    }
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.items.empty()) {
        return false;
    }
    recordTile(x, y, z); // undo: stan przed usunieciem
    m_itemCount -= countItems(tile.items.back());
    tile.items.pop_back();
    if (!m_undoGrouping) emit mapChanged();   // patrz komentarz w placeItem()
    return true;
}

int OtbmReader::removeItemsById(int x, int y, int z, const std::vector<uint16_t> &ids, bool deep)
{
    if (ids.empty()) return 0;
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return 0;
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];

    // Najpierw sprawdz czy jest co usuwac - inaczej niepotrzebny snapshot undo.
    bool any = false;
    if (deep) {
        for (uint16_t id : ids) if (hasMatch(tile.items, id)) { any = true; break; }
    } else {
        for (const OtbmMapItem &item : tile.items)
            if (std::find(ids.begin(), ids.end(), item.server_id) != ids.end()) { any = true; break; }
    }
    if (!any) return 0;

    recordTile(x, y, z);   // undo: stan przed usunieciem
    int removed = 0;
    if (deep) {
        int removedNodes = 0;
        removed = removeMatches(tile.items, ids, removedNodes);
        m_itemCount -= removedNodes;
    } else {
        // Plytko: sciezka borderow. Border nigdy nie lezy w kontenerze, a recomputeBorders
        // wola to na kazdym kaflu - nie ma po co schodzic w zawartosc toreb.
        for (auto vit = tile.items.begin(); vit != tile.items.end(); ) {
            if (std::find(ids.begin(), ids.end(), vit->server_id) != ids.end()) {
                m_itemCount -= countItems(*vit);
                vit = tile.items.erase(vit);
                ++removed;
            } else {
                ++vit;
            }
        }
    }
    if (removed > 0 && !m_undoGrouping) emit mapChanged();
    return removed;
}

void OtbmReader::recordTile(int x, int y, int z)
{
    const quint64 key = posKey3d(x, y, z);

    // Dedup PRZED zbudowaniem snapshotu: snap.items to kopia calego stosu kafla, a
    // w grupie (pociagniecie/prostokat) ten sam kafel wraca wielokrotnie (ground +
    // kazdy border). Kopiowanie i wyrzucanie tego bylo czysta strata.
    if (m_undoGrouping && m_groupRecorded.contains(key)) return;

    TileSnapshot snap;
    snap.x = x; snap.y = y; snap.z = z;
    auto it = m_posIndex.find(key);
    if (it != m_posIndex.end()) {
        const OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
        snap.items = t.items;   // kopia (przed zmiana)
        snap.flags = t.flags;   // strefy - inaczej undo nie cofnie malowania PZ
        snap.spawn_radius = t.spawn_radius;          // spawny - jak wyzej
        snap.creature_name = t.creature_name;
        snap.creature_spawntime = t.creature_spawntime;
        snap.creature_is_npc = t.creature_is_npc;
        snap.is_house = t.is_house;                  // domy - jak wyzej
        snap.house_id = t.house_id;
    }
    // jesli kafelek nie istnieje, snap.items zostaje puste

    if (m_undoGrouping) {
        m_groupRecorded.insert(key);
        m_currentGroup.tiles.push_back(std::move(snap));
    } else {
        UndoAction a;
        a.tiles.push_back(std::move(snap));
        pushUndo(std::move(a));
    }
}

void OtbmReader::pushUndo(UndoAction &&action)
{
    if (action.tiles.empty() || m_undoLimit <= 0) return;
    m_undoStack.push_back(std::move(action));
    while (static_cast<int>(m_undoStack.size()) > m_undoLimit) {
        m_undoStack.pop_front();
    }
    // Nowa edycja uniewaznia sciezke redo (galaz historii sie rozeszla).
    m_redoStack.clear();
}

OtbmReader::TileSnapshot OtbmReader::currentSnapshot(int x, int y, int z) const
{
    TileSnapshot snap;
    snap.x = x; snap.y = y; snap.z = z;
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it != m_posIndex.end()) {
        const OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
        snap.items = t.items;
        snap.flags = t.flags;   // strefy (do redo)
        snap.spawn_radius = t.spawn_radius;
        snap.creature_name = t.creature_name;
        snap.creature_spawntime = t.creature_spawntime;
        snap.creature_is_npc = t.creature_is_npc;
        snap.is_house = t.is_house;
        snap.house_id = t.house_id;
    }
    return snap;   // kafelek nie istnieje -> snapshot pusty
}

void OtbmReader::beginUndoGroup()
{
    m_undoGrouping = true;
    m_currentGroup = UndoAction{};
    m_groupRecorded.clear();
}

void OtbmReader::endUndoGroup()
{
    m_undoGrouping = false;
    m_groupRecorded.clear();
    const bool pushed = !m_currentGroup.tiles.empty();
    if (pushed) {
        pushUndo(std::move(m_currentGroup));
    }
    m_currentGroup = UndoAction{};
    if (pushed) emit mapChanged(); // odswiez undoCount w UI
}

void OtbmReader::setUndoLimit(int n)
{
    m_undoLimit = n < 0 ? 0 : n;
    while (static_cast<int>(m_undoStack.size()) > m_undoLimit) {
        m_undoStack.pop_front();
    }
}

void OtbmReader::restoreSnapshot(const TileSnapshot &snap)
{
    const quint64 key = posKey3d(snap.x, snap.y, snap.z);
    auto it = m_posIndex.find(key);
    if (it != m_posIndex.end()) {
        OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
        for (const OtbmMapItem &item : tile.items) m_itemCount -= countItems(item);
        tile.items = snap.items;
        tile.flags = snap.flags;   // przywroc strefy (PZ itp.)
        tile.spawn_radius = snap.spawn_radius;
        tile.creature_name = snap.creature_name;
        tile.creature_spawntime = snap.creature_spawntime;
        tile.creature_is_npc = snap.creature_is_npc;
        tile.is_house = snap.is_house;
        tile.house_id = snap.house_id;
        for (const OtbmMapItem &item : tile.items) m_itemCount += countItems(item);
    } else if (!snap.items.empty() || snap.spawn_radius > 0 || !snap.creature_name.isEmpty()
               || snap.is_house) {
        // Kafelek zniknal, a powinien miec zawartosc (itemy/spawn/potwora) - odtworz.
        OtbmTile tile;
        tile.x = static_cast<uint16_t>(snap.x);
        tile.y = static_cast<uint16_t>(snap.y);
        tile.z = static_cast<uint8_t>(snap.z);
        tile.flags = snap.flags;
        tile.items = snap.items;
        tile.spawn_radius = snap.spawn_radius;
        tile.creature_name = snap.creature_name;
        tile.creature_spawntime = snap.creature_spawntime;
        tile.creature_is_npc = snap.creature_is_npc;
        tile.is_house = snap.is_house;
        tile.house_id = snap.house_id;
        for (const OtbmMapItem &item : tile.items) m_itemCount += countItems(item);
        m_posIndex.insert(key, static_cast<int>(m_tiles.size()));
        m_tiles.push_back(std::move(tile));
    }
    // Kafelek nie istnial i snapshot pusty -> nic (zostawiamy ew. pusty kafelek).
}

bool OtbmReader::undo()
{
    if (m_undoStack.empty()) return false;
    UndoAction action = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Zbuduj akcje przeciwna (redo) = BIEZACY stan kafli PRZED przywroceniem, by
    // redo mogl je odtworzyc. Tej sciezki NIE czyscimy (to nie nowa edycja).
    UndoAction redoAction;
    redoAction.tiles.reserve(action.tiles.size());
    for (const TileSnapshot &snap : action.tiles)
        redoAction.tiles.push_back(currentSnapshot(snap.x, snap.y, snap.z));
    m_redoStack.push_back(std::move(redoAction));
    while (static_cast<int>(m_redoStack.size()) > m_undoLimit) m_redoStack.pop_front();

    m_lastAffected.clear();
    for (const TileSnapshot &snap : action.tiles) {
        restoreSnapshot(snap);
        m_lastAffected.push_back({ snap.x, snap.y, snap.z });
    }
    emit mapChanged();
    return true;
}

bool OtbmReader::redo()
{
    if (m_redoStack.empty()) return false;
    UndoAction action = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Akcja przeciwna wraca na stos undo (bez czyszczenia redo - patrz pushUndo).
    UndoAction undoAction;
    undoAction.tiles.reserve(action.tiles.size());
    for (const TileSnapshot &snap : action.tiles)
        undoAction.tiles.push_back(currentSnapshot(snap.x, snap.y, snap.z));
    m_undoStack.push_back(std::move(undoAction));
    while (static_cast<int>(m_undoStack.size()) > m_undoLimit) m_undoStack.pop_front();

    m_lastAffected.clear();
    for (const TileSnapshot &snap : action.tiles) {
        restoreSnapshot(snap);
        m_lastAffected.push_back({ snap.x, snap.y, snap.z });
    }
    emit mapChanged();
    return true;
}

bool OtbmReader::saveFile(const QString &path)
{
    if (!m_loaded) {
        setError(QStringLiteral("Brak wczytanej mapy do zapisu"));
        return false;
    }

    // Spawny/domy PRZED naglowkiem OTBM: save*Xml moze ustawic m_spawnFile/
    // m_houseFile dla nowej mapy, a Ext*File w naglowku musi juz to widziec.
    if (!saveSpawnsXml(path)) return false;
    if (!saveHousesXml(path)) return false;

    NodeWriter w;
    // 4-bajtowy naglowek wersji (0).
    w.raw(0); w.raw(0); w.raw(0); w.raw(0);

    w.start(static_cast<uint8_t>(OtbmNode::RootHeader));
    w.u32(m_otbmVersion);
    w.u16(m_width);
    w.u16(m_height);
    w.u32(m_otbItemsMajor);
    w.u32(m_otbItemsMinor);

    w.start(static_cast<uint8_t>(OtbmNode::MapData));
    // Opis = wylacznie stempel edytora. Stare linie (stemple innych edytorow,
    // "Template map :)" itp.) wypadaja - nie mamy edytora opisu w UI, wiec ich
    // zachowywanie tylko wleklo smieci z cudzych szablonow. m_description tez
    // aktualizujemy, inaczej Map Properties pokazywaloby stary opis (np. RME)
    // az do ponownego wczytania pliku.
    m_description = QStringLiteral("Saved with Dewral Map Editor 1.0");
    w.data(static_cast<uint8_t>(OtbmAttribute::Description));
    w.str(m_description);
    if (!m_spawnFile.isEmpty()) { w.data(static_cast<uint8_t>(OtbmAttribute::ExtSpawnFile)); w.str(m_spawnFile); }
    if (!m_houseFile.isEmpty()) { w.data(static_cast<uint8_t>(OtbmAttribute::ExtHouseFile)); w.str(m_houseFile); }

    // Grupowanie kafelkow w obszary 256x256 (dzielace baseX/baseY/baseZ).
    QHash<quint64, std::vector<const OtbmTile *>> areas;
    for (const OtbmTile &tile : m_tiles) {
        const int areaX = tile.x & 0xFF00;
        const int areaY = tile.y & 0xFF00;
        areas[posKey3d(areaX, areaY, tile.z)].push_back(&tile);
    }

    for (auto it = areas.begin(); it != areas.end(); ++it) {
        const std::vector<const OtbmTile *> &group = it.value();
        if (group.empty()) continue;
        const int areaX = group.front()->x & 0xFF00;
        const int areaY = group.front()->y & 0xFF00;
        const int areaZ = group.front()->z;

        w.start(static_cast<uint8_t>(OtbmNode::TileArea));
        w.u16(static_cast<uint16_t>(areaX));
        w.u16(static_cast<uint16_t>(areaY));
        w.data(static_cast<uint8_t>(areaZ));

        for (const OtbmTile *tile : group) {
            const bool house = tile->is_house;
            w.start(static_cast<uint8_t>(house ? OtbmNode::HouseTile : OtbmNode::Tile));
            w.data(static_cast<uint8_t>(tile->x & 0xFF));
            w.data(static_cast<uint8_t>(tile->y & 0xFF));
            if (house) {
                w.u32(tile->house_id);
            }
            if (tile->flags != 0) {
                w.data(static_cast<uint8_t>(OtbmAttribute::TileFlags));
                w.u32(tile->flags);
            }
            // Itemy zapisujemy jako wezly-dzieci (pierwszy = ground; czytniki to akceptuja).
            for (const OtbmMapItem &item : tile->items) {
                writeMapItem(w, item);
            }
            w.end(); // tile
        }
        w.end(); // tile area
    }

    // Miasta.
    if (!m_towns.empty()) {
        w.start(static_cast<uint8_t>(OtbmNode::Towns));
        for (const OtbmTown &town : m_towns) {
            w.start(static_cast<uint8_t>(OtbmNode::Town));
            w.u32(town.id);
            w.str(town.name);
            w.u16(town.temple_x);
            w.u16(town.temple_y);
            w.data(town.temple_z);
            w.end();
        }
        w.end();
    }

    // Waypointy.
    if (!m_waypoints.empty()) {
        w.start(static_cast<uint8_t>(OtbmNode::Waypoints));
        for (const OtbmWaypoint &wp : m_waypoints) {
            w.start(static_cast<uint8_t>(OtbmNode::Waypoint));
            w.str(wp.name);
            w.u16(wp.x);
            w.u16(wp.y);
            w.data(wp.z);
            w.end();
        }
        w.end();
    }

    w.end(); // MapData
    w.end(); // root

    // QSaveFile: zapis idzie do pliku tymczasowego i dopiero commit() atomowo go
    // podmienia na docelowy. Awaria w trakcie (brak miejsca, crash, wyciagniety
    // pendrive) zostawia STARY plik nienaruszony, zamiast urwanej mapy - bez tego
    // nieudany zapis kasowal jedyna kopie mapy uzytkownika.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("Nie mozna zapisac pliku: %1").arg(path));
        return false;
    }
    const qint64 written = file.write(w.buf);
    if (written != w.buf.size() || !file.commit()) {
        setError(QStringLiteral("Blad zapisu pliku: %1").arg(path));
        return false;
    }
    if (m_filePath != path) {   // Save As - dokument zmienia tozsamosc
        m_filePath = path;
        emit filePathChanged();
    }
    setDirty(false);
    return true;
}

int OtbmReader::suggestedClientVersion() const
{
    if (!m_loaded) return 0;
    // Tabela id -> wersja klienta, 1:1 z clients.xml RME (<otb client=... id=.../>).
    // Id to otbItemsMinorVersion z naglowka OTBM.
    static const int table[] = {
        0,    740,  755,  772,  780,  790,  792,  800,  810,  811,  // 0-9
        820,  830,  840,  841,  842,  850,  854,  854,  855,  860,  // 10-19
        860,  861,  862,  870,  871,  872,  873,  900,  910,  920,  // 20-29
        940,  944,  944,  944,  944,  946,  950,  952,  953,  954,  // 30-39
        960,  961,  963,  970,  980,  981,  982,  983,  985,  986,  // 40-49
        1010, 1020, 1021, 1030, 1031, 1041, 1077, 1098, 10100       // 50-58
    };
    const int id = static_cast<int>(m_otbItemsMinor);
    if (id >= 1 && id < static_cast<int>(sizeof(table) / sizeof(table[0])))
        return table[id];
    // Nowsze/nieznane id: zalozenie 10.98+ (najczestszy przypadek nowych OTB).
    return id > 0 ? 1098 : 0;
}

QVariantList OtbmReader::townsList() const
{
    QVariantList out;
    for (const OtbmTown &t : m_towns) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), static_cast<int>(t.id));
        m.insert(QStringLiteral("name"), t.name);
        m.insert(QStringLiteral("x"), static_cast<int>(t.temple_x));
        m.insert(QStringLiteral("y"), static_cast<int>(t.temple_y));
        m.insert(QStringLiteral("z"), static_cast<int>(t.temple_z));
        out.append(m);
    }
    return out;
}

QVariantList OtbmReader::waypointsList() const
{
    QVariantList out;
    for (const OtbmWaypoint &wp : m_waypoints) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), wp.name);
        m.insert(QStringLiteral("x"), static_cast<int>(wp.x));
        m.insert(QStringLiteral("y"), static_cast<int>(wp.y));
        m.insert(QStringLiteral("z"), static_cast<int>(wp.z));
        out.append(m);
    }
    return out;
}

int OtbmReader::addTown()
{
    uint32_t maxId = 0;
    for (const OtbmTown &t : m_towns) maxId = std::max(maxId, t.id);
    OtbmTown town;
    town.id = maxId + 1;
    town.name = QStringLiteral("New Town");
    m_towns.push_back(town);
    emit mapChanged();
    return static_cast<int>(town.id);
}

void OtbmReader::removeTown(int id)
{
    auto it = std::find_if(m_towns.begin(), m_towns.end(),
                            [id](const OtbmTown &t) { return static_cast<int>(t.id) == id; });
    if (it == m_towns.end()) return;
    m_towns.erase(it);
    emit mapChanged();
}

void OtbmReader::renameTown(int id, const QString &name)
{
    for (OtbmTown &t : m_towns) {
        if (static_cast<int>(t.id) == id) {
            t.name = name;
            emit mapChanged();
            return;
        }
    }
}

void OtbmReader::setTownTemple(int id, int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 || z > 15) return;
    for (OtbmTown &t : m_towns) {
        if (static_cast<int>(t.id) == id) {
            t.temple_x = static_cast<uint16_t>(x);
            t.temple_y = static_cast<uint16_t>(y);
            t.temple_z = static_cast<uint8_t>(z);
            emit mapChanged();
            return;
        }
    }
}

QVariantMap OtbmReader::header() const
{
    QVariantMap map;
    map.insert(QStringLiteral("loaded"), m_loaded);
    map.insert(QStringLiteral("otbmVersion"), static_cast<int>(m_otbmVersion));
    map.insert(QStringLiteral("width"), m_width);
    map.insert(QStringLiteral("height"), m_height);
    map.insert(QStringLiteral("otbItemsMajorVersion"), static_cast<int>(m_otbItemsMajor));
    map.insert(QStringLiteral("otbItemsMinorVersion"), static_cast<int>(m_otbItemsMinor));
    map.insert(QStringLiteral("description"), m_description);
    map.insert(QStringLiteral("spawnFile"), m_spawnFile);
    map.insert(QStringLiteral("houseFile"), m_houseFile);
    map.insert(QStringLiteral("tileCount"), tileCount());
    map.insert(QStringLiteral("itemCount"), m_itemCount);
    map.insert(QStringLiteral("townCount"), townCount());
    map.insert(QStringLiteral("waypointCount"), waypointCount());
    return map;
}

QVariantList OtbmReader::tilesOnFloor(int z) const
{
    QVariantList result;
    for (const OtbmTile &tile : m_tiles) {
        if (tile.z != z) {
            continue;
        }
        QVariantMap tileMap;
        tileMap.insert(QStringLiteral("x"), tile.x);
        tileMap.insert(QStringLiteral("y"), tile.y);
        tileMap.insert(QStringLiteral("z"), tile.z);
        tileMap.insert(QStringLiteral("flags"), static_cast<int>(tile.flags));
        tileMap.insert(QStringLiteral("isHouse"), tile.is_house);
        if (tile.is_house) {
            tileMap.insert(QStringLiteral("houseId"), tile.house_id);
        }

        QVariantList items;
        for (const OtbmMapItem &item : tile.items) {
            items.append(itemToVariant(item));
        }
        tileMap.insert(QStringLiteral("items"), items);
        result.append(tileMap);
    }
    return result;
}
