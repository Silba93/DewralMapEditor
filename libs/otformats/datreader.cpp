#include "datreader.h"
#include "canonicalflags.h"
#include <algorithm>

using namespace CanonicalFlags;

namespace {

QVariantList toVariantList(const std::vector<uint32_t> &values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (uint32_t value : values) {
        result.append(QVariant::fromValue(static_cast<quint32>(value)));
    }
    return result;
}

void addRow(QVariantList &rows, const QString &name, const QVariant &value)
{
    QVariantMap row;
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("value"), value);
    rows.append(row);
}

QStringList collectDatFlags(const ClientItem &item)
{
    QStringList flags;
    if (item.is_ground) flags << QStringLiteral("Ground");
    if (item.is_on_bottom) flags << QStringLiteral("On bottom");
    if (item.is_on_top) flags << QStringLiteral("On top");
    if (item.is_container) flags << QStringLiteral("Container");
    if (item.is_stackable) flags << QStringLiteral("Stackable");
    if (item.is_useable) flags << QStringLiteral("Useable");
    if (item.is_writable) flags << QStringLiteral("Writable");
    if (item.is_fluid_container) flags << QStringLiteral("Fluid container");
    if (item.is_fluid) flags << QStringLiteral("Fluid");
    if (item.is_unpassable) flags << QStringLiteral("Unpassable");
    if (item.is_unmoveable) flags << QStringLiteral("Unmoveable");
    if (item.blocks_missiles) flags << QStringLiteral("Blocks missiles");
    if (item.blocks_pathfinder) flags << QStringLiteral("Blocks pathfinder");
    if (item.is_pickupable) flags << QStringLiteral("Pickupable");
    if (item.is_hangable) flags << QStringLiteral("Hangable");
    if (item.is_horizontal) flags << QStringLiteral("Hook east");
    if (item.is_vertical) flags << QStringLiteral("Hook south");
    if (item.is_rotatable) flags << QStringLiteral("Rotatable");
    if (item.has_light) flags << QStringLiteral("Light");
    if (item.dont_hide) flags << QStringLiteral("Dont hide");
    if (item.is_translucent) flags << QStringLiteral("Translucent");
    if (item.has_offset) flags << QStringLiteral("Offset");
    if (item.has_elevation) flags << QStringLiteral("Elevation");
    if (item.is_lying_object) flags << QStringLiteral("Lying object");
    if (item.animate_always) flags << QStringLiteral("Animate always");
    if (item.has_minimap_color) flags << QStringLiteral("Minimap color");
    if (item.full_ground) flags << QStringLiteral("Full ground");
    if (item.ignore_look) flags << QStringLiteral("Ignore look");
    if (item.floor_change) flags << QStringLiteral("Floor change");
    return flags;
}

} // namespace

// transformFlag 1:1 z DatReaderV755:
// raw 23 -> FLOOR_CHANGE (w kanonicznym ukladzie 23 to TRANSLUCENT,
// ale w formacie 7.55-7.72 bajt 23 oznacza floor change).
// Wszystko inne pasuje 1:1 do CanonicalFlags.
uint8_t DatReader::transformFlag(uint8_t raw) const
{
    // Tabele transformacji 1:1 z RME (loadSpriteMetadataFlags):
    if (m_clientVersion >= 1010) {
        // 10.10+: flaga 16 = "No Movement Animation"; wyzsze przesuniete o +1.
        if (raw == 16) return NO_MOVE_ANIMATION;
        if (raw > 16)  return raw - 1;
        return raw;
    }
    if (m_clientVersion >= 860) {
        return raw;               // 8.60-9.86: format kanoniczny (bez zmian)
    }
    if (m_clientVersion >= 780) {
        // 7.80-8.54: flaga 8 = "Item Charges"; wyzsze przesuniete o +1.
        if (raw == 8) return CHARGEABLE;
        if (raw > 8)  return raw - 1;
        return raw;
    }
    // 7.55-7.72: flaga 23 = "Floor Change".
    return (raw == 23) ? FLOOR_CHANGE : raw;
}

void DatReader::setClientVersion(int v)
{
    if (m_clientVersion == v) return;
    m_clientVersion = v;
    emit clientVersionChanged();
}

DatReader::DatReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

DatReader::~DatReader() = default;

