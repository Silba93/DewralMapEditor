// MapView - czesc CHUNKOW: statyczny indeks [pietro][chunk] -> kafelki, budowa
// quadow (pozycja+slot atlasu) per chunk, cache quadow i watek roboczy liczacy
// je w tle (async background loading).
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
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

void MapView::buildStaticIndex()
{
    m_floorChunkTiles.clear();
    m_floorChunkTileSet.clear();   // MUSI isc razem - to indeks tego samego zbioru
    if (!m_otbm || !m_otbm->isLoaded()) return;

    // RAZ (wczytanie / edycja): rozdziel WSZYSTKIE kafelki po [z][chunk].
    for (const OtbmTile &tile : m_otbm->tiles()) {
        const int cx = floorDiv(tile.x, kChunkTiles);
        const int cy = floorDiv(tile.y, kChunkTiles);
        m_floorChunkTiles[tile.z][chunkKey(cx, cy)].push_back(&tile);
        m_floorChunkTileSet[tile.z].insert(posKey(tile.x, tile.y));
    }
}

void MapView::updateCurrentFloor()
{
    // Szybkie (zmiana pietra): lookup + bbox biezacego pietra - bez skanu calej mapy.
    m_spawnMarksDirty = true;   // markery spawnow dotycza biezacego pietra
    m_currentFloorTiles.clear();
    m_minTileX = m_minTileY = m_maxTileX = m_maxTileY = 0;
    if (!m_otbm || !m_otbm->isLoaded()) return;

    bool first = true;
    auto zit = m_floorChunkTiles.find(m_floor);
    if (zit != m_floorChunkTiles.end()) {
        for (auto cit = zit->begin(); cit != zit->end(); ++cit) {
            for (const OtbmTile *tile : cit.value()) {
                m_currentFloorTiles.insert(posKey(tile->x, tile->y), tile);
                if (first) { m_minTileX = m_maxTileX = tile->x; m_minTileY = m_maxTileY = tile->y; first = false; }
                else { m_minTileX = std::min<int>(m_minTileX, tile->x); m_maxTileX = std::max<int>(m_maxTileX, tile->x);
                       m_minTileY = std::min<int>(m_minTileY, tile->y); m_maxTileY = std::max<int>(m_maxTileY, tile->y); }
            }
        }
    }
}

void MapView::rebuildFloorIndex()
{
    {   // buildStaticIndex PISZE m_floorChunkTiles - watek roboczy go czyta
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        buildStaticIndex();
    }
    updateCurrentFloor();      // czyta indeks (zgodne z czytaniem watku) + pisze biezace
    clearChunkQuadCache();     // dane sie zmienily -> quady do przeliczenia w tle
    ++m_dataVersion;           // MapGLView przebuduje bufor instancji
    m_minimapFloor = -1;       // minimapa do przebudowy (nowa mapa/atlas)
    ++m_minimapVer;
}

bool MapView::chunkHasContent(quint64 key) const
{
    const int bottomZ = renderBottomFloor();
    for (int z = m_floor; z <= bottomZ; ++z) {
        auto zit = m_floorChunkTiles.find(z);
        if (zit != m_floorChunkTiles.end() && zit->contains(key)) return true;
    }
    return false;
}

