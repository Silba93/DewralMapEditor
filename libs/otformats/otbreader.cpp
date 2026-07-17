#include "otbreader.h"

#include "datreader.h"
#include "nodefilereader.h"

namespace {

enum class OtbAttribute : uint8_t {
    ServerId = 0x10,
    ClientId = 0x11,
    Name = 0x12,
    Description = 0x13,
    Speed = 0x14,
    SpriteHash = 0x20,
    MinimapColor = 0x21,
    MaxReadWriteChars = 0x22,
    MaxReadChars = 0x23,
    Light = 0x2A,
    StackOrder = 0x2B,
    TradeAs = 0x2D
};

enum class RootAttribute : uint8_t {
    Version = 0x01
};

enum ItemFlag : uint32_t {
    Unpassable = 1u << 0,
    BlockMissiles = 1u << 1,
    BlockPathfinder = 1u << 2,
    HasElevation = 1u << 3,
    Useable = 1u << 4,
    Pickupable = 1u << 5,
    Moveable = 1u << 6,
    Stackable = 1u << 7,
    AlwaysOnTop = 1u << 13,
    Readable = 1u << 14,
    Rotatable = 1u << 15,
    Hangable = 1u << 16,
    HookEast = 1u << 17,
    HookSouth = 1u << 18
};

uint16_t readPayloadU16(const QByteArray &payload, int offset = 0)
{
    if (offset < 0 || offset + 2 > payload.size()) {
        return 0;
    }

    const auto *raw = reinterpret_cast<const uchar *>(payload.constData());
    return static_cast<uint16_t>(raw[offset])
         | (static_cast<uint16_t>(raw[offset + 1]) << 8);
}

uint32_t readPayloadU32(const QByteArray &payload, int offset = 0)
{
    if (offset < 0 || offset + 4 > payload.size()) {
        return 0;
    }

    const auto *raw = reinterpret_cast<const uchar *>(payload.constData());
    return static_cast<uint32_t>(raw[offset])
         | (static_cast<uint32_t>(raw[offset + 1]) << 8)
         | (static_cast<uint32_t>(raw[offset + 2]) << 16)
         | (static_cast<uint32_t>(raw[offset + 3]) << 24);
}

QString readPayloadString(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return QString();
    }

    if (payload.size() >= 2) {
        const uint16_t length = readPayloadU16(payload);
        if (length <= payload.size() - 2) {
            return QString::fromLatin1(payload.constData() + 2, length);
        }
    }

    return QString::fromLatin1(payload);
}

QVariantList toVariantList(const std::vector<uint32_t> &values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (uint32_t value : values) {
        result.append(QVariant::fromValue(static_cast<quint32>(value)));
    }
    return result;
}

bool hasFlag(uint32_t flags, ItemFlag flag)
{
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void addRow(QVariantList &rows, const QString &name, const QVariant &value)
{
    QVariantMap row;
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("value"), value);
    rows.append(row);
}

QString groupName(uint8_t group)
{
    switch (static_cast<OtbItemGroup>(group)) {
    case OtbItemGroup::Ground: return QStringLiteral("Ground");
    case OtbItemGroup::Container: return QStringLiteral("Container");
    case OtbItemGroup::Weapon: return QStringLiteral("Weapon");
    case OtbItemGroup::Ammunition: return QStringLiteral("Ammunition");
    case OtbItemGroup::Armor: return QStringLiteral("Armor");
    case OtbItemGroup::Changes: return QStringLiteral("Changes");
    case OtbItemGroup::Teleport: return QStringLiteral("Teleport");
    case OtbItemGroup::MagicField: return QStringLiteral("Magic field");
    case OtbItemGroup::Writeable: return QStringLiteral("Writeable");
    case OtbItemGroup::Key: return QStringLiteral("Key");
    case OtbItemGroup::Splash: return QStringLiteral("Splash");
    case OtbItemGroup::Fluid: return QStringLiteral("Fluid");
    case OtbItemGroup::Door: return QStringLiteral("Door");
    case OtbItemGroup::Deprecated: return QStringLiteral("Deprecated");
    case OtbItemGroup::Podium: return QStringLiteral("Podium");
    case OtbItemGroup::None:
    default:
        return QStringLiteral("None");
    }
}