void DatReader::reset()
{
    beginResetModel();
    m_items.clear();
    m_effects.clear();
    m_outfits.clear();
    m_signature = 0;
    m_maxItemId = 0;
    m_loaded = false;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
}

void DatReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool DatReader::loadFile(const QString &path, quint32 expectedSignature)
{
    reset();

    BinaryReader reader(path);
    if (!reader.isOpen()) {
        setError(QStringLiteral("Nie mozna otworzyc pliku: %1").arg(path));
        return false;
    }

    m_signature = reader.readU32();
    if (expectedSignature != 0 && m_signature != expectedSignature) {
        setError(QStringLiteral("Nieprawidlowa sygnatura .dat (oczekiwano 0x%1, otrzymano 0x%2)")
                      .arg(expectedSignature, 0, 16)
                      .arg(m_signature, 0, 16));
        return false;
    }

    m_maxItemId                   = reader.readU16();
    const uint16_t maxOutfitId    = reader.readU16();
    const uint16_t maxEffectId    = reader.readU16();
    const uint16_t maxMissileId   = reader.readU16();

    if (!reader.good()) {
        setError(QStringLiteral("Blad odczytu naglowka pliku .dat"));
        return false;
    }

    std::vector<ClientItem> items;
    std::vector<ClientItem> effects;
    std::vector<ClientItem> outfits;

    // Items (ID 100..max_item_id) - przechowujemy
    readCategory(reader, &items, 100, m_maxItemId);

    if (!reader.good() && reader.hasError()) {
        setError(QStringLiteral("Blad parsowania items: %1").arg(reader.getError()));
        return false;
    }

    // Outfits - PRZECHOWUJEMY (potwory/NPC na spawnach; outfits=true bo frame
    // groups w 10.57+ przesuwaja strumien). Effects - PRZECHOWUJEMY (efekty
    // magiczne). Missiles - odrzucamy.
    readCategory(reader, &outfits, 1, maxOutfitId, true);
    readCategory(reader, &effects, 1, maxEffectId);
    readCategory(reader, nullptr, 1, maxMissileId);

    beginResetModel();
    m_items = std::move(items);
    m_effects = std::move(effects);
    m_outfits = std::move(outfits);
    m_loaded = true;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();

    return true;
}

void DatReader::readCategory(BinaryReader &reader,
                              std::vector<ClientItem> *outItems,
                              uint16_t minId, uint16_t maxId, bool outfits)
{
    if (outItems && maxId >= minId) {
        outItems->reserve(static_cast<size_t>(maxId) - minId + 1);
    }

    for (int id = static_cast<int>(minId); id <= static_cast<int>(maxId); ++id) {
        if (!reader.good()) break;

        ClientItem item;
        item.id = static_cast<uint16_t>(id);
        readItemFlags(item, reader);
        readSpriteData(item, reader, outfits);

        if (outItems) {
            outItems->push_back(std::move(item));
        }
        // jesli outItems == nullptr: item zostaje zniszczony po wyjsciu z bloku,
        // ale strumien zostal poprawnie przesuniety przez readItemFlags/readSpriteData
    }
}