// Atlas PRZYROSTOWY (append-only): nowe sprite'y dokladane na KONIEC, istniejace
// sloty NIGDY sie nie ruszaja. Dzieki temu postawienie nowego itemu nie unieważnia
// buforow (zapieczone sloty pozostaja wazne) - zero "smieci na kazdym kafelku".
// Wolane pod m_dataMutex (watek roboczy czyta atlas).
void MapView::appendItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const
{
    // Projekcja pieter (jak Tibia/RME): kazde pietro przesuniete ukosnie o 1 kafelek.
    // Surowe pozycje swiata (BEZ offsetu pietra). Offset ukosny per-pietro jest
    // aplikowany w MACIERZY wezla pietra (floorNode), nie w wierzcholkach - dzieki
    // temu mesh nie zalezy od biezacego pietra i nie wymaga przebudowy.
    // Indeks WIERZCHNIEGO renderowalnego itemu - jego quady dostaja topItem=true,
    // co pozwala tintowac przy zaznaczeniu tylko wierzch (jak RME), a nie caly kafel.
    int topIdx = -1;
    for (int i = static_cast<int>(tile->items.size()) - 1; i >= 0; --i) {
        const int cid = m_otb->clientIdForServerId(tile->items[static_cast<size_t>(i)].server_id);
        if (cid <= 0) continue;
        const ClientItem *c = m_dat->itemByClientId(static_cast<uint16_t>(cid));
        if (c && !c->sprite_ids.empty()) { topIdx = i; break; }
    }

    // Elevation (DatFlagElevation): item PODNOSI kolejne itemy lezace na nim (stol,
    // podest). RME robi to w BlitItem: rysuje sprite w draw_x, a POTEM draw_x -=
    // drawHeight, wiec akumuluje sie w gore stosu kafla (draw_x jest przez referencje).
    // Sam item NIE jest podnoszony przez wlasna elevation - tylko te nad nim.
    int elevation = 0;

    for (int idx = 0; idx < static_cast<int>(tile->items.size()); ++idx) {
        const OtbmMapItem &item = tile->items[static_cast<size_t>(idx)];
        const int clientId = m_otb->clientIdForServerId(item.server_id);
        if (clientId <= 0) continue;
        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(clientId));
        if (!ci || ci->sprite_ids.empty()) continue;

        const int w = std::max<int>(1, ci->width);
        const int h = std::max<int>(1, ci->height);
        const int layers = std::max<int>(1, ci->layers);
        const bool isGround = ci->is_ground;   // do LOD przy oddaleniu (jak RME)
        // Kafel z potworem: to POTWOR jest wierzchem (tint zaznaczenia przy
        // pojedynczym chwycie ma podswietlac jego, nie item pod nim).
        const bool isTop = (idx == topIdx) && tile->creature_name.isEmpty();

        // Displacement (DatFlagDisplacement): sprite rysowany o offset w gore/lewo -
        // tak siedza itemy na wieszakach, w oknach itp. (RME: screenx = draw_x - offset).
        const int ox = ci->has_offset ? ci->offset_x : 0;
        const int oy = ci->has_offset ? ci->offset_y : 0;

        // Klatka animacji z globalnego zegara (0 gdy animacje wylaczone) - patrz
        // itemFrame/setShowAnimations. Zapieczona w quadach; tick zegara uniewaznia
        // cache chunkow i quady przelicza sie z nowa klatka.
        const int fr = itemFrame(ci);

        for (int l = 0; l < layers; ++l)
            for (int hh = 0; hh < h; ++hh)
                for (int ww = 0; ww < w; ++ww) {
                    const uint32_t sid = cellSpriteId(ci, ww, hh, l, w, h, tile->x, tile->y,
                                                      tile->z, item.count, fr);
                    if (sid == 0) continue;
                    const int as = atlasSlotForSprite(sid);
                    if (as < 0) continue;
                    // Komorka (ww,hh) trafia na kafelek (x-ww, y-hh); kotwica zaznaczenia
                    // to kafel itemu (tile->x, tile->y). Tint TYLKO wierzchniego itemu.
                    out.push_back(QuadRef{
                        (tile->x - ww) * kSprite - ox - elevation,
                        (tile->y - hh) * kSprite - oy - elevation,
                        as, isGround, tile->x, tile->y, isTop,
                        // Strefy tintuja PODLOGE (jak RME) - dla reszty itemow 0. Bit 64
                        // (poza flagami OTBM 1-32) = dom: shader daje mu PIERWSZENSTWO
                        // nad zwyklym PZ tintem (niebieski zamiast zielonego) - 1:1 z RME
                        // map_drawer.cpp DrawTile (house tile ma inny tint niz "goly" PZ,
                        // mimo ze house brush ustawia PZ na kaflu - patrz setHouseTileAt).
                        // Tint strefy/domu na SPODZIE STOSU (idx==0), jak RME tintuje
                        // kafel - NIE na kazdym itemie z data-flaga is_ground: w 7.x
                        // sporo itemow (kamienne posadzki, platformy) ma te flage i
                        // lezac NAD prawdziwym groundem dostawaly drugi tint ("zielony
                        // brush na itemie"). Kafle BEZ itemow obsluguje osobna nakladka
                        // (glCollectZoneMarkInstances). Przelaczniki Show gasza u zrodla.
                        idx == 0 ? ((m_showZones ? static_cast<int>(tile->flags) : 0)
                                   | ((m_showHouses && tile->is_house) ? 64 : 0))
                                 : 0 });
                }

        // Po narysowaniu tego itemu podnies nastepne (0 gdy brak flagi elevation).
        if (ci->has_elevation) elevation += ci->elevation;
    }

    // Potwor/NPC na kaflu (spawny): sprite outfitu z .dat (kierunek poludnie,
    // warstwa bazowa bez barwienia template - MVP). Rysowany na wierzchu stosu,
    // podniesiony o skumulowana elevation jak itemy nad podestami.
    // m_showCreatures (Show > Show creatures): quady potwora w ogole nie powstaja.
    if (m_showCreatures && !tile->creature_name.isEmpty() && m_creatureStore && m_dat) {
        const CreatureStore::CreatureType *ct = m_creatureStore->byName(tile->creature_name);
        const ClientItem *of = (ct && ct->lookType > 0)
                                   ? m_dat->outfitByLookType(static_cast<uint16_t>(ct->lookType))
                                   : nullptr;
        if (of && !of->sprite_ids.empty()) {
            const int w = std::max<int>(1, of->width);
            const int h = std::max<int>(1, of->height);
            const int patX = std::max<int>(1, of->pattern_x);
            const int layers = std::max<int>(1, of->layers);
            const int dir = std::min(2, patX - 1);   // 2 = poludnie (patternX outfitu)
            for (int hh = 0; hh < h; ++hh)
                for (int ww = 0; ww < w; ++ww) {
                    const int idx = ((dir * layers + 0) * h + hh) * w + ww;
                    if (idx < 0 || idx >= static_cast<int>(of->sprite_ids.size())) continue;
                    const uint32_t sid = of->sprite_ids[static_cast<size_t>(idx)];
                    if (sid == 0) continue;
                    const int as = atlasSlotForSprite(sid);
                    if (as < 0) continue;
                    out.push_back(QuadRef{
                        (tile->x - ww) * kSprite - elevation,
                        (tile->y - hh) * kSprite - elevation,
                        as, false, tile->x, tile->y, /*topItem=*/true, 0 });
                }
        }
    }
}

