#ifndef OTBMREADER_H
#define OTBMREADER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <deque>
#include <vector>

class BinaryNode;

// -----------------------------------------------------------------------------
// OtbmReader
//
// Parser map OTBM (Open Tibia Binary Map). OTBM uzywa dokladnie tego samego
// formatu drzewa node'ow co items.otb (markery 0xFE/0xFF/0xFD), wiec do
// odczytu surowej struktury wykorzystujemy istniejacy NodeFileReader, a tutaj
// tylko interpretujemy poszczegolne wezly mapy.
//
// Wzorowane na MapEditor::IO::Otbm::OtbmReader z repo tibia-imgui-map-editor.
//
// Struktura pliku:
//   [u32 naglowek wersji = 0] 0xFE
//     RootHeader: u32 wersja OTBM, u16 width, u16 height, u32 otbMajor, u32 otbMinor
//     0xFE MapData: atrybuty (description, spawn file, house file)
//       0xFE TileArea: u16 baseX, u16 baseY, u8 baseZ
//         0xFE Tile/HouseTile: u8 dx, u8 dy, [u32 houseId], atrybuty, dzieci-itemy
//       0xFE Towns / Waypoints ...
// -----------------------------------------------------------------------------

// Numeracja wezlow 1:1 z OtbmReader.h z repo referencyjnego.
enum class OtbmNode : uint8_t {
    RootHeader = 0,
    MapData = 2,
    TileArea = 4,
    Tile = 5,
    Item = 6,
    Spawns = 9,
    SpawnArea = 10,
    Monster = 11,
    Towns = 12,
    Town = 13,
    HouseTile = 14,
    Waypoints = 15,
    Waypoint = 16
};

enum class OtbmAttribute : uint8_t {
    Description = 1,
    ExtFile = 2,
    TileFlags = 3,
    ActionId = 4,
    UniqueId = 5,
    Text = 6,
    Desc = 7,
    TeleportDest = 8,
    Item = 9,
    DepotId = 10,
    ExtSpawnFile = 11,
    RuneCharges = 12,
    ExtHouseFile = 13,
    HouseDoorId = 14,
    Count = 15,
    Duration = 16,
    DecayingState = 17,
    WrittenDate = 18,
    WrittenBy = 19,
    SleeperGuid = 20,
    SleepStart = 21,
    Charges = 22,
    ExtSpawnNpcFile = 23,
    PodiumOutfit = 40,
    Tier = 41,
    AttributeMap = 128
};

enum class OtbmVersion : uint32_t {
    V1 = 0,
    V2 = 1,
    V3 = 2,
    V4 = 3
};

enum OtbmTileFlag : uint32_t {
    TileNone = 0,
    TileProtection = 1u << 0,
    TileDeprecated = 1u << 1,
    TileNoPvp = 1u << 2,
    TileNoLogout = 1u << 3,
    TilePvpZone = 1u << 4,
    TileRefresh = 1u << 5
};

struct OtbmMapItem {
    uint16_t server_id = 0;
    uint16_t count = 1;
    uint16_t depot_id = 0;
    uint32_t action_id = 0;
    uint32_t unique_id = 0;
    bool is_ground = false;
    // Zawartosc kontenerow (rekurencyjnie). Puste dla zwyklych itemow.
    std::vector<OtbmMapItem> children;
};

struct OtbmTile {
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t z = 0;
    uint32_t flags = 0;
    bool is_house = false;
    uint32_t house_id = 0;
    std::vector<OtbmMapItem> items; // ground jako pierwszy

    // Spawny (model RME): centrum spawnu to kafel z radius > 0; potwory/NPC leza
    // na WLASNYCH kaflach w promieniu. Zapis idzie do spawns.xml (ExtSpawnFile),
    // NIE do OTBM - te pola nie dotykaja writeMapItem/parseTile.
    int spawn_radius = 0;           // > 0 = ten kafel jest centrum spawnu
    QString creature_name;          // niepuste = potwor/NPC stoi na tym kaflu
    int creature_spawntime = 60;    // sekundy (atrybut spawntime w XML)
    bool creature_is_npc = false;
};

struct OtbmTown {
    uint32_t id = 0;
    QString name;
    uint16_t temple_x = 0;
    uint16_t temple_y = 0;
    uint8_t temple_z = 0;
};

struct OtbmWaypoint {
    QString name;
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t z = 0;
};