void DatReader::readItemFlags(ClientItem &item, BinaryReader &reader)
{
    while (true) {
        const uint8_t rawFlag = reader.readU8();
        if (!reader.good()) break;
        if (rawFlag == LAST) break;

        const uint8_t flag = transformFlag(rawFlag);

        switch (flag) {
        case GROUND:
            item.is_ground = true;
            item.ground_speed = reader.readU16();
            break;
        case GROUND_BORDER:
        case ON_BOTTOM:
            item.is_on_bottom = true;
            break;
        case ON_TOP:
            item.is_on_top = true;
            break;
        case CONTAINER:
            item.is_container = true;
            break;
        case STACKABLE:
            item.is_stackable = true;
            break;
        case FORCE_USE:
            break;
        case MULTI_USE:
            item.is_useable = true;
            break;
        case WRITABLE:
        case WRITABLE_ONCE:
            item.is_writable = true;
            item.max_text_length = reader.readU16();
            break;
        case FLUID_CONTAINER:
            item.is_fluid_container = true;
            break;
        case FLUID:
            item.is_fluid = true;
            break;
        case UNPASSABLE:
            item.is_unpassable = true;
            break;
        case UNMOVEABLE:
            item.is_unmoveable = true;
            break;
        case BLOCK_MISSILE:
            item.blocks_missiles = true;
            break;
        case BLOCK_PATHFINDER:
            item.blocks_pathfinder = true;
            break;
        case PICKUPABLE:
            item.is_pickupable = true;
            break;
        case HANGABLE:
            item.is_hangable = true;
            break;
        case HOOK_SOUTH:
            item.is_vertical = true;
            break;
        case HOOK_EAST:
            item.is_horizontal = true;
            break;
        case ROTATABLE:
            item.is_rotatable = true;
            break;
        case HAS_LIGHT:
            item.has_light = true;
            item.light_level = reader.readU16();
            item.light_color = reader.readU16();
            break;
        case DONT_HIDE:
            item.dont_hide = true;
            break;
        case TRANSLUCENT:
            item.is_translucent = true;
            break;
        case HAS_OFFSET:
            item.has_offset = true;
            item.offset_x = static_cast<int16_t>(reader.readU16());
            item.offset_y = static_cast<int16_t>(reader.readU16());
            break;
        case HAS_ELEVATION:
            item.has_elevation = true;
            item.elevation = reader.readU16();
            break;
        case LYING_OBJECT:
            item.is_lying_object = true;
            break;
        case ANIMATE_ALWAYS:
            item.animate_always = true;
            break;
        case MINI_MAP:
            item.has_minimap_color = true;
            item.minimap_color = reader.readU16();
            break;
        case LENS_HELP:
            item.lens_help = reader.readU16();
            break;
        case FULL_GROUND:
            item.full_ground = true;
            break;
        case IGNORE_LOOK:
            item.ignore_look = true;
            break;
        case FLOOR_CHANGE:
            item.floor_change = true;
            break;

        // --- Flagi nowszych wersji (payloady 1:1 z RME) ---
        case CLOTH:            // 7.9+: slot ubrania
            reader.readU16();
            break;
        case MARKET_ITEM: {    // 10.10+: category u16, tradeAs u16, showAs u16,
            reader.readBytes(6);          //         nazwa (string u16-len!), prof u16, lvl u16
            reader.readString();          // length-prefixed - MUSI byc przeczytana
            reader.readBytes(4);
            break;
        }
        case DEFAULT_ACTION:   // 10.x (RME: DatFlagUsable, payload u16)
            reader.readU16();
            break;
        case WRAPPABLE:        // bez payloadu
        case UNWRAPPABLE:
        case TOP_EFFECT:
        case CHARGEABLE:       // 7.80-8.54 (bez payloadu)
        case NO_MOVE_ANIMATION: // 10.10+ (bez payloadu)
            break;

        default:
            // Nieznana flaga bez payloadu - jak RME: ostrzezenie i czytaj dalej.
            // (Flagi z payloadem sa wszystkie obsluzone powyzej.)
            break;
        }
    }
}

void DatReader::readSpriteData(ClientItem &item, BinaryReader &reader, bool outfits)
{
    // 10.57+: OUTFITY maja grupy klatek (idle/moving) - kazda grupa to osobny
    // komplet geometrii+sprite'ow. Itemy/efekty/pociski: zawsze 1 "grupa".
    uint8_t groupCount = 1;
    const bool hasGroups = outfits && frameGroups();
    if (hasGroups) groupCount = std::max<uint8_t>(1, reader.readU8());

    for (uint8_t g = 0; g < groupCount; ++g) {
        if (hasGroups) reader.readU8();   // typ grupy (idle/moving) - pomijamy

        item.width  = reader.readU8();
        item.height = reader.readU8();

        if (item.width > 1 || item.height > 1) {
            reader.readU8(); // exact_size - nieuzywane, czytamy zeby nie zgubic offsetu
        }

        item.layers    = reader.readU8();
        item.pattern_x = reader.readU8();
        item.pattern_y = reader.readU8();
        item.pattern_z = reader.readU8(); // czytany z pliku od 7.55+
        item.frames    = reader.readU8();

        // 10.50+: animowane rzeczy maja naglowek animacji + czasy klatek.
        if (item.frames > 1 && frameDurations()) {
            reader.readU8();                       // async (0/1)
            reader.readU32();                      // loop count (i32)
            reader.readU8();                       // start frame (i8)
            for (uint32_t f = 0; f < item.frames; ++f) {
                reader.readU32();                  // min duration ms
                reader.readU32();                  // max duration ms
            }
        }

        const uint32_t spriteCount = item.getTotalSprites();
        if (g == 0) item.sprite_ids.reserve(spriteCount);

        for (uint32_t i = 0; i < spriteCount; ++i) {
            // 9.60+: sprite ID w .dat to u32 (extended); starsze: u16.
            const uint32_t sid = extendedSprites() ? reader.readU32() : reader.readU16();
            if (g == 0) item.sprite_ids.push_back(sid);
            // kolejne grupy (outfity): tylko przesuwamy strumien
        }
    }
}