void MapView::appendTopItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const
{
    // Tylko OSTATNI renderowalny item (na wierzchu). Selekcja dotyczy biezacego
    // pietra (offset 0), wiec surowe pozycje swiata - bez offsetu pietra.
    // Offset/elevation liczymy TAK SAMO jak appendItemQuads (elevation akumuluje sie
    // przez caly stos), inaczej duch przenoszenia odskoczylby od realnego sprite'a.
    std::vector<QuadRef> topQuads;
    int elevation = 0;

    for (const OtbmMapItem &item : tile->items) {
        const int clientId = m_otb->clientIdForServerId(item.server_id);
        if (clientId <= 0) continue;
        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(clientId));
        if (!ci || ci->sprite_ids.empty()) continue;

        const int w = std::max<int>(1, ci->width);
        const int h = std::max<int>(1, ci->height);
        const int layers = std::max<int>(1, ci->layers);
        const int ox = ci->has_offset ? ci->offset_x : 0;
        const int oy = ci->has_offset ? ci->offset_y : 0;
        const int elev = elevation;            // stan PRZED tym itemem (jak w RME)
        if (ci->has_elevation) elevation += ci->elevation;

        // Ta sama klatka co glowny przebieg - inaczej sylwetka zaznaczenia/duch
        // przenoszenia odklejalyby sie od animowanego sprite'a pod spodem.
        const int fr = itemFrame(ci);

        topQuads.clear();
        for (int l = 0; l < layers; ++l)
            for (int hh = 0; hh < h; ++hh)
                for (int ww = 0; ww < w; ++ww) {
                    const uint32_t sid = cellSpriteId(ci, ww, hh, l, w, h, tile->x, tile->y,
                                                      tile->z, item.count, fr);
                    if (sid == 0) continue;
                    const int as = atlasSlotForSprite(sid);
                    if (as < 0) continue;
                    topQuads.push_back(QuadRef{
                        (tile->x - ww) * kSprite - ox - elev,
                        (tile->y - hh) * kSprite - oy - elev,
                        as, ci->is_ground });
                }
    }

    for (const QuadRef &q : topQuads) out.push_back(q);
}

