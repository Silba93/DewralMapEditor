// MapView - czesc EDYCYJNA: malowanie pedzlem (surowym i ground brushem z
// auto-borderami), stawianie/przenoszenie/usuwanie itemow, undo/redo,
// batchowanie edycji i punktowe odswiezanie chunkow po zmianach.
#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QGuiApplication>
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

void MapView::undo()
{
    bool ok;
    {   // undo modyfikuje kafelki - pod lockiem (watek roboczy je czyta)
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ok = m_otbm && m_otbm->undo();
        if (ok) refreshUndoRedoTilesLocked();
    }
    if (ok) {
        m_spawnMarksDirty = true;   // snapshot mogl przywrocic/zdjac spawn
        emit contentUpdated(); update();
    }
}

void MapView::redo()
{
    bool ok;
    {   // redo modyfikuje kafelki - pod lockiem (watek roboczy je czyta)
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ok = m_otbm && m_otbm->redo();
        if (ok) refreshUndoRedoTilesLocked();
    }
    if (ok) {
        m_spawnMarksDirty = true;   // jak undo - snapshot mogl dotknac spawnow
        emit contentUpdated(); update();
    }
}

void MapView::refreshUndoRedoTilesLocked()
{
    // Punktowe, SYNCHRONICZNE odswiezenie dotknietych kafli - jak przy zwyklej edycji.
    // onTileEdited dopisuje ew. nowe kafle do indeksu i zaznacza chunki; flush przelicza
    // je od razu (nie asynchronicznie), wiec zmiana jest widoczna natychmiast, a nie
    // dopiero po kolejnym kliknieciu (stary bug: render trzymal nieaktualny VBO chunka
    // dopoki watek roboczy go nie przeliczyl, a to budzenie renderu bywalo zawodne).
    for (const OtbmReader::EditPos &p : m_otbm->lastAffected())
        onTileEdited(p.x, p.y, p.z);
    flushEditedChunksLocked();
}

MapView::~MapView() { stopWorker(); }

void MapView::refreshAfterEdit(uint16_t serverId)
{
    Q_UNUSED(serverId);
    // BEZ rebuildFloorIndex i BEZ przebudowy atlasu! Wskazniki kafelkow stabilne
    // (deque), indeks zaktualizowal onTileEdited(), a atlas jest przyrostowy ze
    // STABILNYMI slotami (nowe sprite'y dolozone przed edycja w placeItemAt) - wiec
    // istniejace bufory pozostaja wazne (zero "smieci na kazdym kafelku").
    emit contentUpdated(); update();
}

// UWAGA: wolane z trzymanym m_dataMutex (modyfikuje indeks/czyta kafelki).
void MapView::onTileEdited(int x, int y, int z)
{
    const int cx = floorDiv(x, kChunkTiles);
    const int cy = floorDiv(y, kChunkTiles);
    const quint64 ck = chunkKey(cx, cy);

    // Dopisz wskaznik do indeksu statycznego, jesli to nowy kafelek (deque =>
    // istniejace wskazniki pozostaja wazne, bez skanu calej mapy).
    // Test obecnosci przez m_floorChunkTileSet, a NIE std::find po wektorze chunka:
    // to samo pytanie, ale O(1) zamiast O(kafli w chunku) - patrz komentarz w mapview.h.
    if (const OtbmTile *tile = m_otbm ? m_otbm->tileAt(x, y, z) : nullptr) {
        const quint64 pk = posKey(x, y);
        if (!m_floorChunkTileSet[z].contains(pk)) {
            m_floorChunkTileSet[z].insert(pk);
            m_floorChunkTiles[z][ck].push_back(tile);   // kolejnosc push_back zachowana
        }
        if (z == m_floor)
            m_currentFloorTiles.insert(posKey(x, y), tile);
    }
    // UWAGA: dirty markerow spawnow NIE jest ustawiane tutaj - onTileEdited leci przy
    // KAZDEJ edycji (malowanie = setki/s), a rebuild markerow iteruje cale pietro.
    // Dirty ustawiaja wylacznie miejsca realnie zmieniajace spawny (place/clear/undo).
    // Swiatlo: usun z cache tylko chunki dotkniete tym kaflem (nie caly widok).
    // invalidateLightAround sam sprawdza czy oswietlenie w ogole wlaczone.
    invalidateLightAround(x, y, z);
    // Minimapa: punktowa aktualizacja piksela edytowanego kafla (tania - sam
    // sprawdza pietro i bbox).
    minimapUpdateTile(x, y, z);

    // Oznacz chunk jako oczekujacy przeliczenia (NIE przeliczaj tu!). Przy duzym
    // pedzlu wiele kafelkow z rzedu trafia w TEN SAM chunk - bez batchowania
    // przeliczalibysmy go wielokrotnie (np. 529x dla pedzla 23x23 = zabojca FPS).
    // Faktyczne przeliczenie robi flushEditedChunksLocked() (raz na chunk, wolane
    // przez endEditBatch lub bezposrednio z placeItemAt gdy poza batchem).
    m_pendingChunkRecompute.insert({z, ck});
}

void MapView::flushEditedChunksLocked()
{
    if (m_pendingChunkRecompute.empty()) return;
    for (const auto &zc : m_pendingChunkRecompute) {
        std::vector<QuadRef> quads;
        collectFloorChunkQuads(zc.first, zc.second, quads);
        storeChunkQuads(zc.first, zc.second, std::move(quads));
        m_dirtyFloorChunks[zc.first].insert(zc.second);
    }
    m_pendingChunkRecompute.clear();
    ++m_dataVersion;   // tresc sie zmienila -> MapGLView przebuduje bufor instancji
}

void MapView::endEditBatch()
{
    if (--m_editBatchDepth > 0) return;
    m_editBatchDepth = 0;   // guard przed nieprawidlowym zagniezdzeniem (ujemna glebokosc)
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    flushEditedChunksLocked();
}

int MapView::itemCategory(uint16_t serverId) const
{
    const int cid = m_otb ? m_otb->clientIdForServerId(serverId) : 0;
    const ClientItem *ci = (m_dat && cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    if (!ci) return 2;
    if (ci->is_ground) return 0;     // podloga - osobny "slot" na dole
    if (ci->is_on_bottom) return 1;  // border / always-on-bottom
    return 2;                        // normalny / always-on-top (na wierzchu)
}

void MapView::placeItemAt(int x, int y, int serverId)
{
    placeItemOnFloor(x, y, m_floor, serverId);
}

void MapView::placeItemOnFloor(int x, int y, int z, int serverId)
{
    if (serverId <= 0) return;
    OtbmMapItem item;
    item.server_id = static_cast<uint16_t>(serverId);
    placeItemOnFloor(x, y, z, item);   // swiezy item: count=1, bez atrybutow
}

void MapView::placeItemOnFloor(int x, int y, int z, const OtbmMapItem &src)
{
    if (!m_otbm || src.server_id == 0) return;

    const uint16_t sid = src.server_id;
    const int cat = itemCategory(sid);
    const OtbmTile *tile = m_otbm->tileAt(x, y, z);

    int index = 0;
    bool replace = false;

    if (!tile) {
        // Nowy kafelek - item staje sie pierwszy (na dole).
        index = 0;
    } else if (cat == 0) {
        // Ground: ZASTAP istniejacy ground (nie nakladaj na obiekty nad nim).
        int groundIdx = -1;
        for (size_t i = 0; i < tile->items.size(); ++i) {
            if (itemCategory(tile->items[i].server_id) == 0) { groundIdx = static_cast<int>(i); break; }
        }
        if (groundIdx >= 0) { index = groundIdx; replace = true; }
        else { index = 0; }
    } else if (cat == 1) {
        // onBottom (border/dywanik/stol itp.): wstaw wg OTB TopOrder (atrybut 0x2B),
        // 1:1 z RME Tile::addItem - mniejszy TopOrder = blizej podlogi. Przy rownym
        // TopOrder nowy item ląduje PO istniejacych (stabilnie). Bez tego wiele
        // itemow onBottom na jednym kafelku (np. rozne kawalki muru/bordera)
        // ukladalo sie w kolejnosci wstawiania zamiast wg TopOrder - stad zle
        // nakladajace sie fragmenty widoczne po malowaniu/edycji.
        const int newTopOrder = m_otb ? m_otb->topOrderForServerId(sid) : 0;
        index = static_cast<int>(tile->items.size());
        for (size_t i = 0; i < tile->items.size(); ++i) {
            const int otherCat = itemCategory(tile->items[i].server_id);
            if (otherCat == 0) continue;                            // ground - pomijamy
            if (otherCat >= 2) { index = static_cast<int>(i); break; } // trafil na normalny -> stop
            const int otherTopOrder = m_otb ? m_otb->topOrderForServerId(tile->items[i].server_id) : 0;
            if (newTopOrder < otherTopOrder) { index = static_cast<int>(i); break; }
        }
    } else {
        // normalny / onTop: dopisz na wierzch.
        index = static_cast<int>(tile->items.size());
    }

    bool placed;
    {   // m_dataMutex: atlas + edycja kafelka + indeksu, gdy watek roboczy czyta
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(sid);   // dodaj sprite'y do atlasu PRZED przeliczeniem chunka
        placed = m_otbm->placeItem(x, y, z, src, index, replace, cat == 0);
        if (placed) {
            onTileEdited(x, y, z);   // aktualizacja indeksu, oznacza chunk do przeliczenia
            // Poza batchem (pojedynczy klik/Paste): przelicz od razu, jak dawniej.
            // W batchu (paintAt z duzym pedzlem): flush robi endEditBatch - raz na chunk.
            if (m_editBatchDepth == 0) flushEditedChunksLocked();
        }
    }
    if (placed) {
        // Efekt magiczny zalezy tylko od m_placeEffect (feedback surowego pedzla).
        if (m_placeEffect)
            m_activeEffects.push_back({x, y, m_floor, m_effectClock.elapsed()});
        // Odswiezenie widoku POMIJAMY w trybie masowym (malowanie linia) - robi je
        // paintAt raz na cale zdarzenie myszy zamiast raz na kafel = plynniej.
        if (!m_bulkEdit) refreshAfterEdit(sid);
    }
}

void MapView::paintZoneAt(int cx, int cy)
{
    if (!m_otbm || m_activeZone == 0) return;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    bool any = false;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int tx = cx + dx, ty = cy + dy;
            const uint32_t cur = m_otbm->tileFlags(tx, ty, m_floor);
            const uint32_t next = m_eraseStroke ? (cur & ~m_activeZone) : (cur | m_activeZone);
            if (m_otbm->setTileFlags(tx, ty, m_floor, next)) {
                onTileEdited(tx, ty, m_floor);   // flagi siedza w quadach -> przelicz chunk
                any = true;
            }
        }
    if (any && m_editBatchDepth == 0) flushEditedChunksLocked();
}