const ClientItem *DatReader::outfitByLookType(uint16_t lookType) const
{
    // Looktype 1-indeksowany: outfit N lezy pod m_outfits[N-1].
    if (lookType == 0 || static_cast<size_t>(lookType) > m_outfits.size()) {
        return nullptr;
    }
    return &m_outfits[static_cast<size_t>(lookType) - 1];
}

QVariantMap DatReader::outfitPreview(int lookType) const
{
    QVariantMap out;
    const ClientItem *of = outfitByLookType(static_cast<uint16_t>(std::max(0, lookType)));
    if (!of || of->sprite_ids.empty()) return out;

    const int w = std::max<int>(1, of->width);
    const int h = std::max<int>(1, of->height);
    const int patX = std::max<int>(1, of->pattern_x);
    const int layers = std::max<int>(1, of->layers);
    const int dir = std::min(2, patX - 1);   // 2 = poludnie (jak render na mapie)

    QVariantList ids;
    for (int hh = 0; hh < h; ++hh)
        for (int ww = 0; ww < w; ++ww) {
            const int idx = ((dir * layers + 0) * h + hh) * w + ww;
            ids.push_back(idx >= 0 && idx < static_cast<int>(of->sprite_ids.size())
                              ? QVariant(of->sprite_ids[static_cast<size_t>(idx)])
                              : QVariant(0u));
        }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("width"), w);
    out.insert(QStringLiteral("height"), h);
    return out;
}

QVariantMap DatReader::itemPreview(int clientId) const
{
    QVariantMap out;
    const ClientItem *ci = itemByClientId(static_cast<uint16_t>(std::max(0, clientId)));
    if (!ci || ci->sprite_ids.empty()) return out;

    const int w = std::max<int>(1, ci->width);
    const int h = std::max<int>(1, ci->height);
    QVariantList ids;
    for (int hh = 0; hh < h; ++hh)
        for (int ww = 0; ww < w; ++ww) {
            const int idx = hh * w + ww;   // warstwa 0, pattern 0, klatka 0
            ids.push_back(idx < static_cast<int>(ci->sprite_ids.size())
                              ? QVariant(ci->sprite_ids[static_cast<size_t>(idx)])
                              : QVariant(0u));
        }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("width"), w);
    out.insert(QStringLiteral("height"), h);
    return out;
}

const ClientItem *DatReader::itemByClientId(uint16_t clientId) const
{
    if (clientId < 100 || clientId > m_maxItemId) {
        return nullptr;
    }

    const size_t idx = static_cast<size_t>(clientId - 100);
    if (idx >= m_items.size() || m_items[idx].id != clientId) {
        return nullptr;
    }

    return &m_items[idx];
}

const ClientItem *DatReader::effectById(int id) const
{
    if (id < 1 || static_cast<size_t>(id) > m_effects.size()) return nullptr;
    return &m_effects[static_cast<size_t>(id - 1)];   // efekty 1-indeksowane
}

quint32 DatReader::previewSpriteIdAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return 0;
    return m_items[static_cast<size_t>(row)].previewSpriteId();
}

int DatReader::itemIdAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return 0;
    return m_items[static_cast<size_t>(row)].id;
}