QStringList collectOtbFlags(const OtbItem &item)
{
    QStringList flags;
    if (item.is_unpassable) flags << QStringLiteral("Unpassable");
    if (item.blocks_missiles) flags << QStringLiteral("Blocks missiles");
    if (item.blocks_pathfinder) flags << QStringLiteral("Blocks pathfinder");
    if (item.has_elevation) flags << QStringLiteral("Elevation");
    if (item.is_useable) flags << QStringLiteral("Useable");
    if (item.is_pickupable) flags << QStringLiteral("Pickupable");
    if (item.is_moveable) flags << QStringLiteral("Moveable");
    if (item.is_stackable) flags << QStringLiteral("Stackable");
    if (item.always_on_top) flags << QStringLiteral("Always on top");
    if (item.is_readable) flags << QStringLiteral("Readable");
    if (item.is_rotatable) flags << QStringLiteral("Rotatable");
    if (item.is_hangable) flags << QStringLiteral("Hangable");
    if (item.hook_east) flags << QStringLiteral("Hook east");
    if (item.hook_south) flags << QStringLiteral("Hook south");
    return flags;
}

} // namespace

OtbReader::OtbReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

OtbReader::~OtbReader() = default;

uint8_t OtbReader::mapGroup(uint8_t group)
{
    return group <= static_cast<uint8_t>(OtbItemGroup::Podium)
               ? group
               : static_cast<uint8_t>(OtbItemGroup::None);
}

void OtbReader::setDatReader(DatReader *datReader)
{
    if (m_datReader == datReader) {
        return;
    }

    if (m_datReader) {
        disconnect(m_datReader, nullptr, this, nullptr);
    }

    m_datReader = datReader;

    if (m_datReader) {
        connect(m_datReader, &DatReader::loadedChanged, this, &OtbReader::refreshDatRoles);
        connect(m_datReader, &DatReader::itemCountChanged, this, &OtbReader::refreshDatRoles);
    }
}

void OtbReader::reset()
{
    beginResetModel();
    m_items.clear();
    m_serverIdToRow.clear();
    m_clientIdToRow.clear();
    m_majorVersion = 0;
    m_minorVersion = 0;
    m_buildNumber = 0;
    m_loaded = false;
    m_errorString.clear();
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
    emit errorChanged();
}

void OtbReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool OtbReader::loadFile(const QString &path)
{
    reset();

    NodeFileReader file;
    if (!file.loadFile(path, {QByteArrayLiteral("OTBI"), QByteArray(4, '\0')})) {
        setError(file.errorString());
        return false;
    }

    BinaryNode &root = file.rootNode();

    root.skip(1); // unused 0 byte
    uint32_t unusedFlags = 0;
    root.getU32(unusedFlags);

    uint8_t rootAttr = 0;
    if (root.getU8(rootAttr) && rootAttr == static_cast<uint8_t>(RootAttribute::Version)) {
        uint16_t dataLength = 0;
        if (root.getU16(dataLength) && dataLength >= 12) {
            root.getU32(m_majorVersion);
            root.getU32(m_minorVersion);
            root.getU32(m_buildNumber);
            if (dataLength > 12) {
                root.skip(dataLength - 12);
            }
        }
    }

    std::vector<OtbItem> parsedItems;
    parsedItems.reserve(static_cast<size_t>(root.children().size()));

    for (const BinaryNode &sourceNode : root.children()) {
        BinaryNode node = sourceNode;
        OtbItem item;

        uint8_t group = 0;
        uint32_t flags = 0;
        if (!node.getU8(group) || !node.getU32(flags)) {
            continue;
        }

        item.group = mapGroup(group);
        item.flags = flags;

        item.is_unpassable = hasFlag(flags, Unpassable);
        item.blocks_missiles = hasFlag(flags, BlockMissiles);
        item.blocks_pathfinder = hasFlag(flags, BlockPathfinder);
        item.has_elevation = hasFlag(flags, HasElevation);
        item.is_useable = hasFlag(flags, Useable);
        item.is_pickupable = hasFlag(flags, Pickupable);
        item.is_moveable = !hasFlag(flags, Moveable);
        item.is_stackable = hasFlag(flags, Stackable);
        item.always_on_top = hasFlag(flags, AlwaysOnTop);
        item.is_readable = hasFlag(flags, Readable);
        item.is_rotatable = hasFlag(flags, Rotatable);
        item.is_hangable = hasFlag(flags, Hangable);
        item.hook_east = hasFlag(flags, HookEast);
        item.hook_south = hasFlag(flags, HookSouth);

        while (node.bytesRemaining() > 0) {
            uint8_t attrType = 0;
            uint16_t length = 0;
            if (!node.getU8(attrType) || !node.getU16(length)) {
                break;
            }

            QByteArray payload;
            if (!node.readBytes(length, payload)) {
                break;
            }

            switch (static_cast<OtbAttribute>(attrType)) {
            case OtbAttribute::ServerId:
                item.server_id = readPayloadU16(payload);
                break;
            case OtbAttribute::ClientId:
                item.client_id = readPayloadU16(payload);
                break;
            case OtbAttribute::Name:
                item.name = readPayloadString(payload);
                break;
            case OtbAttribute::Description:
                item.description = readPayloadString(payload);
                break;
            case OtbAttribute::Speed:
                item.speed = readPayloadU16(payload);
                break;
            case OtbAttribute::Light:
                item.light_level = static_cast<uint8_t>(readPayloadU16(payload, 0));
                item.light_color = static_cast<uint8_t>(readPayloadU16(payload, 2));
                break;
            case OtbAttribute::StackOrder:
                item.stack_order = payload.isEmpty() ? 0 : static_cast<int8_t>(payload.at(0));
                break;
            case OtbAttribute::MaxReadWriteChars:
            case OtbAttribute::MaxReadChars:
                item.max_text_length = readPayloadU16(payload);
                break;
            case OtbAttribute::SpriteHash:
            case OtbAttribute::MinimapColor:
            case OtbAttribute::TradeAs:
            default:
                break;
            }
        }

        if (item.server_id > 0) {
            parsedItems.push_back(std::move(item));
        }
    }

    beginResetModel();
    m_items = std::move(parsedItems);
    m_serverIdToRow.clear();
    m_clientIdToRow.clear();

    for (int row = 0; row < static_cast<int>(m_items.size()); ++row) {
        const OtbItem &item = m_items[static_cast<size_t>(row)];
        m_serverIdToRow.insert(item.server_id, row);
        if (item.client_id > 0) {
            m_clientIdToRow.insert(item.client_id, row);
        }
    }

    m_loaded = true;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
    return true;
}