void MapView::eraseAt(int cx, int cy)
{
    if (!m_otbm) return;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    // Zdejmij wierzchni item; zapamietaj CO zniknelo, zeby wiedziec co przeliczyc.
    QSet<QString> touchedWalls;
    bool touchedGround = false;
    for (const auto &p : footprint) {
        // Gumka: kafel czyszczony RAZ na pociagniecie - rewizyta (wezykowanie) nie
        // powinna zdejmowac kolejnych warstw stosu.
        const quint64 pk = posKey(p.first, p.second);
        if (m_strokePlaced.contains(pk)) continue;
        m_strokePlaced.insert(pk);
        // Gumka czysci tez potwory i centra spawnow (jak RME eraser).
        bool clearedSpawn = false;
        if (m_otbm->clearCreatureAt(p.first, p.second, m_floor)) clearedSpawn = true;
        if (m_otbm->clearSpawnAt(p.first, p.second, m_floor)) { clearedSpawn = true; m_spawnMarksDirty = true; }
        if (clearedSpawn) onTileEdited(p.first, p.second, m_floor);
        const OtbmTile *t = currentFloorTileAt(p.first, p.second);
        if (!t || t->items.empty()) continue;
        const uint16_t top = t->items.back().server_id;
        if (m_brushStore) {
            const QString wn = m_brushStore->wallBrushForServerId(top);
            if (!wn.isEmpty()) touchedWalls.insert(wn);
        }
        if (itemCategory(top) == 0) touchedGround = true;   // znikla podloga -> bordery
        if (m_otbm->removeTopItem(p.first, p.second, m_floor))
            onTileEdited(p.first, p.second, m_floor);
    }

    // Skutki: footprint + sasiedzi (bordery i sciany sa wzajemne).
    QSet<quint64> around;
    for (const auto &p : footprint)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                around.insert(posKey(p.first + dx, p.second + dy));

    // Bordery: oddajemy do wspolnego zbioru - paintAt przeliczy je RAZ na zdarzenie
    // (przy duzym pedzlu i zoom-out gumka miala ten sam problem co malowanie).
    if (touchedGround && m_brushStore && m_brushStore->hasData())
        m_strokeBorderTiles.unite(around);
    // Sciany zostawiamy tu: zaleza od nazwy brusha, ktora jest lokalna dla tego kafla.
    for (const QString &wn : touchedWalls)
        for (quint64 a : around)
            recomputeWallAt(static_cast<int>(a >> 32), static_cast<int>(a & 0xffffffffu), wn);

    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
}

void MapView::paintFootprint(int x, int y)
{
    // Pedzel STREFY (PZ / No-PvP / No-Logout / PvP) - osobna sciezka, nie stawia itemow.
    // paintZoneAt sam patrzy na m_eraseStroke (stawia albo kasuje flage).
    if (m_activeZone != 0) {
        paintZoneAt(x, y);
        return;
    }

    // Pedzel domu PRZED gumka: Ctrl/Erase z aktywnym domem zdejmuje kafle DOMU
    // (RME undraw), a nie itemy. placeHouseAt sam patrzy na m_eraseStroke.
    if (m_houseBrush > 0) {
        placeHouseAt(x, y);
        return;
    }

    // GUMKA (tryb Erase albo Ctrl) - kasuje itemy zamiast stawiac.
    if (m_eraseStroke) {
        eraseAt(x, y);
        return;
    }

    // Pedzel spawnu / potwora - punktowe, przed sciezkami itemowymi.
    if (m_spawnBrush) {
        placeSpawnAt(x, y);
        return;
    }
    if (!m_creatureBrush.isEmpty()) {
        placeCreatureBrushAt(x, y);
        return;
    }

    // Ground brush (grass/sand/...): stawianie z auto-borderami - osobna sciezka.
    if (!m_activeGroundBrush.isEmpty() && m_brushStore && m_brushStore->hasData()) {
        paintGroundBrushAt(x, y);
        return;
    }

    // Wall brush (auto-laczenie scian) - osobna sciezka (RME WallBrush::doWalls).
    if (!m_activeWallBrush.isEmpty() && m_brushStore && m_brushStore->hasWallData()) {
        paintWallBrushAt(x, y);
        return;
    }

    // Doodad brush (losowe warianty / stemple wielokaflowe) - osobna sciezka.
    if (!m_activeDoodadBrush.isEmpty() && m_brushStore && m_brushStore->hasDoodadData()) {
        paintDoodadBrushAt(x, y);
        return;
    }

    // Maluj caly footprint pedzla (kwadrat/okrag o promieniu brushSize, jak RME).
    // Efekt magiczny tylko na srodkowym kafelku (przy 19x19 bylby spamem 361 animacji).
    // m_strokePlaced: kazdy kafel obslugiwany RAZ na pociagniecie - bez tego rewizyta
    // (wezykowanie) DOKLADALA kolejna kopie itemu na stos kafla (patrz mapview.h).
    const bool savedFx = m_placeEffect;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) {
                const quint64 pk = posKey(x + dx, y + dy);
                if (m_strokePlaced.contains(pk)) continue;
                m_strokePlaced.insert(pk);
                m_placeEffect = savedFx && dx == 0 && dy == 0;   // efekt tylko na srodku
                placeItemAt(x + dx, y + dy, m_brushServerId);
            }
    m_placeEffect = savedFx;
}