QVariantMap DatReader::detailsAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return {};

    const ClientItem &item = m_items[static_cast<size_t>(row)];
    const QStringList flags = collectDatFlags(item);

    QVariantMap details;
    details.insert(QStringLiteral("title"),          QStringLiteral("Item %1").arg(item.id));
    details.insert(QStringLiteral("category"),       QStringLiteral("Item"));
    details.insert(QStringLiteral("idLabel"),        QStringLiteral("Client ID"));
    details.insert(QStringLiteral("itemId"),         item.id);
    details.insert(QStringLiteral("clientId"),       item.id);
    details.insert(QStringLiteral("spriteIds"),      toVariantList(item.sprite_ids));
    details.insert(QStringLiteral("itemWidth"),      item.width);
    details.insert(QStringLiteral("itemHeight"),     item.height);
    details.insert(QStringLiteral("layers"),         item.layers);
    details.insert(QStringLiteral("patternX"),       item.pattern_x);
    details.insert(QStringLiteral("patternY"),       item.pattern_y);
    details.insert(QStringLiteral("patternZ"),       item.pattern_z);
    details.insert(QStringLiteral("frames"),         item.frames);
    details.insert(QStringLiteral("spriteCount"),    static_cast<int>(item.sprite_ids.size()));
    details.insert(QStringLiteral("previewSpriteId"),static_cast<quint32>(item.previewSpriteId()));
    details.insert(QStringLiteral("flags"),          flags);
    details.insert(QStringLiteral("flagsText"),      flags.isEmpty() ? QStringLiteral("-") : flags.join(QStringLiteral(", ")));

    QVariantList rows;
    addRow(rows, QStringLiteral("Client ID"),    item.id);
    addRow(rows, QStringLiteral("Size"),         QStringLiteral("%1x%2").arg(item.width).arg(item.height));
    addRow(rows, QStringLiteral("Layers"),       item.layers);
    addRow(rows, QStringLiteral("Pattern X"),    item.pattern_x);
    addRow(rows, QStringLiteral("Pattern Y"),    item.pattern_y);
    addRow(rows, QStringLiteral("Pattern Z"),    item.pattern_z);
    addRow(rows, QStringLiteral("Frames"),       item.frames);
    addRow(rows, QStringLiteral("Sprites"),      static_cast<int>(item.sprite_ids.size()));
    addRow(rows, QStringLiteral("First sprite"), static_cast<quint32>(item.previewSpriteId()));

    if (item.is_ground)         addRow(rows, QStringLiteral("Ground speed"),  item.ground_speed);
    if (item.is_writable)       addRow(rows, QStringLiteral("Max text"),       item.max_text_length);
    if (item.has_light)         addRow(rows, QStringLiteral("Light"),          QStringLiteral("%1 / %2").arg(item.light_level).arg(item.light_color));
    if (item.has_offset)        addRow(rows, QStringLiteral("Offset"),         QStringLiteral("%1, %2").arg(item.offset_x).arg(item.offset_y));
    if (item.has_elevation)     addRow(rows, QStringLiteral("Elevation"),      item.elevation);
    if (item.has_minimap_color) addRow(rows, QStringLiteral("Minimap color"),  item.minimap_color);
    if (item.lens_help != 0)    addRow(rows, QStringLiteral("Lens help"),      item.lens_help);
    addRow(rows, QStringLiteral("Flags"), details.value(QStringLiteral("flagsText")));

    details.insert(QStringLiteral("rows"), rows);
    return details;
}

int DatReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant DatReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const ClientItem &item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case ItemIdRole:         return item.id;
    case PreviewSpriteIdRole: return item.previewSpriteId();
    case SpriteIdsRole:      return toVariantList(item.sprite_ids);
    case ItemWidthRole:      return item.width;
    case ItemHeightRole:     return item.height;
    case LayersRole:         return item.layers;
    case IsGroundRole:       return item.is_ground;
    case IsStackableRole:    return item.is_stackable;
    case IsContainerRole:    return item.is_container;
    case IsUnpassableRole:   return item.is_unpassable;
    default:                 return QVariant();
    }
}

QHash<int, QByteArray> DatReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ItemIdRole]          = "itemId";
    roles[PreviewSpriteIdRole] = "previewSpriteId";
    roles[SpriteIdsRole]       = "spriteIds";
    roles[ItemWidthRole]       = "itemWidth";
    roles[ItemHeightRole]      = "itemHeight";
    roles[LayersRole]          = "layers";
    roles[IsGroundRole]        = "isGround";
    roles[IsStackableRole]     = "isStackable";
    roles[IsContainerRole]     = "isContainer";
    roles[IsUnpassableRole]    = "isUnpassable";
    return roles;
}