// Dom (houses.xml, format RME/TFS). Kafle domu zyja w OTBM (HouseTile: is_house +
// house_id na OtbmTile); tu tylko metadane. "size" liczone przy zapisie z kafli.
struct OtbmHouse {
    uint32_t id = 0;
    QString name;
    int rent = 0;
    int townId = 0;
    bool guildhall = false;
    int entryX = 0, entryY = 0, entryZ = 0;
};

class OtbmReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    // Stan DOKUMENTU (system kart map): kazda otwarta mapa nosi wlasna sciezke i flage
    // niezapisanych zmian. Wczesniej to byly globalne property w Main.qml - z tabami
    // musialyby byc recznie przelaczane, a tak podrozuja razem z readerem.
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(int width READ width NOTIFY loadedChanged)
    Q_PROPERTY(int height READ height NOTIFY loadedChanged)
    Q_PROPERTY(int otbmVersion READ otbmVersion NOTIFY loadedChanged)
    Q_PROPERTY(int otbItemsMajorVersion READ otbItemsMajorVersion NOTIFY loadedChanged)
    Q_PROPERTY(int otbItemsMinorVersion READ otbItemsMinorVersion NOTIFY loadedChanged)
    Q_PROPERTY(QString description READ description NOTIFY loadedChanged)
    Q_PROPERTY(QString spawnFile READ spawnFile NOTIFY loadedChanged)
    Q_PROPERTY(QString houseFile READ houseFile NOTIFY loadedChanged)
    Q_PROPERTY(int tileCount READ tileCount NOTIFY loadedChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY loadedChanged)
    Q_PROPERTY(int townCount READ townCount NOTIFY loadedChanged)
    Q_PROPERTY(int waypointCount READ waypointCount NOTIFY loadedChanged)
    Q_PROPERTY(int undoCount READ undoCount NOTIFY mapChanged)
    Q_PROPERTY(int redoCount READ redoCount NOTIFY mapChanged)