void MapView::setCreatureBrush(const QString &name)
{
    if (m_creatureBrush == name) return;
    m_creatureBrush = name;
    if (!name.isEmpty()) {
        // Wyklucz pozostale pedzle (jak w RME - jeden aktywny pedzel).
        applyBrushServerId(0, false);
        m_spawnBrush = false;
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        // Sprite'y outfitu do atlasu PRZED pierwszym postawieniem.
        if (m_creatureStore && m_dat) {
            if (const auto *ct = m_creatureStore->byName(name)) {
                std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
                ensureOutfitSprites(ct->lookType);
            }
        }
        setCursor(Qt::CrossCursor);
    } else if (m_brushServerId <= 0 && !m_spawnBrush) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setSpawnBrush(bool on)
{
    if (m_spawnBrush == on) return;
    m_spawnBrush = on;
    if (on) {
        applyBrushServerId(0, false);
        m_creatureBrush.clear();
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    } else if (m_brushServerId <= 0 && m_creatureBrush.isEmpty()) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setHouseBrush(int id)
{
    if (id < 0) id = 0;
    if (m_houseBrush == id) return;
    m_houseBrush = id;
    if (id > 0) {
        applyBrushServerId(0, false);
        m_creatureBrush.clear();
        m_spawnBrush = false;
        m_houseExitMode = false;
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    } else if (m_brushServerId <= 0 && m_creatureBrush.isEmpty() && !m_spawnBrush) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setHouseExitMode(bool on)
{
    if (m_houseExitMode == on) return;
    m_houseExitMode = on;
    if (on) {
        // Tryb wejscia potrzebuje aktywnego domu (m_houseBrush) - nie czyscimy go,
        // tylko pozostale pedzle.
        applyBrushServerId(0, false);
        m_creatureBrush.clear();
        m_spawnBrush = false;
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::placeHouseAt(int x, int y)
{
    if (m_houseBrush <= 0) return;

    // Tryb wejscia: klik ustawia entry aktywnego domu (bez malowania kafli).
    if (m_houseExitMode) {
        m_otbm->setHouseEntry(m_houseBrush, x, y, m_floor);
        emit contentUpdated(); update();
        return;
    }

    // Malowanie kafli domu footprintem pedzla (jak strefy). Gumka/Ctrl zdejmuje.
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int tx = x + dx, ty = y + dy;
            const quint64 pk = posKey(tx, ty);
            if (m_strokePlaced.contains(pk)) continue;
            m_strokePlaced.insert(pk);
            const bool ok = m_eraseStroke ? m_otbm->clearHouseTileAt(tx, ty, m_floor)
                                          : m_otbm->setHouseTileAt(tx, ty, m_floor,
                                                static_cast<uint32_t>(m_houseBrush));
            if (ok) onTileEdited(tx, ty, m_floor);
        }
    if (m_editBatchDepth == 0) flushEditedChunksLocked();
    if (!m_bulkEdit) refreshAfterEdit(0);
}

void MapView::ensureOutfitSprites(int lookType)
{
    if (lookType <= 0 || m_ensuredOutfits.contains(lookType)) return;
    m_ensuredOutfits.insert(lookType);
    const ClientItem *of = m_dat ? m_dat->outfitByLookType(static_cast<uint16_t>(lookType))
                                 : nullptr;
    if (!of) return;
    QSet<uint32_t> sids;
    for (uint32_t sid : of->sprite_ids) if (sid != 0) sids.insert(sid);
    addSpritesToAtlas(sids);
}

bool MapView::tileInAnySpawn(int x, int y) const
{
    // Promien spawnu jest KWADRATOWY (tak zapisuje XML: offsety w [-r..r]).
    // Po liscie centrow pietra (kilka-kilkadziesiat wpisow), NIE skanem 31x31
    // kafli - 961 hash-lookupow per malowany kafel zabijalo FPS przy malowaniu.
    auto *self = const_cast<MapView *>(this);
    if (m_spawnMarksDirty) self->rebuildSpawnMarks();
    for (const SpawnCenter &c : m_spawnCentersFloor)
        if (std::max(std::abs(x - c.x), std::abs(y - c.y)) <= c.r)
            return true;
    return false;
}

void MapView::placeSpawnAt(int x, int y)
{
    // Promien z dedykowanego pola palety (nie z rozmiaru pedzla) - jawny i widoczny.
    const int radius = m_spawnBrushRadius;
    const quint64 pk = posKey(x, y);
    if (m_strokePlaced.contains(pk)) return;
    m_strokePlaced.insert(pk);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    // Nowe centrum = tani append; nadpisanie istniejacego (zmiana promienia) = pelny
    // rebuild (append zostawilby stary obrys).
    const OtbmTile *before = m_otbm->tileAt(x, y, m_floor);
    const bool wasCenter = before && before->spawn_radius > 0;
    if (m_otbm->setSpawnAt(x, y, m_floor, radius)) {
        if (wasCenter) m_spawnMarksDirty = true;
        else appendSpawnMark(x, y, radius);
        onTileEdited(x, y, m_floor);
        if (m_editBatchDepth == 0) flushEditedChunksLocked();
        if (!m_bulkEdit) refreshAfterEdit(0);
    }
}

void MapView::placeCreatureBrushAt(int x, int y)
{
    if (!m_creatureStore) return;
    const CreatureStore::CreatureType *ct = m_creatureStore->byName(m_creatureBrush);
    if (!ct) return;

    const quint64 pk = posKey(x, y);
    if (m_strokePlaced.contains(pk)) return;
    m_strokePlaced.insert(pk);

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    ensureOutfitSprites(ct->lookType);
    // RME: potwor poza zasiegiem jakiegokolwiek spawnu dostaje wlasny spawn 1x1
    // (inaczej zapis by go zgubil - XML trzyma potwory wewnatrz <spawn>).
    // appendSpawnMark zamiast dirty: dirty wymuszaloby pelny rebuild markerow,
    // a tileInAnySpawn NASTEPNEGO kafla pociagniecia by go natychmiast wykonal
    // (= rebuild calego pietra per postawiony potwor).
    if (!tileInAnySpawn(x, y)) {
        if (m_otbm->setSpawnAt(x, y, m_floor, 1)) appendSpawnMark(x, y, 1);
    }
    if (m_otbm->setCreatureAt(x, y, m_floor, ct->name, m_creatureSpawntime, ct->isNpc)) {
        onTileEdited(x, y, m_floor);
        if (m_editBatchDepth == 0) flushEditedChunksLocked();
        if (!m_bulkEdit) refreshAfterEdit(0);
    }
}

bool MapView::brushCanDrag() const
{
    // 1:1 z RME (brush.h): TerrainBrush (ground/sciany/dywany/stoly), RAWBrush,
    // FlagBrush (strefy) i EraserBrush maja canDrag()==true. DoodadBrush ma false.
    // Potwory/spawny: pojedyncze punkty, prostokat nie ma sensu.
    if (!m_creatureBrush.isEmpty() || m_spawnBrush) return false;
    // Dom: prostokat bardzo wygodny (pokoje sa prostokatne); tryb wejscia - punktowy.
    if (m_houseBrush > 0) return !m_houseExitMode;
    if (!m_activeDoodadBrush.isEmpty()) return false;   // sprawdzaj PRZED server-id
    if (m_activeZone != 0 || m_eraseMode) return true;
    return m_brushServerId > 0 || !m_activeGroundBrush.isEmpty() || !m_activeWallBrush.isEmpty();
}

void MapView::cleanManagedBordersAt(int x, int y)
{
    const OtbmTile *t = currentFloorTileAt(x, y);
    if (!t || t->items.empty()) return;
    std::vector<uint16_t> ids;
    for (const OtbmMapItem &it : t->items)
        if (m_brushStore->isManagedBorderItem(it.server_id))
            ids.push_back(it.server_id);
    if (ids.empty()) return;
    if (m_otbm->removeItemsById(x, y, m_floor, ids) > 0)
        onTileEdited(x, y, m_floor);
}

void MapView::drawDragRect(int x0, int y0, int x1, int y1)
{
    if (!m_otbm) return;

    // RME normalizuje rogi przed petla (last_click > mouse -> swap), wiec ciagniecie
    // w dowolna strone daje ten sam prostokat.
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    const int savedSize = m_brushSize;
    m_bulkEdit = true;
    m_placeEffect = false;   // przy duzym prostokacie efekt na kazdym kaflu = spam
    // W RME prostokat JEST obszarem: tilestodraw budowane wylacznie z jego granic, a
    // pedzel klada sie raz na kafel. Rozmiar pedzla nie ma tu znaczenia - stad 0.
    m_brushSize = 0;
    m_otbm->beginUndoGroup();   // caly prostokat = jedno cofniecie

    // Znamy z gory liczbe kafli - reserve oszczedza kilkanascie rehashow przy setkach
    // tysiecy insertow (kazdy rehash QHash/QSet kopiuje cala tablice).
    const int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    m_strokePlaced.reserve(m_strokePlaced.size() + area);
    m_currentFloorTiles.reserve(m_currentFloorTiles.size() + area);
    m_floorChunkTileSet[m_floor].reserve(m_floorChunkTileSet[m_floor].size() + area);

    const bool groundFill = !m_activeGroundBrush.isEmpty() && m_brushStore
                            && m_brushStore->hasData() && !m_eraseMode && m_activeZone == 0;
    m_dragFillActive = groundFill;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            paintFootprint(x, y);
    m_dragFillActive = false;

    // Bordery jak RME (tilestoborder przy dragging_draw): tylko GRANICA prostokata +
    // zewnetrzny pierscien. Wnetrze wypelnione jednym groundem nie ma borderow z
    // definicji - pelny recompute per kafel wnetrza (QStringi 9 sasiadow) potrafil
    // kosztowac ~0.7s przy 100k kafli. Wnetrzu wystarczy sprzatniecie osieroconych
    // borderow po STARYM terenie (tani skan itemow kafla).
    if (groundFill && m_automagic) {
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;   // jak w paintAt - sasiedzi granicy sie pokrywaja
        for (int y = y0 - 1; y <= y1 + 1; ++y)
            for (int x = x0 - 1; x <= x1 + 1; ++x)
                if (x <= x0 || x >= x1 || y <= y0 || y >= y1)
                    recomputeBordersAt(x, y);
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        for (int y = y0 + 1; y <= y1 - 1; ++y)
            for (int x = x0 + 1; x <= x1 - 1; ++x)
                cleanManagedBordersAt(x, y);
        m_strokeBorderTiles.clear();   // nic nie zbieralismy, ale badz spojny
    } else if (!m_strokeBorderTiles.isEmpty()) {
        // Inne pedzle (erase na groundzie itp.) - zbior zebrany po drodze, jak dotad.
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        for (quint64 p : m_strokeBorderTiles)
            recomputeBordersAt(static_cast<int>(p >> 32), static_cast<int>(p & 0xffffffffu));
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        m_strokeBorderTiles.clear();
    }

    m_otbm->endUndoGroup();
    m_brushSize = savedSize;
    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(static_cast<uint16_t>(m_brushServerId));
}

void MapView::paintAt(int x, int y)
{
    // Malujemy gdy jest JAKIKOLWIEK pedzel: itemu, strefy, gumka, spawn, potwor, dom.
    if (!m_otbm || (m_brushServerId <= 0 && m_activeZone == 0 && !m_eraseMode
                    && !m_spawnBrush && m_creatureBrush.isEmpty()
                    && m_houseBrush <= 0)) return;
    if (x == m_paintLastX && y == m_paintLastY) return; // nie dubluj na tym samym kafelku

    // Caly odcinek (od poprzedniego kafla do biezacego) w jednym batchu i jednym
    // odswiezeniu - przy szybkim ruchu myszy kursor przeskakuje kilka kafli miedzy
    // zdarzeniami, wiec interpolujemy linie (Bresenham), zeby pedzel klad sie CIAGLE
    // bez luk (jak w RME). m_bulkEdit tlumi per-footprint refresh - jedno na koniec.
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    m_bulkEdit = true;
    // m_strokePlaced NIE jest czyszczone tutaj - dedup obejmuje CALE pociagniecie
    // (czysci mousePressEvent). Bordery zbieramy per-zdarzenie.
    m_strokeBorderTiles.clear();

    if (m_paintLastX > -1000000) {
        int x0 = m_paintLastX, y0 = m_paintLastY;
        const int dx = std::abs(x - x0), dy = std::abs(y - y0);
        const int sx = x0 < x ? 1 : -1, sy = y0 < y ? 1 : -1;
        int err = dx - dy, cx = x0, cy = y0;
        while (cx != x || cy != y) {   // od kafla PO poprzednim do biezacego wlacznie
            const int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; cx += sx; }
            if (e2 <  dx) { err += dx; cy += sy; }
            paintFootprint(cx, cy);
        }
    } else {
        paintFootprint(x, y);   // pierwszy kafel pociagniecia
    }

    // Bordery RAZ na cale zdarzenie, na zdeduplikowanym zbiorze (patrz komentarz w
    // paintGroundBrushAt). Wczesniej liczylo sie to per-kafel-linii => O(linia x 625).
    if (!m_strokeBorderTiles.isEmpty()) {
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;   // wspoldzielone nazwy sasiadow licza sie raz
        for (quint64 p : m_strokeBorderTiles)
            recomputeBordersAt(static_cast<int>(p >> 32), static_cast<int>(p & 0xffffffffu));
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        m_strokeBorderTiles.clear();
    }

    m_bulkEdit = savedBulk;
    m_paintLastX = x;
    m_paintLastY = y;
    endEditBatch();
    refreshAfterEdit(static_cast<uint16_t>(m_brushServerId));
}

int MapView::groundServerIdAt(const OtbmTile *tile) const
{
    if (!tile) return 0;
    for (const OtbmMapItem &it : tile->items)
        if (itemCategory(it.server_id) == 0) return it.server_id;
    return 0;
}

QString MapView::groundBrushNameAt(int x, int y) const
{
    if (!m_brushStore) return QString();
    // Cache aktywny tylko w przebiegu borderow (patrz mapview.h przy m_groundNameCache).
    if (m_groundNameCacheOn) {
        const quint64 pk = posKey(x, y);
        auto it = m_groundNameCache.constFind(pk);
        if (it != m_groundNameCache.cend()) return it.value();
        const int sid = groundServerIdAt(currentFloorTileAt(x, y));
        QString n = sid > 0 ? m_brushStore->groundBrushForServerId(sid) : QString();
        m_groundNameCache.insert(pk, n);
        return n;
    }
    const int sid = groundServerIdAt(currentFloorTileAt(x, y));
    return sid > 0 ? m_brushStore->groundBrushForServerId(sid) : QString();
}

void MapView::recomputeBordersAt(int x, int y)
{
    if (!m_brushStore || !m_otbm) return;
    // "Border Automagic" (A) wylaczony = grunt klada sie bez krawedzi. Menu "Borderize
    // Selection/Map" woli borderizeSelection()/borderizeMap(), ktore ustawiaja flage
    // tymczasowo, wiec dzialaja nawet przy wylaczonym automagicu (jak RME).
    if (!m_automagic) return;

    const QString center = groundBrushNameAt(x, y);
    // 8 sasiadow w kolejnosci RME: 0=NW 1=N 2=NE 3=W 4=E 5=SW 6=S 7=SE.
    static const int dxs[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dys[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    QStringList neighbours;
    neighbours.reserve(8);
    for (int i = 0; i < 8; ++i)
        neighbours << groundBrushNameAt(x + dxs[i], y + dys[i]);

    // Jednolity teren (kafel i wszyscy sasiedzi z tym samym brushem, lacznie z
    // "wszystko puste") z definicji nie ma borderow - pomijamy computeBorderItems
    // (QStringi + tablice przejsc). To NAJCZESTSZY przypadek przy malowaniu po
    // wnetrzu wlasnego terenu, wiec early-out realnie tnie koszt pociagniecia.
    // Dalsza czesc (porownanie ze starymi borderami) musi zostac: stare bordery
    // po POPRZEDNIM terenie trzeba sprzatnac nawet gdy nowych nie ma.
    bool uniform = true;
    for (const QString &n : neighbours)
        if (n != center) { uniform = false; break; }

    QVector<int> newBorders;
    if (!uniform) {
        newBorders = m_brushStore->computeBorderItems(center, neighbours);
        // RME stack order: doBorders przetwarza od najwyzszego z-order i robi
        // addBorderItem = insert na POCZATEK items[] (tuz nad groundem), wiec ostatni
        // wstawiony (najnizszy z-order) laduje najnizej, a najwyzszy z-order na wierzchu
        // stosu borderow. computeBorderItems zwraca od najwyzszego z-order -> odwracamy,
        // by wstawiac od najnizszego (najnizej) do najwyzszego (na wierzchu).
        std::reverse(newBorders.begin(), newBorders.end());
    }

    // "cleanBorders": obecne kafle bordera na kaflu (zarzadzane przez silnik), w
    // kolejnosci stosu - do porownania "czy cos sie zmienilo".
    const OtbmTile *tile = currentFloorTileAt(x, y);
    std::vector<uint16_t> oldBorders;
    if (tile) {
        for (const OtbmMapItem &it : tile->items)
            if (m_brushStore->isManagedBorderItem(it.server_id))
                oldBorders.push_back(it.server_id);
    }

    // Bez zmian (te same idy w tej samej kolejnosci)? Nie ruszaj kafelka - unikamy
    // zbednych snapshotow undo i przeliczen chunkow.
    {
        std::vector<uint16_t> b;
        b.reserve(newBorders.size());
        for (int id : newBorders) b.push_back(static_cast<uint16_t>(id));
        if (oldBorders == b) return;
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!oldBorders.empty())
        m_otbm->removeItemsById(x, y, m_floor, oldBorders);

    // Wstawiamy bordery WPROST (nie przez placeItemAt) tuz nad groundem, w kolejnosci
    // z-order - inaczej placeItemAt ukladalby je wg OTB TopOrder (male 0..3), co NIE
    // odzwierciedla z-order terenu i psuloby nakladanie (np. gravel gor pod trawa).
    const OtbmTile *t2 = currentFloorTileAt(x, y);
    int base = (t2 && !t2->items.empty() && itemCategory(t2->items[0].server_id) == 0) ? 1 : 0;
    for (int k = 0; k < newBorders.size(); ++k) {
        const int id = newBorders[k];
        if (id <= 0) continue;
        ensureItemSprites(static_cast<uint16_t>(id));
        m_otbm->placeItem(x, y, m_floor, static_cast<uint16_t>(id), base + k, false, false);
    }
    onTileEdited(x, y, m_floor);
}

void MapView::paintGroundBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    // 1. Zbierz footprint pedzla.
    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    // BEZ wlasnej undo group ani wlasnego refresh - to obsluguje wywolujacy: press/
    // release trzyma jedno cofniecie na cale pociagniecie, a paintAt robi jeden batch
    // + jedno odswiezenie na cale zdarzenie myszy (zagniezdzenie beginUndoGroup tu
    // resetowaloby grupe pociagniecia w srodku = cofanie po jednym kaflu).
    beginEditBatch();
    const bool savedFx = m_placeEffect;   // bez efektu magicznego przy ground brushu
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;                    // placeItemAt bez per-item refreshAfterEdit

    // 2. Postaw losowy ground brusha na kafelkach footprintu (zastap istniejacy ground).
    //    m_strokePlaced: kafel malujemy RAZ na zdarzenie myszy - footprinty sasiednich
    //    kafli linii mocno zachodza, wiec bez tego ten sam kafel bylby przemalowywany
    //    (i za kazdym razem losowal inny wariant) kilkanascie razy.
    for (const auto &p : footprint) {
        const quint64 pk = posKey(p.first, p.second);
        if (m_strokePlaced.contains(pk)) continue;   // juz malowany w tym zdarzeniu
        m_strokePlaced.insert(pk);
        const int id = m_brushStore->pickGroundItem(m_activeGroundBrush);
        if (id > 0) placeItemAt(p.first, p.second, id);

        // 3. Bordery sa wzajemne (kafel grass obok sand potrzebuje krawedzi i odwrotnie),
        //    wiec zbieramy kafel + 8 sasiadow. Przeliczenie robi paintAt RAZ na koniec
        //    zdarzenia - inaczej nakladajace sie footprinty liczyly to samo wielokrotnie
        //    (przy zoom-out linia ma dziesiatki kafli => spadek FPS).
        //    Przy wypelnianiu prostokata (m_dragFillActive) pomijamy - drawDragRect
        //    liczy bordery tylko na granicy prostokata (wnetrze jednolite).
        if (!m_dragFillActive)
            for (int ddy = -1; ddy <= 1; ++ddy)
                for (int ddx = -1; ddx <= 1; ++ddx)
                    m_strokeBorderTiles.insert(posKey(p.first + ddx, p.second + ddy));
    }

    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
}

void MapView::paintDoodadBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    const QString name = m_activeDoodadBrush;

    // Footprint pedzla - na kazdym kaflu losujemy OSOBNO, dzieki czemu wieksze pedzle
    // rozsiewaja rozne warianty (jak RME przy doodadach z <alternate>).
    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    // Bez wlasnej undo group/refresh - obsluguje wywolujacy (press/release + paintAt).
    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    for (const auto &p : footprint) {
        // Rewizyta kafla w tym samym pociagnieciu stackowalaby kolejne doodady.
        const quint64 pk = posKey(p.first, p.second);
        if (m_strokePlaced.contains(pk)) continue;
        m_strokePlaced.insert(pk);
        // Wariant wybrany (R) => deterministyczny; -1 => losowy scatter (jak dotad).
        const QVector<BrushStore::DoodadTile> tiles =
            m_doodadVariant >= 0 ? m_brushStore->doodadVariantTiles(name, m_doodadVariant)
                                 : m_brushStore->pickDoodad(name);
        for (const BrushStore::DoodadTile &t : tiles) {
            const int tx = p.first + t.dx, ty = p.second + t.dy;
            const int tz = m_floor + t.dz;          // wodospad/rampa: czesci na innych pietrach
            if (tz < 0 || tz > 15) continue;
            // Itemy kafla od dolu do gory; placeItemOnFloor uklada wg kategorii/TopOrder.
            for (int id : t.items) placeItemOnFloor(tx, ty, tz, id);
        }
    }

    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
}

bool MapView::tileHasWallBrush(int x, int y, const QString &name) const
{
    if (!m_brushStore || name.isEmpty()) return false;
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile) return false;
    for (const OtbmMapItem &it : tile->items)
        if (m_brushStore->wallBrushForServerId(it.server_id) == name)
            return true;
    return false;
}

void MapView::recomputeWallAt(int x, int y, const QString &name)
{
    if (!m_brushStore || !m_otbm || name.isEmpty()) return;
    // Tylko kafle ktore JUZ maja sciane tego brusha - na pustych sasiadach nic nie
    // tworzymy (jak RME doWalls dziala na istniejacych sciankach).
    if (!tileHasWallBrush(x, y, name)) return;

    // Sasiedzi ortogonalni (N/W/E/S) - czy maja sciane tego brusha (maska w BrushStore).
    const bool n = tileHasWallBrush(x, y - 1, name);
    const bool w = tileHasWallBrush(x - 1, y, name);
    const bool e = tileHasWallBrush(x + 1, y, name);
    const bool s = tileHasWallBrush(x, y + 1, name);
    const int newId = m_brushStore->computeWallItem(name, n, w, e, s);
    if (newId <= 0) return;

    // Obecne kafle sciany tego brusha na kaflu (do podmiany).
    const OtbmTile *tile = currentFloorTileAt(x, y);
    std::vector<uint16_t> oldWalls;
    if (tile)
        for (const OtbmMapItem &it : tile->items)
            if (m_brushStore->wallBrushForServerId(it.server_id) == name)
                oldWalls.push_back(it.server_id);

    // Dokladnie jeden kafel sciany i to juz newId? Nie ruszaj (mniej snapshotow undo).
    if (oldWalls.size() == 1 && oldWalls[0] == static_cast<uint16_t>(newId)) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!oldWalls.empty())
        m_otbm->removeItemsById(x, y, m_floor, oldWalls);
    placeItemAt(x, y, newId);   // kategoria 1 (onBottom) - placeItemAt uklada wg TopOrder
}

void MapView::paintWallBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    const QString name = m_activeWallBrush;

    // Footprint pedzla (sciany zwykle rysuje sie promieniem 0, ale respektujemy rozmiar).
    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    // Bez wlasnej undo group/refresh - obsluguje wywolujacy (press/release + paintAt).
    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    // 1. Postaw marker sciany (pole) na kaflach footprintu, ktore jej nie maja - zeby
    //    recomputeWallAt (i przeliczenie sasiadow) widzialo tu juz sciane.
    const int pole = m_brushStore->wallPoleItem(name);
    for (const auto &p : footprint)
        if (pole > 0 && !tileHasWallBrush(p.first, p.second, name))
            placeItemAt(p.first, p.second, pole);

    // 2. Przelicz wyrownanie na footprincie + 4 sasiadach ortogonalnych (unikalny zbior).
    //    Sciany lacza sie wzajemnie: nowy kafel zmienia tez ksztalt sasiadow.
    std::set<std::pair<int, int>> toWall;
    static const int dxs[4] = { 0, -1, 1, 0 };
    static const int dys[4] = { -1, 0, 0, 1 };
    for (const auto &p : footprint) {
        toWall.insert(p);
        for (int i = 0; i < 4; ++i)
            toWall.insert({ p.first + dxs[i], p.second + dys[i] });
    }
    for (const auto &p : toWall)
        recomputeWallAt(p.first, p.second, name);

    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
}