void MapView::collectFloorChunkQuads(int z, quint64 key, std::vector<QuadRef> &out)
{
    auto zit = m_floorChunkTiles.find(z);
    if (zit == m_floorChunkTiles.end()) return;
    auto cit = zit->find(key);
    if (cit == zit->end()) return;

    // Kolejnosc rysowania kafli = rosnace (y, x), jak RME (skanuje row-by-row).
    // Lista chunka jest w kolejnosci WSTAWIANIA (edycja robi push_back na koniec),
    // wiec bez sortu nowo postawiona sciana rysowalaby sie OSTATNIA i jej wystajacy
    // fragment (displacement w gore) zaslanialby sasiadow. Kafel z wiekszym y musi
    // isc PO mniejszym, zeby jego sprite poprawnie przykryl kafel nad nim. Sort
    // lokalny, tylko przy przeliczaniu chunka (max 1024 kafle) - nie co klatke.
    std::vector<const OtbmTile *> tiles(cit.value().begin(), cit.value().end());
    std::sort(tiles.begin(), tiles.end(),
              [](const OtbmTile *a, const OtbmTile *b) {
                  return a->y != b->y ? a->y < b->y : a->x < b->x;
              });
    for (const OtbmTile *tile : tiles)
        appendItemQuads(tile, out);
}

// --- Async: watek roboczy + cache gotowych quadow ----------------------------

void MapView::startWorker()
{
    m_workerStop = false;
    m_worker = std::thread([this] { workerLoop(); });
}

void MapView::stopWorker()
{
    {
        std::lock_guard<std::mutex> lk(m_reqMutex);
        m_workerStop = true;
    }
    m_reqCv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void MapView::workerLoop()
{
    for (;;) {
        std::pair<int, quint64> req;
        {
            std::unique_lock<std::mutex> lk(m_reqMutex);
            m_reqCv.wait(lk, [this] { return m_workerStop || !m_reqQueue.empty(); });
            if (m_workerStop) return;
            req = m_reqQueue.front();
            m_reqQueue.pop_front();
            m_reqPending.erase(req);
        }
        // CIEZKA czesc (lookupy + skladanie quadow) - poza watkiem renderu.
        std::vector<QuadRef> quads;
        {
            std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);   // dane mapy stabilne
            if (m_workerStop) return;
            collectFloorChunkQuads(req.first, req.second, quads);
        }
        storeChunkQuads(req.first, req.second, std::move(quads));
        // Obudz render (na watku GUI): emit contentUpdated -> MapGLView::update().
        // update() na samym MapView nie wystarcza (ItemHasContents=false => brak
        // wezla renderu), wiec bez tego sygnalu chunki czekaly na interakcje.
        QMetaObject::invokeMethod(this, [this] { emit contentUpdated(); update(); }, Qt::QueuedConnection);
    }
}

void MapView::requestChunkQuads(int z, quint64 key)
{
    const auto rk = std::make_pair(z, key);
    std::lock_guard<std::mutex> lk(m_reqMutex);
    if (m_reqPending.count(rk)) return;     // juz w kolejce / w toku
    m_reqPending.insert(rk);
    m_reqQueue.push_back(rk);
    m_reqCv.notify_one();
}