public:
    explicit OtbmReader(QObject *parent = nullptr);
    ~OtbmReader() override;

    bool isLoaded() const { return m_loaded; }
    bool isDirty() const { return m_dirty; }
    QString filePath() const { return m_filePath; }

    // Nowa PUSTA mapa (File > New): zeruje stan i ustawia naglowek tak, by zapis
    // i wykrywanie wersji klienta (suggestedClientVersion z wersji items.otb)
    // dzialaly od razu. Wersja OTBM wg RME clients.xml: <8.0 -> OTBM1, 8.0x -> OTBM2,
    // nowsze -> OTBM3 (w pliku 0-based, patrz OtbmVersion). filePath zostaje pusty -
    // Save robi wtedy Save As.
    Q_INVOKABLE bool newMap(int width, int height, int clientVersion,
                            int otbMajor, int otbMinor);

    // Ustawia wersje naglowka pod AKTUALNEGO klienta (OTBM wg wersji klienta, items
    // wg zaladowanego items.otb) - QML wola to przed KAZDYM zapisem, jak robi RME.
    Q_INVOKABLE void applyClientVersions(int clientVersion, int otbMajor, int otbMinor);
    QString errorString() const { return m_errorString; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int otbmVersion() const { return static_cast<int>(m_otbmVersion); }
    int otbItemsMajorVersion() const { return static_cast<int>(m_otbItemsMajor); }
    int otbItemsMinorVersion() const { return static_cast<int>(m_otbItemsMinor); }
    QString description() const { return m_description; }
    QString spawnFile() const { return m_spawnFile; }
    QString houseFile() const { return m_houseFile; }
    int tileCount() const { return static_cast<int>(m_tiles.size()); }
    int itemCount() const { return m_itemCount; }
    int townCount() const { return static_cast<int>(m_towns.size()); }
    int waypointCount() const { return static_cast<int>(m_waypoints.size()); }

    // deque: stabilne wskazniki do elementow przy push_back (edycja dodaje kafelki,
    // nigdy nie usuwa), wiec indeks renderera nie wymaga przebudowy po realokacji.
    const std::deque<OtbmTile> &tiles() const { return m_tiles; }
    // Wskaznik do kafelka na pozycji (lub nullptr). Stabilny przez caly czas zycia.
    const OtbmTile *tileAt(int x, int y, int z) const;
    const std::vector<OtbmTown> &towns() const { return m_towns; }
    const std::vector<OtbmWaypoint> &waypoints() const { return m_waypoints; }
    // Listy dla QML (do okien "Edit Towns" / waypointow): [{name,id,x,y,z}].
    Q_INVOKABLE QVariantList townsList() const;
    Q_INVOKABLE QVariantList waypointsList() const;

    // --- Edycja miast (Map > Edit Towns w QML) ---
    // Dodaje nowe miasto (id = max(istniejace)+1, nazwa "New Town", temple 0,0,0).
    // Zwraca nowe id.
    Q_INVOKABLE int addTown();
    Q_INVOKABLE void removeTown(int id);
    Q_INVOKABLE void renameTown(int id, const QString &name);
    Q_INVOKABLE void setTownTemple(int id, int x, int y, int z);

    Q_INVOKABLE bool loadFile(const QString &path);
    // Zapisuje biezacy stan mapy do pliku .otbm (format drzewa node'ow).
    Q_INVOKABLE bool saveFile(const QString &path);
    Q_INVOKABLE QVariantMap header() const;
    // Wersja klienta (772, 860, 1098...) wywnioskowana z naglowka OTBM
    // (otbItemsMinorVersion = id z tabeli clients.xml RME). 0 = nieznana.
    Q_INVOKABLE int suggestedClientVersion() const;
    // Lista itemow (server id) na danym pietrze - przydatne do podgladu/renderu.
    Q_INVOKABLE QVariantList tilesOnFloor(int z) const;

    // Edycja: dodaje item (server_id) na kafelek; tworzy kafelek jesli nie istnieje.
    // Pierwszy item na nowym kafelku staje sie groundem. Zwraca true gdy dodano.
    // UWAGA: utworzenie nowego kafelka moze przealokowac m_tiles - wszystkie
    // wczesniej pobrane wskazniki/referencje do kafelkow staja sie niewazne.
    bool addItem(int x, int y, int z, uint16_t serverId);
    // Wstawia/zastepuje item na konkretnej pozycji w stosie kafelka (tworzy
    // kafelek jesli nie istnieje). replace=true nadpisuje item pod indeksem
    // (uzywane do podmiany ground). isGround ustawia flage ground itemu.
    bool placeItem(int x, int y, int z, uint16_t serverId,
                   int index, bool replace, bool isGround);
    // Jak wyzej, ale wstawia GOTOWY item - zachowuje count (rozmiar sterty), action_id,
    // unique_id, depot_id i zawartosc kontenera. Move/paste musi isc ta sciezka, inaczej
    // przenoszony item odradza sie z domyslnym count=1.
    bool placeItem(int x, int y, int z, const OtbmMapItem &item,
                   int index, bool replace, bool isGround);
    // Usuwa wierzchni item z kafelka. Zwraca true gdy cos usunieto.
    bool removeTopItem(int x, int y, int z);
    // Ustawia flagi kafelka (strefy: PZ / No-PvP / No-Logout / PvP - OTBM_ATTR_TILE_FLAGS).
    // Dziala tylko na ISTNIEJACYCH kafelkach (jak RME - strefy sa cecha kafla, nie tworza go).
    // Zwraca true gdy cos sie zmienilo.
    bool setTileFlags(int x, int y, int z, uint32_t flags);
    // Flagi kafelka lub 0 gdy kafel nie istnieje.
    uint32_t tileFlags(int x, int y, int z) const;

    // Podmienia na kafelku wszystkie itemy fromId -> toId W MIEJSCU (zachowuje pozycje
    // w stosie, w odroznieniu od remove+add). Zwraca liczbe podmian.
    // Wchodzi tez w zawartosc kontenerow (OtbmMapItem::children).
    int replaceItemsById(int x, int y, int z, uint16_t fromId, uint16_t toId);

    // --- Domy (jak RME: metadane w houses.xml, kafle jako HouseTile w OTBM) ---
    // Lista domow: [{id, name, rent, townId, guildhall, entryX/Y/Z, size}].
    Q_INVOKABLE QVariantList housesList() const;
    Q_INVOKABLE int addHouse(int townId);            // nowy dom, zwraca id
    Q_INVOKABLE void removeHouse(int id);            // usuwa metadane + CZYSCI kafle domu
    Q_INVOKABLE void setHouseName(int id, const QString &name);
    Q_INVOKABLE void setHouseRent(int id, int rent);
    // Miasto domu (jak RME house_choice) - TFS/RME wymagaja poprawnego townid
    // wskazujacego na ISTNIEJACE miasto (0 nie jest realnym id - addTown numeruje
    // od 1), inaczej dom moze byc niekupowalny/niewchodzalny w grze.
    Q_INVOKABLE void setHouseTownId(int id, int townId);
    Q_INVOKABLE void setHouseEntry(int id, int x, int y, int z);
    // Kafel domu (RME HouseBrush): draw = house_id + flaga PZ; undraw czysci oba.
    bool setHouseTileAt(int x, int y, int z, uint32_t houseId);
    bool clearHouseTileAt(int x, int y, int z);

    // --- Spawny (model RME: centrum+radius na kaflu, potwory na wlasnych kaflach).
    // Wszystko ze snapshotem undo; radius/creature czyszczone przez wartosci 0/"".
    // Tworza kafel gdy nie istnieje (spawn moze stac na pustym terenie).
    bool setSpawnAt(int x, int y, int z, int radius);
    bool setCreatureAt(int x, int y, int z, const QString &name, int spawntime, bool isNpc);
    bool clearSpawnAt(int x, int y, int z);      // usuwa centrum spawnu
    bool clearCreatureAt(int x, int y, int z);   // usuwa potwora z kafla

    // Ustawia count (rozmiar sterty) WIERZCHNIEGO itemu kafelka; jedno cofniecie.
    // Sens ma tylko dla stackowalnych - czy item nim jest, sprawdza wolajacy (zna .dat).
    // Zapis do OTBM wychodzi tylko dla count > 1 (patrz writeMapItem).
    bool setTopItemCount(int x, int y, int z, uint16_t count);

    // Ile itemow o danym server-id na kafelku, wliczajac zawartosc kontenerow.
    int countItemsOnTile(int x, int y, int z, int serverId) const;

    // --- Operacje na CALEJ mapie (menu Edit, jak RME) ---
    // Wszystkie jako jedna grupa undo; wypelniaja lastAffected() dotknietymi kaflami.
    // Licza/zmieniaja tez itemy w kontenerach (torba w skrzyni w depocie...).
    Q_INVOKABLE int countItemsOnMap(int serverId) const;
    int replaceItemsOnMap(uint16_t fromId, uint16_t toId);
    int removeItemsOnMap(uint16_t serverId);
    // Pierwsze wystapienie itemu na mapie (do "skocz do") - {x,y,z} lub pusty QVariantMap.
    Q_INVOKABLE QVariantMap findFirstItemOnMap(int serverId) const;

    // Usuwa z kafelka WSZYSTKIE itemy o podanych server-id (jednym snapshotem undo).
    // Uzywane przez auto-bordery ("cleanBorders" - kasuje stare kafle bordera przed
    // przeliczeniem). Zwraca liczbe usunietych itemow.
    // deep=true schodzi tez w kontenery (menu Remove Item); domyslnie plytko, bo sciezka
    // borderow wola to na kazdym kaflu, a bordery nigdy nie leza w torbie.
    int removeItemsById(int x, int y, int z, const std::vector<uint16_t> &ids, bool deep = false);

    // --- Undo (Ctrl+Z) / Redo (Ctrl+Y) ---
    // Grupowanie kilku zmian w jedno cofniecie (np. pociagniecie pedzlem / move).
    void beginUndoGroup();
    void endUndoGroup();
    Q_INVOKABLE bool undo();                 // cofa ostatnia akcje
    Q_INVOKABLE bool redo();                 // ponawia cofnieta akcje
    // Pozycje kafli dotknietych przez OSTATNIE undo()/redo() - do PUNKTOWEGO,
    // SYNCHRONICZNEGO odswiezenia renderu (jak przy zwyklej edycji), zamiast polegac
    // na asynchronicznym przeliczaniu chunkow (ktore budzi render zawodnie -> zmiana
    // widoczna dopiero po kolejnym kliknieciu).
    struct EditPos { int x, y, z; };
    const std::vector<EditPos> &lastAffected() const { return m_lastAffected; }
    int undoCount() const { return static_cast<int>(m_undoStack.size()); }
    int redoCount() const { return static_cast<int>(m_redoStack.size()); }
    Q_INVOKABLE int undoLimit() const { return m_undoLimit; }
    Q_INVOKABLE void setUndoLimit(int n);    // maksymalna liczba cofniec

signals:
    void loadedChanged();
    void errorChanged();
    void mapChanged();   // dane mapy sie zmienily (po edycji)
    void dirtyChanged();
    void filePathChanged();

private:
    void reset();
    void setError(const QString &message);
    void setDirty(bool d);

    bool parseRootHeader(BinaryNode &root);
    bool parseMapData(BinaryNode &mapData);
    void parseTileArea(BinaryNode &area);
    void parseTile(BinaryNode &tile, uint16_t baseX, uint16_t baseY, uint8_t baseZ);
    OtbmMapItem parseItem(BinaryNode &itemNode);
    void parseTowns(BinaryNode &townsNode);
    void parseWaypoints(BinaryNode &waypointsNode);

    int countItems(const OtbmMapItem &item) const;

    void rebuildPosIndex();
    OtbmTile *getOrCreateTileRaw(int x, int y, int z); // bez undo (load spawns.xml)
    OtbmTile *tileForSpawnEdit(int x, int y, int z);   // kafel pod spawn (tworzy + undo)

    // spawns.xml (format RME/TFS): <spawn centerx/y/z radius> z potworami na
    // OFFSETACH wzgledem centrum. Sciezka wzgledem katalogu mapy (ExtSpawnFile).
    void loadSpawnsXml(const QString &mapPath);
    bool saveSpawnsXml(const QString &mapPath);
    // houses.xml (format RME/TFS) - jak spawny, sciezka z ExtHouseFile.
    void loadHousesXml(const QString &mapPath);
    bool saveHousesXml(const QString &mapPath);
    OtbmHouse *houseById(int id);

    // Snapshot stanu kafelka (przed zmiana) dla undo.
    // flags = strefy kafla (PZ/No-PvP/...). BEZ nich undo cofalo tylko itemy, a
    // malowanie strefy zostawalo na mapie.
    // flags = strefy; spawn/creature = spawny. Bez ktoregokolwiek undo cofaloby
    // itemy, a malowanie stref/spawnow zostawalo na mapie (bylo tak ze strefami).
    struct TileSnapshot {
        int x, y, z;
        uint32_t flags = 0;
        int spawn_radius = 0;
        QString creature_name;
        int creature_spawntime = 60;
        bool creature_is_npc = false;
        bool is_house = false;      // domy - ten sam wzorzec co strefy/spawny
        uint32_t house_id = 0;
        std::vector<OtbmMapItem> items;
    };
    struct UndoAction { std::vector<TileSnapshot> tiles; };
    void recordTile(int x, int y, int z);    // zapamietaj stan kafelka przed edycja
    void pushUndo(UndoAction &&action);
    void restoreSnapshot(const TileSnapshot &snap);
    // Biezacy stan kafelka jako snapshot (dla budowy akcji przeciwnej przy undo/redo).
    TileSnapshot currentSnapshot(int x, int y, int z) const;

    static quint64 posKey3d(int x, int y, int z) {
        return (static_cast<quint64>(static_cast<uint32_t>(z)) << 48)
             | (static_cast<quint64>(static_cast<uint32_t>(y)) << 24)
             | static_cast<uint32_t>(x);
    }

    std::deque<OtbmTile> m_tiles;
    QHash<quint64, int> m_posIndex; // (x,y,z) -> indeks w m_tiles

    std::deque<UndoAction> m_undoStack;
    std::deque<UndoAction> m_redoStack;   // cofniete akcje do ponowienia (Redo)
    std::vector<EditPos> m_lastAffected;  // kafle dotkniete przez ostatnie undo/redo
    int m_undoLimit = 500;
    bool m_undoGrouping = false;
    UndoAction m_currentGroup;
    QSet<quint64> m_groupRecorded;  // pozycje juz zapamietane w biezacej grupie
    std::vector<OtbmTown> m_towns;
    std::vector<OtbmWaypoint> m_waypoints;
    std::vector<OtbmHouse> m_houses;   // metadane domow (houses.xml)

    uint32_t m_otbmVersion = 0;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
    uint32_t m_otbItemsMajor = 0;
    uint32_t m_otbItemsMinor = 0;
    QString m_description;
    QString m_spawnFile;
    QString m_houseFile;
    int m_itemCount = 0;

    bool m_loaded = false;
    QString m_errorString;
    bool m_dirty = false;
    QString m_filePath;   // sciezka dokumentu (load/save); pusta dla nowej mapy
};

#endif // OTBMREADER_H