bool MapView::setContextSpawnRadius(int radius)
{
    if (!m_otbm) return false;
    radius = std::clamp(radius, 1, 15);
    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
        if (!t || t->spawn_radius <= 0) return false;   // tylko istniejace centrum
        if (t->spawn_radius == radius) return false;
        changed = m_otbm->setSpawnAt(m_contextX, m_contextY, m_floor, radius);
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);   // markery do przebudowy
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::setContextCreatureSpawntime(int seconds)
{
    if (!m_otbm) return false;
    seconds = std::clamp(seconds, 1, 86400);
    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
        if (!t || t->creature_name.isEmpty()) return false;
        if (t->creature_spawntime == seconds) return false;
        // setCreatureAt nadpisuje caly komplet - nazwa/npc bez zmian, nowy czas.
        changed = m_otbm->setCreatureAt(m_contextX, m_contextY, m_floor,
                                        t->creature_name, seconds, t->creature_is_npc);
    }
    return changed;
}

bool MapView::setContextItemCount(int count)
{
    if (!m_otbm) return false;
    // Tibia: sterta ma max 100 sztuk, a warianty sprite'a koncza sie na progu 50+.
    count = std::clamp(count, 1, 100);

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
        if (!t || t->items.empty()) return false;

        // Tylko stackowalne - dla reszty OTBM i tak trzyma 1 i zmiana nic nie znaczy.
        const int cid = m_otb ? m_otb->clientIdForServerId(t->items.back().server_id) : 0;
        const ClientItem *ci = (m_dat && cid > 0)
                                   ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
        if (!ci || !ci->is_stackable) return false;

        changed = m_otbm->setTopItemCount(m_contextX, m_contextY, m_floor,
                                          static_cast<uint16_t>(count));
        if (changed) {
            // Count wybiera wariant sprite'a (cellSpriteId), wiec chunk musi sie przeliczyc.
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::applyContextItemProperties(const QVariantMap &props)
{
    if (!m_otbm) return false;

    bool changed = false;
    bool spriteDirty = false;   // count wybiera wariant sprite'a -> chunk do przeliczenia
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        // Cala zawartosc okna = JEDNA akcja undo (jak OK w RME).
        m_otbm->beginUndoGroup();

        if (props.contains(QStringLiteral("actionId"))) {
            const int v = std::clamp(props.value(QStringLiteral("actionId")).toInt(), 0, 65535);
            changed |= m_otbm->setTopItemActionId(m_contextX, m_contextY, m_floor,
                                                  static_cast<uint16_t>(v));
        }
        if (props.contains(QStringLiteral("uniqueId"))) {
            const int v = std::clamp(props.value(QStringLiteral("uniqueId")).toInt(), 0, 65535);
            changed |= m_otbm->setTopItemUniqueId(m_contextX, m_contextY, m_floor,
                                                  static_cast<uint16_t>(v));
        }
        if (props.contains(QStringLiteral("text"))) {
            changed |= m_otbm->setTopItemText(m_contextX, m_contextY, m_floor,
                                              props.value(QStringLiteral("text")).toString());
        }
        if (props.value(QStringLiteral("teleportClear")).toBool()) {
            changed |= m_otbm->setTopItemTeleport(m_contextX, m_contextY, m_floor, -1, -1, -1);
        } else if (props.contains(QStringLiteral("teleportX"))) {
            changed |= m_otbm->setTopItemTeleport(
                m_contextX, m_contextY, m_floor,
                props.value(QStringLiteral("teleportX")).toInt(),
                props.value(QStringLiteral("teleportY")).toInt(),
                props.value(QStringLiteral("teleportZ")).toInt());
        }
        if (props.contains(QStringLiteral("count"))) {
            // Tylko stackowalne - dla reszty OTBM i tak trzyma 1 (jak w RME, gdzie
            // pole Count jest wtedy wyszarzone).
            const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
            const int cid = (t && !t->items.empty() && m_otb)
                                ? m_otb->clientIdForServerId(t->items.back().server_id) : 0;
            const ClientItem *ci = (m_dat && cid > 0)
                                       ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
            if (ci && ci->is_stackable) {
                const int v = std::clamp(props.value(QStringLiteral("count")).toInt(), 1, 100);
                if (m_otbm->setTopItemCount(m_contextX, m_contextY, m_floor,
                                            static_cast<uint16_t>(v))) {
                    changed = true;
                    spriteDirty = true;
                }
            }
        }

        m_otbm->endUndoGroup();

        if (spriteDirty) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

// Atrybuty wierzchniego itemu (okno Properties). Wszystkie ida tym samym torem:
// mutacja w OtbmReader (z undo) + odswiezenie widoku. Sprite'a nie zmieniaja
// (inaczej niz count), wiec bez onTileEdited/flush - wystarczy repaint.
bool MapView::setContextItemActionId(int actionId)
{
    if (!m_otbm) return false;
    const int v = std::clamp(actionId, 0, 65535);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setTopItemActionId(m_contextX, m_contextY, m_floor,
                                      static_cast<uint16_t>(v));
}

bool MapView::setContextItemUniqueId(int uniqueId)
{
    if (!m_otbm) return false;
    const int v = std::clamp(uniqueId, 0, 65535);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setTopItemUniqueId(m_contextX, m_contextY, m_floor,
                                      static_cast<uint16_t>(v));
}

bool MapView::setContextItemText(const QString &text)
{
    if (!m_otbm) return false;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setTopItemText(m_contextX, m_contextY, m_floor, text);
}

bool MapView::setContextItemTeleport(int destX, int destY, int destZ)
{
    if (!m_otbm) return false;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setTopItemTeleport(m_contextX, m_contextY, m_floor,
                                      destX, destY, destZ);
}

QVariantMap MapView::contextInfo() const
{
    QVariantMap m;
    m.insert(QStringLiteral("x"), m_contextX);
    m.insert(QStringLiteral("y"), m_contextY);
    m.insert(QStringLiteral("z"), m_floor);
    m.insert(QStringLiteral("selectionCount"), m_selected.size());

    const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
    const bool has = tile && !tile->items.empty();
    m.insert(QStringLiteral("hasItem"), has);
    // Spawny: potwor/centrum na klikanym kaflu (Properties pokazuje i edytuje).
    m.insert(QStringLiteral("creatureName"), tile ? tile->creature_name : QString());
    m.insert(QStringLiteral("creatureSpawntime"), tile ? tile->creature_spawntime : 0);
    m.insert(QStringLiteral("spawnRadius"), tile ? tile->spawn_radius : 0);
    if (has) {
        const OtbmMapItem &top = tile->items.back();
        const int cid = m_otb ? m_otb->clientIdForServerId(top.server_id) : 0;
        const ClientItem *ci = (m_dat && cid > 0)
                                   ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
        m.insert(QStringLiteral("serverId"), top.server_id);
        m.insert(QStringLiteral("clientId"), cid);
        m.insert(QStringLiteral("name"), m_otb ? m_otb->nameForServerId(top.server_id) : QString());
        // Grupa z items.otb - po niej serwer poznaje teleport/kontener/drzwi.
        m.insert(QStringLiteral("groupName"),
                 m_otb ? m_otb->groupNameForServerId(top.server_id) : QString());
        // Count ma sens tylko dla stackowalnych - dla reszty OTBM i tak trzyma 1.
        m.insert(QStringLiteral("stackable"), ci && ci->is_stackable);
        m.insert(QStringLiteral("count"), top.count);
        m.insert(QStringLiteral("actionId"), top.action_id);
        m.insert(QStringLiteral("uniqueId"), top.unique_id);
        // Tekst: pole pokazujemy dla itemow zapisywalnych wg .dat (znaki, ksiazki)
        // albo gdy tekst juz jest (mapa moze go niesc na dowolnym itemie).
        const QString text = top.extra ? top.extra->text : QString();
        m.insert(QStringLiteral("text"), text);
        m.insert(QStringLiteral("writable"), (ci && ci->is_writable) || !text.isEmpty());
        // Teleport: pole celu dla itemow grupy Teleport z OTB (jak RME) albo gdy
        // cel juz istnieje.
        const bool isTele = m_otb && m_otb->isTeleportItem(top.server_id);
        const bool hasTele = top.extra && top.extra->has_teleport;
        m.insert(QStringLiteral("teleport"), isTele || hasTele);
        m.insert(QStringLiteral("hasTeleportDest"), hasTele);
        m.insert(QStringLiteral("teleportX"), hasTele ? top.extra->tele_x : 0);
        m.insert(QStringLiteral("teleportY"), hasTele ? top.extra->tele_y : 0);
        m.insert(QStringLiteral("teleportZ"), hasTele ? top.extra->tele_z : 0);
    } else {
        m.insert(QStringLiteral("serverId"), 0);
        m.insert(QStringLiteral("clientId"), 0);
        m.insert(QStringLiteral("name"), QString());
        m.insert(QStringLiteral("groupName"), QString());
        m.insert(QStringLiteral("stackable"), false);
        m.insert(QStringLiteral("count"), 0);
        m.insert(QStringLiteral("actionId"), 0);
        m.insert(QStringLiteral("uniqueId"), 0);
        m.insert(QStringLiteral("text"), QString());
        m.insert(QStringLiteral("writable"), false);
        m.insert(QStringLiteral("teleport"), false);
        m.insert(QStringLiteral("hasTeleportDest"), false);
        m.insert(QStringLiteral("teleportX"), 0);
        m.insert(QStringLiteral("teleportY"), 0);
        m.insert(QStringLiteral("teleportZ"), 0);
    }
    return m;
}

void MapView::deleteSelectedTop()
{
    if (!m_otbm || m_selected.isEmpty()) return;
    bool any = false;
    {   // m_dataMutex wokol calej grupy edycji (watek roboczy czyta kafelki)
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        m_otbm->beginUndoGroup(); // usuniecie z wielu kafelkow = jedno cofniecie
        for (quint64 key : m_selected) {
            const int x = selX(key), y = selY(key), z = selZ(key);
            // Od wierzchu: potwor (rysowany na stosie) -> centrum spawnu -> top item.
            if (m_otbm->clearCreatureAt(x, y, z)) { any = true; onTileEdited(x, y, z); continue; }
            if (m_otbm->clearSpawnAt(x, y, z))    { any = true; m_spawnMarksDirty = true; onTileEdited(x, y, z); continue; }
            if (m_otbm->removeTopItem(x, y, z)) { any = true; onTileEdited(x, y, z); }
        }
        m_otbm->endUndoGroup();
        flushEditedChunksLocked();   // jeden przelicz na dotkniety chunk (nie na kafelek)
    }
    if (any) refreshAfterEdit(0);
}

void MapView::copySelection()
{
    if (!m_otbm || m_selected.isEmpty()) return;

    // Lewy-gorny rog zaznaczenia = punkt odniesienia dla offsetow.
    bool first = true;
    int minX = 0, minY = 0;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key);
        if (first) { minX = x; minY = y; first = false; }
        else { minX = std::min(minX, x); minY = std::min(minY, y); }
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    m_clipboard.clear();
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *t = m_otbm->tileAt(x, y, z);
        if (!t) continue;
        ClipTile ct;
        ct.dx = x - minX;
        ct.dy = y - minY;
        // Pietro wzgledem AKTYWNEGO przy kopiowaniu - wklejanie odtwarza strukture
        // pieter wokol pietra kursora (jak RME).
        ct.dz = z - m_floor;
        // Region (Shift+drag) = caly stos + potwor + spawn; pojedynczy grab = wierzch
        // (potwor ma pierwszenstwo - jak przy przenoszeniu).
        if (m_selWholeStack) {
            ct.items = t->items;                       // kopia z count/aid/uid/zawartoscia
            ct.creature = t->creature_name;
            ct.spawntime = t->creature_spawntime;
            ct.npc = t->creature_is_npc;
            ct.spawnRadius = t->spawn_radius;
        } else if (!t->creature_name.isEmpty()) {
            ct.creature = t->creature_name;
            ct.spawntime = t->creature_spawntime;
            ct.npc = t->creature_is_npc;
        } else if (!t->items.empty()) {
            ct.items.push_back(t->items.back());       // tylko wierzch
        }
        if (ct.items.empty() && ct.creature.isEmpty() && ct.spawnRadius == 0) continue;
        m_clipboard.push_back(std::move(ct));
    }
    emit clipboardChanged();
}

void MapView::cutSelection()
{
    copySelection();
    if (m_clipboard.empty() || m_selected.isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();          // cale wyciecie = jedno cofniecie
    bool any = false;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        bool removedHere = false;
        if (m_selWholeStack) {
            // Region: czysci CALY kafel (od wierzchu do groundu wlacznie) + potwora
            // i centrum spawnu (wyciecie regionu zabiera wszystko).
            while (m_otbm->removeTopItem(x, y, z)) { removedHere = true; any = true; }
            if (m_otbm->clearCreatureAt(x, y, z)) { removedHere = true; any = true; }
            if (m_otbm->clearSpawnAt(x, y, z)) { removedHere = true; any = true; m_spawnMarksDirty = true; }
        } else {
            // Pojedynczy: wierzch (potwor przed itemem, jak przy Delete).
            if (m_otbm->clearCreatureAt(x, y, z)) { removedHere = true; any = true; }
            else if (m_otbm->removeTopItem(x, y, z)) { removedHere = true; any = true; }
        }
        if (removedHere) onTileEdited(x, y, z);
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    if (any) refreshAfterEdit(0);
}

void MapView::moveSelection(int dx, int dy)
{
    if (!m_otbm || m_selected.isEmpty() || (dx == 0 && dy == 0)) return;

    // Co przenosimy z kazdego kafla: CALY stos (Shift+drag = region) albo tylko
    // WIERZCH (pojedynczy grab, jak RME selection.add(tile,getTopItem())) - a
    // wierzchem kafla z potworem jest POTWOR, nie item. Region bierze tez potwora
    // i centrum spawnu. Snapshot trzyma PELNE itemy (count/aid/uid przezywaja move).
    struct Snap {
        int x, y, z;
        std::vector<OtbmMapItem> items;
        QString creature; int spawntime = 60; bool npc = false;
        int spawnRadius = 0;
    };
    std::vector<Snap> snap;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *t = m_otbm->tileAt(x, y, z);
        if (!t) continue;
        Snap s; s.x = x; s.y = y; s.z = z;
        if (m_selWholeStack) {
            s.items = t->items;
            s.creature = t->creature_name;
            s.spawntime = t->creature_spawntime;
            s.npc = t->creature_is_npc;
            s.spawnRadius = t->spawn_radius;
        } else if (!t->creature_name.isEmpty()) {
            // Pojedynczy chwyt: potwor jest "na wierzchu" - przenies tylko jego.
            s.creature = t->creature_name;
            s.spawntime = t->creature_spawntime;
            s.npc = t->creature_is_npc;
        } else if (!t->items.empty()) {
            s.items.push_back(t->items.back());   // tylko wierzchni item
        } else if (t->spawn_radius > 0) {
            s.spawnRadius = t->spawn_radius;      // kafel z samym centrum spawnu
        }
        if (s.items.empty() && s.creature.isEmpty() && s.spawnRadius == 0) continue;
        snap.push_back(std::move(s));
    }
    if (snap.empty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();          // cale przeniesienie = jedno cofniecie

    // Zdejmij co bierzemy (itemy/potwora/spawn), postaw na celu.
    // Snapshot zdjety wczesniej, wiec overlap zrodlo/cel jest bezpieczny.
    bool movedSpawn = false;
    for (const Snap &s : snap) {
        for (size_t i = 0; i < s.items.size(); ++i) m_otbm->removeTopItem(s.x, s.y, s.z);
        if (!s.creature.isEmpty()) m_otbm->clearCreatureAt(s.x, s.y, s.z);
        if (s.spawnRadius > 0) { m_otbm->clearSpawnAt(s.x, s.y, s.z); movedSpawn = true; }
        onTileEdited(s.x, s.y, s.z);
    }
    QSet<quint64> newSel;
    for (const Snap &s : snap) {
        const int nx = s.x + dx, ny = s.y + dy;   // delta myszy jest 2D; pietro zostaje
        // od dolu (ground) do wierzchu; pelny item, wiec count/aid/uid przezywaja move
        for (const OtbmMapItem &it : s.items) placeItemOnFloor(nx, ny, s.z, it);
        if (!s.creature.isEmpty()) {
            m_otbm->setCreatureAt(nx, ny, s.z, s.creature, s.spawntime, s.npc);
            onTileEdited(nx, ny, s.z);
        }
        if (s.spawnRadius > 0) {
            m_otbm->setSpawnAt(nx, ny, s.z, s.spawnRadius);
            onTileEdited(nx, ny, s.z);
        }
        newSel.insert(selKey(nx, ny, s.z));
    }
    if (movedSpawn) m_spawnMarksDirty = true;   // centra zmienily pozycje

    m_otbm->endUndoGroup();
    m_bulkEdit = savedBulk;
    m_placeEffect = savedFx;
    endEditBatch();
    m_selected = newSel;
    notifySelectionChanged();
    refreshAfterEdit(0);
}

// --- Menu "Select" (jak RME): operacje hurtowe na zaznaczeniu ---------------------

void MapView::borderizeSelection()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData() || m_selected.isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedAuto = m_automagic;
    m_bulkEdit = true;
    m_automagic = true;              // jawne "borderize" dziala mimo wylaczonego automagicu
    m_otbm->beginUndoGroup();        // cala operacja = jedno cofniecie
    for (quint64 key : m_selected) {
        // Silnik borderow pracuje na BIEZACYM pietrze (groundBrushNameAt/currentFloor-
        // TileAt) - kafle zaznaczone na innych pietrach pomijamy.
        if (selZ(key) != m_floor) continue;
        recomputeBordersAt(selX(key), selY(key));
    }
    m_otbm->endUndoGroup();
    m_automagic = savedAuto;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

void MapView::borderizeMap()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData()) return;

    // Zbierz pozycje PRZED edycja - recomputeBordersAt modyfikuje m_tiles (insert/erase),
    // wiec iterowanie po tiles() w trakcie byloby niebezpieczne.
    std::vector<std::pair<int, int>> positions;
    positions.reserve(m_otbm->tiles().size());
    for (const OtbmTile &t : m_otbm->tiles())
        if (t.z == m_floor) positions.push_back({ t.x, t.y });   // biezace pietro

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedAuto = m_automagic;
    m_bulkEdit = true;
    m_automagic = true;
    m_otbm->beginUndoGroup();
    for (const auto &p : positions) recomputeBordersAt(p.first, p.second);
    m_otbm->endUndoGroup();
    m_automagic = savedAuto;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

void MapView::randomizeMap()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData()) return;

    std::vector<std::pair<int, int>> positions;
    positions.reserve(m_otbm->tiles().size());
    for (const OtbmTile &t : m_otbm->tiles())
        if (t.z == m_floor) positions.push_back({ t.x, t.y });

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (const auto &p : positions) {
        const QString bn = groundBrushNameAt(p.first, p.second);
        if (bn.isEmpty()) continue;
        const int id = m_brushStore->pickGroundItem(bn);
        if (id > 0) placeItemAt(p.first, p.second, id);
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

int MapView::replaceItemsOnMap(int fromId, int toId)
{
    if (!m_otbm || fromId <= 0 || toId <= 0 || fromId == toId) return 0;
    int n;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(static_cast<uint16_t>(toId));
        n = m_otbm->replaceItemsOnMap(static_cast<uint16_t>(fromId), static_cast<uint16_t>(toId));
        if (n > 0) refreshUndoRedoTilesLocked();   // odswieza kafle z lastAffected()
    }
    if (n > 0) refreshAfterEdit(0);
    return n;
}