std::shared_ptr<const std::vector<MapView::QuadRef>> MapView::takeChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_quadMutex);
    auto zit = m_quadCache.find(z);
    if (zit == m_quadCache.end()) return nullptr;
    auto it = zit->find(key);
    if (it == zit->end()) return nullptr;
    return it.value();   // kopia WSKAZNIKA (wpis niemutowalny - patrz naglowek)
}

void MapView::storeChunkQuads(int z, quint64 key, std::vector<QuadRef> &&q)
{
    {
        std::lock_guard<std::mutex> lk(m_quadMutex);
        // NOWY wektor (nie modyfikacja w miejscu): czytelnicy trzymajacy stary
        // shared_ptr dokanczaja iteracje na starych danych - bez wyscigu.
        m_quadCache[z][key] = std::make_shared<const std::vector<QuadRef>>(std::move(q));
        // Wersja z GLOBALNEGO licznika - nigdy sie nie powtarza (patrz m_chunkVerCounter:
        // per-chunkowe "1,2,3..." po wyczyszczeniu cache wracalo do 1 i zbiegalo sie z
        // wersja trzymana przez renderer -> VBO nie byl przebudowywany).
        if (++m_chunkVerCounter == 0 || m_chunkVerCounter == kChunkPending)
            m_chunkVerCounter = 1;   // omin wartosci-strazniki (0 = Empty, 0xFFFFFFFF = Pending)
        m_chunkVer[z][key] = m_chunkVerCounter;
    }
    m_quadCacheVer.fetch_add(1, std::memory_order_relaxed);   // sygnal dla MapGLView
}

void MapView::refreshSelectionTint()
{
    // Chunki, ktore MAJA teraz zaznaczone kafle (klucz chunka jest wspolny dla
    // wszystkich pieter - bump wersji odswiezy kazde pietro tego chunka).
    QSet<quint64> nowSet;
    for (quint64 pk : m_selected) {
        const int x = selX(pk);
        const int y = selY(pk);
        nowSet.insert(chunkKey(floorDiv(x, kChunkTiles), floorDiv(y, kChunkTiles)));
    }
    // Do przebudowy: chunki ktore zyskaly LUB stracily zaznaczenie (union stary/nowy).
    // Prosto i poprawnie; przy przeciaganiu to garstka chunkow na krawedzi.
    QSet<quint64> dirty = nowSet;
    dirty.unite(m_selChunks);
    if (dirty.isEmpty()) return;

    {
        std::lock_guard<std::mutex> lk(m_quadMutex);
        // WSZYSTKIE pietra (selekcja jest wielopietrowa) - bump tylko chunkow z listy.
        for (auto vit = m_chunkVer.begin(); vit != m_chunkVer.end(); ++vit) {
            for (quint64 ck : dirty) {
                auto it = vit->find(ck);
                if (it == vit->end()) continue;   // brak cache = policzy sie normalnie z flaga
                // Bump wersji (quady zostaja w cache!) -> MapGLView re-uploaduje instancje.
                // Tez z globalnego licznika - wersje nie moga sie powtarzac (patrz store).
                if (++m_chunkVerCounter == 0 || m_chunkVerCounter == kChunkPending)
                    m_chunkVerCounter = 1;
                it.value() = m_chunkVerCounter;
            }
        }
    }
    m_selChunks = nowSet;
    m_quadCacheVer.fetch_add(1, std::memory_order_relaxed);   // obudz MapGLView
}

void MapView::invalidateChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_quadMutex);
    auto zit = m_quadCache.find(z);
    if (zit != m_quadCache.end()) zit->remove(key);
    auto vit = m_chunkVer.find(z);            // trzymaj spojnie z cache
    if (vit != m_chunkVer.end()) vit->remove(key);
}

void MapView::clearChunkQuadCache()
{
    {   // wyrzuc tez zakolejkowane zadania - i tak liczone na starym atlasie
        std::lock_guard<std::mutex> lk(m_reqMutex);
        m_reqQueue.clear();
        m_reqPending.clear();
    }
    std::lock_guard<std::mutex> lk(m_quadMutex);
    m_quadCache.clear();
    m_chunkVer.clear();
}