int OtbReader::clientIdForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return 0;
    }

    return m_items[static_cast<size_t>(it.value())].client_id;
}

int OtbReader::topOrderForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return 0;
    }
    return m_items[static_cast<size_t>(it.value())].stack_order;
}

QString OtbReader::nameForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return QString();
    }
    return m_items[static_cast<size_t>(it.value())].name;
}

int OtbReader::rowForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    return it == m_serverIdToRow.end() ? -1 : it.value();
}

QVariantMap OtbReader::detailsAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
        return {};
    }

    const OtbItem &item = m_items[static_cast<size_t>(row)];
    const ClientItem *clientItem = m_datReader ? m_datReader->itemByClientId(item.client_id) : nullptr;
    const QString title = item.name.isEmpty()
                              ? QStringLiteral("Item %1").arg(item.server_id)
                              : item.name;
    const QStringList flags = collectOtbFlags(item);

    QVariantMap details;
    details.insert(QStringLiteral("title"), title);
    details.insert(QStringLiteral("category"), QStringLiteral("Item"));
    details.insert(QStringLiteral("name"), item.name);
    details.insert(QStringLiteral("description"), item.description);
    details.insert(QStringLiteral("itemId"), item.server_id);
    details.insert(QStringLiteral("serverId"), item.server_id);
    details.insert(QStringLiteral("clientId"), item.client_id);
    details.insert(QStringLiteral("group"), groupName(item.group));
    details.insert(QStringLiteral("spriteIds"), clientItem ? toVariantList(clientItem->sprite_ids) : QVariantList());
    details.insert(QStringLiteral("itemWidth"), clientItem ? clientItem->width : 1);
    details.insert(QStringLiteral("itemHeight"), clientItem ? clientItem->height : 1);
    details.insert(QStringLiteral("layers"), clientItem ? clientItem->layers : 1);
    details.insert(QStringLiteral("patternX"), clientItem ? clientItem->pattern_x : 1);
    details.insert(QStringLiteral("patternY"), clientItem ? clientItem->pattern_y : 1);
    details.insert(QStringLiteral("patternZ"), clientItem ? clientItem->pattern_z : 1);
    details.insert(QStringLiteral("frames"), clientItem ? clientItem->frames : 1);
    details.insert(QStringLiteral("spriteCount"), clientItem ? static_cast<int>(clientItem->sprite_ids.size()) : 0);
    details.insert(QStringLiteral("previewSpriteId"), clientItem ? static_cast<quint32>(clientItem->previewSpriteId()) : 0);
    details.insert(QStringLiteral("renderable"), clientItem != nullptr && !clientItem->sprite_ids.empty());
    details.insert(QStringLiteral("flags"), flags);
    details.insert(QStringLiteral("flagsText"), flags.isEmpty() ? QStringLiteral("-") : flags.join(QStringLiteral(", ")));

    QVariantList rows;
    addRow(rows, QStringLiteral("Name"), item.name.isEmpty() ? QStringLiteral("-") : item.name);
    addRow(rows, QStringLiteral("Server ID"), item.server_id);
    addRow(rows, QStringLiteral("Client ID"), item.client_id);
    addRow(rows, QStringLiteral("Group"), groupName(item.group));
    addRow(rows, QStringLiteral("Size"), clientItem ? QStringLiteral("%1x%2").arg(clientItem->width).arg(clientItem->height) : QStringLiteral("-"));
    addRow(rows, QStringLiteral("Layers"), clientItem ? clientItem->layers : 0);
    addRow(rows, QStringLiteral("Pattern X"), clientItem ? clientItem->pattern_x : 0);
    addRow(rows, QStringLiteral("Pattern Y"), clientItem ? clientItem->pattern_y : 0);
    addRow(rows, QStringLiteral("Pattern Z"), clientItem ? clientItem->pattern_z : 0);
    addRow(rows, QStringLiteral("Frames"), clientItem ? clientItem->frames : 0);
    addRow(rows, QStringLiteral("Sprites"), clientItem ? static_cast<int>(clientItem->sprite_ids.size()) : 0);
    addRow(rows, QStringLiteral("First sprite"), clientItem ? static_cast<quint32>(clientItem->previewSpriteId()) : 0);
    if (item.speed != 0) addRow(rows, QStringLiteral("Speed"), item.speed);
    if (item.max_text_length != 0) addRow(rows, QStringLiteral("Max text"), item.max_text_length);
    if (item.light_level != 0 || item.light_color != 0) {
        addRow(rows, QStringLiteral("Light"), QStringLiteral("%1 / %2").arg(item.light_level).arg(item.light_color));
    }
    addRow(rows, QStringLiteral("Stack order"), item.stack_order);
    addRow(rows, QStringLiteral("Flags"), details.value(QStringLiteral("flagsText")));

    if (!item.description.isEmpty()) {
        addRow(rows, QStringLiteral("Description"), item.description);
    }

    details.insert(QStringLiteral("rows"), rows);
    return details;
}

int OtbReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant OtbReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const OtbItem &item = m_items[static_cast<size_t>(index.row())];
    const ClientItem *clientItem = m_datReader ? m_datReader->itemByClientId(item.client_id) : nullptr;

    switch (role) {
    case ItemIdRole:
    case ServerIdRole:
        return item.server_id;
    case ClientIdRole:
        return item.client_id;
    case NameRole:
        return item.name;
    case GroupRole:
        return item.group;
    case SpriteIdsRole:
        return clientItem ? toVariantList(clientItem->sprite_ids) : QVariantList();
    case ItemWidthRole:
        return clientItem ? clientItem->width : 1;
    case ItemHeightRole:
        return clientItem ? clientItem->height : 1;
    case LayersRole:
        return clientItem ? clientItem->layers : 1;
    case IsRenderableRole:
        return clientItem != nullptr && !clientItem->sprite_ids.empty();
    case IsGroundRole:
        return item.group == static_cast<uint8_t>(OtbItemGroup::Ground);
    case IsStackableRole:
        return item.is_stackable;
    case IsContainerRole:
        return item.group == static_cast<uint8_t>(OtbItemGroup::Container);
    case IsUnpassableRole:
        return item.is_unpassable;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> OtbReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ItemIdRole] = "itemId";
    roles[ServerIdRole] = "serverId";
    roles[ClientIdRole] = "clientId";
    roles[NameRole] = "itemName";
    roles[GroupRole] = "itemGroup";
    roles[SpriteIdsRole] = "spriteIds";
    roles[ItemWidthRole] = "itemWidth";
    roles[ItemHeightRole] = "itemHeight";
    roles[LayersRole] = "layers";
    roles[IsRenderableRole] = "isRenderable";
    roles[IsGroundRole] = "isGround";
    roles[IsStackableRole] = "isStackable";
    roles[IsContainerRole] = "isContainer";
    roles[IsUnpassableRole] = "isUnpassable";
    return roles;
}

void OtbReader::refreshDatRoles()
{
    if (m_items.empty()) {
        return;
    }

    const QModelIndex first = index(0, 0);
    const QModelIndex last = index(static_cast<int>(m_items.size()) - 1, 0);
    emit dataChanged(first, last, {SpriteIdsRole, ItemWidthRole, ItemHeightRole, LayersRole, IsRenderableRole});
}