int MapView::removeItemsOnMap(int serverId)
{
    if (!m_otbm || serverId <= 0) return 0;
    int n;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        n = m_otbm->removeItemsOnMap(static_cast<uint16_t>(serverId));
        if (n > 0) refreshUndoRedoTilesLocked();
    }
    if (n > 0) refreshAfterEdit(0);
    return n;
}

void MapView::centerOnPosition(int x, int y, int z)
{
    // Zapamietaj skad skaczemy - "Go to Previous Position" (P) wraca tutaj.
    m_prevCenterX = static_cast<int>(m_originX + width() / (2.0 * std::max(1, m_tileSize)));
    m_prevCenterY = static_cast<int>(m_originY + height() / (2.0 * std::max(1, m_tileSize)));
    m_prevCenterZ = m_floor;
    m_prevCenterValid = true;

    if (z >= 0 && z <= 15 && z != m_floor) setFloor(z);
    const double ts = std::max(1, m_tileSize);
    m_originX = x - width() / (2.0 * ts);
    m_originY = y - height() / (2.0 * ts);
    emit contentUpdated(); update();
}

bool MapView::goToPreviousPosition()
{
    if (!m_prevCenterValid) return false;
    const int x = m_prevCenterX, y = m_prevCenterY, z = m_prevCenterZ;
    centerOnPosition(x, y, z);   // zapisze BIEZACA pozycje jako poprzednia -> dziala jak toggle
    return true;
}

bool MapView::jumpToItemOnMap(int serverId)
{
    if (!m_otbm || serverId <= 0) return false;
    const QVariantMap pos = m_otbm->findFirstItemOnMap(serverId);
    if (pos.isEmpty()) return false;
    centerOnPosition(pos.value(QStringLiteral("x")).toInt(),
                     pos.value(QStringLiteral("y")).toInt(),
                     pos.value(QStringLiteral("z")).toInt());
    return true;
}

void MapView::randomizeSelection()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData() || m_selected.isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (quint64 key : m_selected) {
        // Silnik brushy pracuje na BIEZACYM pietrze - inne pietra pomijamy (jak borderize).
        if (selZ(key) != m_floor) continue;
        const int x = selX(key), y = selY(key);
        // Tylko kafle nalezace do ground brusha - reszte RME pomija (nie ma czego losowac).
        const QString bn = groundBrushNameAt(x, y);
        if (bn.isEmpty()) continue;
        const int id = m_brushStore->pickGroundItem(bn);
        if (id > 0) placeItemAt(x, y, id);   // placeItemAt ZASTEPUJE istniejacy ground
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

int MapView::countItemOnSelection(int serverId) const
{
    if (!m_otbm || m_selected.isEmpty() || serverId <= 0) return 0;
    int n = 0;
    for (quint64 key : m_selected) {
        // countItemsOnTile schodzi w kontenery - inaczej licznik gubi wszystko,
        // co siedzi w torbach/skrzyniach na zaznaczonych kaflach.
        n += m_otbm->countItemsOnTile(selX(key), selY(key), selZ(key), serverId);
    }
    return n;
}

int MapView::removeItemOnSelection(int serverId)
{
    if (!m_otbm || m_selected.isEmpty() || serverId <= 0) return 0;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    const std::vector<uint16_t> ids{ static_cast<uint16_t>(serverId) };
    int n = 0;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const int c = m_otbm->removeItemsById(x, y, z, ids, /*deep=*/true);
        if (c > 0) { n += c; onTileEdited(x, y, z); }
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    if (n > 0) refreshAfterEdit(0);
    return n;
}

int MapView::replaceItemsOnSelection(int fromId, int toId)
{
    if (!m_otbm || m_selected.isEmpty() || fromId <= 0 || toId <= 0 || fromId == toId) return 0;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    ensureItemSprites(static_cast<uint16_t>(toId));   // nowy item moze nie byc w atlasie
    beginEditBatch();
    m_otbm->beginUndoGroup();
    int n = 0;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const int c = m_otbm->replaceItemsById(x, y, z, static_cast<uint16_t>(fromId),
                                               static_cast<uint16_t>(toId));
        if (c > 0) { n += c; onTileEdited(x, y, z); }
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    if (n > 0) refreshAfterEdit(0);
    return n;
}

void MapView::startPasting()
{
    if (!m_otbm || m_clipboard.empty()) return;
    {   // Sprite'y schowka moga nie byc w atlasie (skopiowane z innego pietra/obszaru),
        // a bez nich podglad bylby pusty. Atlas jest przyrostowy - to bezpieczne.
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        for (const ClipTile &ct : m_clipboard)
            for (const OtbmMapItem &ci : ct.items) ensureItemSprites(ci.server_id);
    }
    m_pasting = true;
    setCursor(Qt::CrossCursor);
    emit pastingChanged();
    emit contentUpdated(); update();
}

void MapView::cancelPasting()
{
    if (!m_pasting) return;
    m_pasting = false;
    setCursor((m_brushServerId > 0 || m_activeZone != 0 || m_eraseMode) ? Qt::CrossCursor
                                                                        : Qt::ArrowCursor);
    emit pastingChanged();
    emit contentUpdated(); update();
}

void MapView::commitPasteAt(int px, int py)
{
    if (!m_otbm || m_clipboard.empty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;                 // placeItemAt bez per-item refresh
    m_placeEffect = false;
    m_otbm->beginUndoGroup();          // cale wklejenie = jedno cofniecie
    bool pastedSpawn = false;
    for (const ClipTile &ct : m_clipboard) {
        const int tx = px + ct.dx, ty = py + ct.dy;
        // Pietro docelowe = biezace + offset z chwili kopiowania (multi-floor paste).
        const int tz = std::clamp(m_floor + ct.dz, 0, 15);
        // Itemy w oryginalnej kolejnosci stosu; placeItemOnFloor sam uklada wg kategorii
        // (ground zastepuje istniejacy, onBottom wg TopOrder, reszta na wierzch).
        // Pelny item - wklejony gold coin zachowuje count, kontener zawartosc.
        for (const OtbmMapItem &ci : ct.items)
            placeItemOnFloor(tx, ty, tz, ci);
        if (!ct.creature.isEmpty()) {
            m_otbm->setCreatureAt(tx, ty, tz, ct.creature, ct.spawntime, ct.npc);
            onTileEdited(tx, ty, tz);
        }
        if (ct.spawnRadius > 0) {
            m_otbm->setSpawnAt(tx, ty, tz, ct.spawnRadius);
            onTileEdited(tx, ty, tz);
            pastedSpawn = true;
        }
    }
    if (pastedSpawn) m_spawnMarksDirty = true;
    m_otbm->endUndoGroup();
    m_bulkEdit = savedBulk;
    m_placeEffect = savedFx;
    endEditBatch();
    refreshAfterEdit(0);
}

