// MapView - RDZEN: konstruktor/destruktor, wpiecie readerow, wlasciwosci
// widoku (pietro, rozmiar kafelka, warstwy), wczytywanie mapy, przebudowa
// atlasu po zaladowaniu klienta, centrowanie widoku.
#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
#include <QBuffer>
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
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>


MapView::MapView(QQuickItem *parent)
    : QQuickItem(parent)
{
    // MapView NIE renderuje juz sam (Scene Graph usuniety) - jest kontrolerem:
    // dane (atlas/indeks/watek), stan widoku i obsluga myszy/klawiatury. Render
    // robi MapGLView (OpenGL instancing) czytajac z tego itemu przez 'source'.
    setFlag(ItemHasContents, false);
    // Lewy = zaznaczanie (jak edytor); prawy/srodkowy = panning.
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    m_effectClock.start();   // zegar animacji efektow magicznych
    startWorker();   // watek liczacy quady chunkow w tle (async background loading)
}


// --- Minimapa ---------------------------------------------------------------

// Paleta minimapy Tibii: indeks 0..215 = szescian 6x6x6, skladowe co 51
// (1:1 z tablica minimap_color[256] w RME graphics.h; 216+ = czarny).
static inline uint32_t minimapPalette(int idx)
{
    if (idx < 0 || idx >= 216) return 0xFF000000u;
    const int r = (idx / 36) % 6 * 51;
    const int g = (idx / 6) % 6 * 51;
    const int b = idx % 6 * 51;
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16)
                       | (static_cast<uint32_t>(g) << 8)
                       |  static_cast<uint32_t>(b);
}

uint32_t MapView::minimapColorForTile(const OtbmTile *tile) const
{
    if (!tile || !m_otb || !m_dat) return 0;
    // Od WIERZCHU stosu: pierwszy item z flaga minimap-color wygrywa (jak RME -
    // dywan na trawie daje kolor dywanu, nie trawy).
    for (int i = static_cast<int>(tile->items.size()) - 1; i >= 0; --i) {
        const int cid = m_otb->clientIdForServerId(tile->items[static_cast<size_t>(i)].server_id);
        if (cid <= 0) continue;
        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(cid));
        if (ci && ci->has_minimap_color)
            return minimapPalette(static_cast<int>(ci->minimap_color));
    }
    return 0;   // brak koloru = tlo minimapy
}

void MapView::buildMinimap()
{
    m_minimapImg = QImage();
    m_minimapFloor = m_floor;
    m_minimapOX = m_minimapOY = 0;
    ++m_minimapVer;

    auto zit = m_floorChunkTiles.constFind(m_floor);
    if (zit == m_floorChunkTiles.cend() || zit->isEmpty()) return;

    // Bbox kafli pietra (po chunkach - tanio, bez skanu calej mapy).
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (auto cit = zit->cbegin(); cit != zit->cend(); ++cit)
        for (const OtbmTile *t : cit.value()) {
            minX = std::min<int>(minX, t->x); maxX = std::max<int>(maxX, t->x);
            minY = std::min<int>(minY, t->y); maxY = std::max<int>(maxY, t->y);
        }
    if (minX > maxX) return;

    // Bezpiecznik pamieci: 1px/kafel, wiec 8k x 8k = 256 MB RGB32 - powyzej tnij
    // (mapy OTS praktycznie nie przekraczaja 4-5k kafli w jednej osi).
    const int w = std::min(maxX - minX + 1, 8192);
    const int h = std::min(maxY - minY + 1, 8192);
    m_minimapOX = minX;
    m_minimapOY = minY;
    m_minimapImg = QImage(w, h, QImage::Format_RGB32);
    m_minimapImg.fill(QColor(12, 14, 18));   // tlo jak pustka renderera

    for (auto cit = zit->cbegin(); cit != zit->cend(); ++cit)
        for (const OtbmTile *t : cit.value()) {
            const int px = t->x - minX, py = t->y - minY;
            if (px < 0 || px >= w || py < 0 || py >= h) continue;
            const uint32_t c = minimapColorForTile(t);
            if (c != 0)
                reinterpret_cast<uint32_t *>(m_minimapImg.scanLine(py))[px] = c;
        }
}

const QImage &MapView::minimapImage()
{
    // m_minimapFloor == -1 (inwalidacja po wczytaniu mapy / edycji poza bbox)
    // tez wpada w rebuild, bo m_floor jest zawsze >= 0.
    if (m_minimapFloor != m_floor) buildMinimap();
    return m_minimapImg;
}

void MapView::minimapUpdateTile(int x, int y, int z)
{
    if (z != m_minimapFloor || m_minimapImg.isNull()) return;
    const int px = x - m_minimapOX, py = y - m_minimapOY;
    if (px < 0 || px >= m_minimapImg.width() || py < 0 || py >= m_minimapImg.height()) {
        // Kafel poza bbox obrazu (mapa urosla) - pelny rebuild przy nastepnym odczycie.
        m_minimapFloor = -1;
        ++m_minimapVer;
        return;
    }
    const uint32_t c = minimapColorForTile(m_otbm ? m_otbm->tileAt(x, y, z) : nullptr);
    reinterpret_cast<uint32_t *>(m_minimapImg.scanLine(py))[px] =
        (c != 0) ? c : 0xFF0C0E12u;   // brak koloru = tlo
    ++m_minimapVer;
}

void MapView::setShowAnimations(bool on)
{
    if (m_showAnimations == on) return;
    m_showAnimations = on;

    // Natychmiastowa zmiana stanu (takze powrot na klatke 0 przy wylaczeniu).
    // Rytm klatek napedza MapGLView przez animTick() - tu tylko stan.
    clearChunkQuadCache();
    ++m_dataVersion;
    emit showAnimationsChanged();
    emit contentUpdated(); update();
}

void MapView::animTick()
{
    ++m_animFrame;
    // Klatka jest zapieczona w quadach (cellSpriteId) - uniewaznij cache,
    // widoczne chunki przeliczy worker (renderer trzyma stary VBO do czasu
    // dostarczenia nowych quadow, wiec nic nie mryga). Atlas juz zawiera
    // sprite'y wszystkich klatek (dodawane hurtem per item).
    clearChunkQuadCache();
    ++m_dataVersion;
    emit contentUpdated(); update();
}

void MapView::setShowLowerFloors(bool on)
{
    if (m_showLowerFloors == on) return;
    m_showLowerFloors = on;
    m_lightChunks.clear();   // zakres widocznych pieter = inne zrodla swiatla
    m_lightDirty = true;
    // Zmiane zakresu rysowanych pieter wykrywa MapGLView (glBottomFloor w sync).
    emit showLowerFloorsChanged();
    emit contentUpdated(); update();
}


bool MapView::loadMap(const QString &path)
{
    if (!m_otbm) return false;
    // Lock trzymany PRZEZ CALY loadFile(): OtbmReader resetuje/realokuje m_tiles
    // (deque) i m_posIndex bez wiedzy o watku roboczym - bez tego locka worker
    // moze w tym momencie czytac (przez OtbmTile* w m_floorChunkTiles) kafelki
    // ktore reset() wlasnie kasuje (use-after-free, zalezny od timingu = "czasem
    // dziala czasem nie"). m_dataMutex jest rekurencyjny: loadFile() NA KONCU
    // synchronicznie emituje loadedChanged -> onMapLoaded(), ktore tez lockuje
    // ten sam mutex (na tym samym watku) - musi to byc dozwolone bez deadlocku.
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->loadFile(path);
}

void MapView::rebuildAtlas()
{
    {   // buildAtlasImage PISZE atlas (watek roboczy go czyta) - pod lockiem
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        buildAtlasImage();
    }
    clearChunkQuadCache();   // pusty/stary atlas -> quady do przeliczenia z nowym atlasem
    m_atlasDirty = true;
    emit atlasChanged();
    emit contentUpdated(); update();
}

void MapView::setOtbm(OtbmReader *reader)
{
    if (m_otbm == reader) return;
    if (m_otbm) disconnect(m_otbm, nullptr, this, nullptr);
    m_otbm = reader;
    if (m_otbm) connect(m_otbm, &OtbmReader::loadedChanged, this, &MapView::onMapLoaded);
    emit readersChanged();
    onMapLoaded();
}

void MapView::setOtb(OtbReader *reader)
{
    if (m_otb == reader) return;
    m_otb = reader;
    emit readersChanged();
    onMapLoaded();
}

void MapView::setDat(DatReader *reader)
{
    if (m_dat == reader) return;
    m_dat = reader;
    emit readersChanged();
    onMapLoaded();
}

void MapView::setSpr(SprReader *reader)
{
    if (m_spr == reader) return;
    m_spr = reader;
    emit readersChanged();
    onMapLoaded();
}

void MapView::setFloor(int floor)
{
    floor = std::clamp(floor, 0, 15);
    if (m_floor == floor) return;
    m_floor = floor;
    emit floorChanged();
    m_spawnMarksDirty = true;   // markery spawnow dotycza pietra (leniwa przebudowa)
    m_lightChunks.clear();      // projekcja swiatel zalezy od pietra - caly cache stary
    m_lightDirty = true;
    // Zaznaczenie NIE jest czyszczone: klucz selekcji niesie pietro (selKey), wiec
    // zaznaczone kafle na innych pietrach zyja dalej (multi-floor, jak RME).
    // BEZ updateCurrentFloor() - lookup kafelka idzie teraz przez O(1) tileAt, wiec
    // zmiana pietra jest natychmiastowa (zaden skan calego pietra). To naprawia
    // spadki FPS przy czestym przelaczaniu pieter na full zoomie.
    emit contentUpdated(); update();
}

void MapView::setTileSize(int size)
{
    size = std::clamp(size, 1, 256);
    if (m_tileSize == size) return;
    m_tileSize = size;
    emit tileSizeChanged();
    emit contentUpdated(); update();
}

QString MapView::doodadPreviewSource(int serverId) const
{
    if (!m_brushStore || !m_otb || !m_dat || !m_spr || serverId <= 0) return QString();
    const QString name = m_brushStore->doodadBrushForServerId(serverId);
    if (name.isEmpty()) return QString();
    const QVector<BrushStore::DoodadTile> tiles = m_brushStore->doodadPreviewTiles(name);
    if (tiles.isEmpty()) return QString();   // doodad bez compositu - zwykla ikona

    auto clientItemFor = [&](int sid) -> const ClientItem * {
        const int cid = m_otb->clientIdForServerId(static_cast<uint16_t>(sid));
        return cid > 0 ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    };
    // Pietra rysuja sie ukosnie: kafel o offsecie pietra dz laduje wizualnie na
    // (dx+dz, dy+dz) - tak jak uFloorOff w shaderze. Dzieki temu wodospad/rampa
    // wygladaja w podgladzie tak jak na mapie, a nie splaszczone na jednym poziomie.
    auto effX = [](const BrushStore::DoodadTile &t) { return t.dx + t.dz; };
    auto effY = [](const BrushStore::DoodadTile &t) { return t.dy + t.dz; };

    // Kolejnosc malarza: NIZSZE pietra (wieksze dz) najpierw, wyzsze na wierzchu.
    QVector<BrushStore::DoodadTile> ordered = tiles;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const BrushStore::DoodadTile &a, const BrushStore::DoodadTile &b) {
                         return a.dz > b.dz;
                     });

    // Zbieramy gotowe rysunki z pozycjami w PIKSELACH. Bounds musza uwzgledniac
    // displacement i elevation, bo o nie przesuwa sie rysowanie - liczone z samych
    // wspolrzednych kafli obcinaly itemy z offsetem (np. statuy) na krawedzi platna.
    struct Draw { QImage img; int px, py; };
    QVector<Draw> draws;
    for (const BrushStore::DoodadTile &t : ordered) {
        int elevation = 0;                // akumuluje sie w stosie kafla (jak w renderze)
        for (int id : t.items) {          // itemy kafla od dolu do gory
            const ClientItem *ci = clientItemFor(id);
            if (!ci || ci->sprite_ids.empty()) continue;
            const int w = std::max<int>(1, ci->width);
            const int h = std::max<int>(1, ci->height);
            const int layers = std::max<int>(1, ci->layers);
            // Displacement/elevation jak w appendItemQuads - podglad ma pokazywac to,
            // co realnie wyladuje na mapie.
            const int ox = ci->has_offset ? ci->offset_x : 0;
            const int oy = ci->has_offset ? ci->offset_y : 0;
            const int elev = elevation;
            if (ci->has_elevation) elevation += ci->elevation;
            for (int l = 0; l < layers; ++l)
                for (int hh = 0; hh < h; ++hh)
                    for (int ww = 0; ww < w; ++ww) {
                        const int idx = ((l * h) + hh) * w + ww;   // jak SprReader::itemImageSource
                        if (idx < 0 || idx >= static_cast<int>(ci->sprite_ids.size())) continue;
                        const uint32_t sp = ci->sprite_ids[static_cast<size_t>(idx)];
                        if (sp == 0) continue;
                        auto sprite = m_spr->loadSprite(sp);
                        if (!sprite || sprite->image.isNull()) continue;
                        draws.push_back({ sprite->image,
                                          (effX(t) - ww) * kSprite - ox - elev,
                                          (effY(t) - hh) * kSprite - oy - elev });
                    }
        }
    }
    if (draws.isEmpty()) return QString();   // nic renderowalnego

    int minPx = draws[0].px, minPy = draws[0].py;
    int maxPx = minPx, maxPy = minPy;
    for (const Draw &d : draws) {
        minPx = std::min(minPx, d.px);              maxPx = std::max(maxPx, d.px + d.img.width());
        minPy = std::min(minPy, d.py);              maxPy = std::max(maxPy, d.py + d.img.height());
    }
    const int wpx = maxPx - minPx, hpx = maxPy - minPy;
    if (wpx <= 0 || hpx <= 0 || wpx > 64 * kSprite || hpx > 64 * kSprite) return QString();  // sanity

    QImage img(wpx, hpx, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (const Draw &d : draws)
        p.drawImage(d.px - minPx, d.py - minPy, d.img);
    p.end();

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    buf.close();
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

void MapView::setEraseMode(bool on)
{
    if (m_eraseMode == on) return;
    m_eraseMode = on;
    setCursor((on || m_brushServerId > 0 || m_activeZone != 0) ? Qt::CrossCursor : Qt::ArrowCursor);
    emit eraseModeChanged();
    emit contentUpdated(); update();   // kursor-box gumki
}

void MapView::setActiveZone(int zone)
{
    const quint32 z = static_cast<quint32>(zone < 0 ? 0 : zone);
    if (m_activeZone == z) return;
    m_activeZone = z;
    if (z != 0) {
        // Strefa wyklucza sie z pedzlem itemow i wlacza tryb rysowania (jak wybor pedzla).
        if (m_brushServerId != 0) {
            m_brushServerId = 0;
            m_activeGroundBrush.clear();
            m_activeWallBrush.clear();
            m_activeDoodadBrush.clear();
            emit brushChanged();
        }
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
    }
    setCursor(z != 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    emit activeZoneChanged();
    emit contentUpdated(); update();
}

void MapView::setSelectionMode(bool on)
{
    if (m_selectionMode == on) return;
    m_selectionMode = on;
    // W trybie zaznaczania kursor strzalka (nie krzyzyk pedzla), nawet gdy pedzel aktywny.
    setCursor(on ? Qt::ArrowCursor : (m_brushServerId > 0 ? Qt::CrossCursor : Qt::ArrowCursor));
    emit selectionModeChanged();
    emit contentUpdated(); update();   // odswiez (znika/pojawia sie box podgladu pedzla)
}

void MapView::applyBrushServerId(int serverId, bool asBrush)
{
    if (serverId < 0) serverId = 0;
    // Wybor pedzla z palety = wejscie w tryb rysowania (jak RME SetDrawingMode)
    // i wylaczenie pedzla strefy (wzajemnie sie wykluczaja).
    if (serverId > 0) {
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        m_creatureBrush.clear();   // pedzle wykluczaja sie (jak w RME)
        m_spawnBrush = false;
    }
    if (m_brushServerId == serverId) return;
    m_brushServerId = serverId;
    // Tryb ground brusha (auto-bordery) tylko gdy wolane z palety brushowej (asBrush)
    // ORAZ id nalezy do ground brusha. Palety All Items/RAW wolaja z asBrush=false ->
    // zawsze surowy pojedynczy item (jak w RME).
    m_activeGroundBrush = (asBrush && m_brushStore && serverId > 0)
                              ? m_brushStore->groundBrushForServerId(serverId)
                              : QString();
    // Wall brush (auto-laczenie scian) - jak ground, tylko gdy wolane jako brush i id
    // nalezy do wall brusha. Wzajemnie wyklucza sie z ground brushem.
    m_activeWallBrush = (asBrush && m_brushStore && serverId > 0)
                            ? m_brushStore->wallBrushForServerId(serverId)
                            : QString();
    // Doodad brush (stemple/warianty obiektow) - analogicznie; id = lookid doodada.
    const QString prevDoodad = m_activeDoodadBrush;
    m_activeDoodadBrush = (asBrush && m_brushStore && serverId > 0)
                              ? m_brushStore->doodadBrushForServerId(serverId)
                              : QString();
    // Nowy doodad -> reset wariantu na losowy (R zaczyna od 0). Ten sam -> zostaw.
    if (m_activeDoodadBrush != prevDoodad) m_doodadVariant = -1;
    setCursor(serverId > 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    if (serverId > 0) {   // proaktywnie dodaj sprite'y pedzla do atlasu (przed malowaniem)
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(serverId);
        // Doodad sklada sie z WIELU itemow - bez ich sprite'ow ghost pokazywalby tylko
        // czesc (te, ktore juz sa w atlasie). Dodaj cala liste od razu.
        if (!m_activeDoodadBrush.isEmpty() && m_brushStore)
            for (int id : m_brushStore->doodadItemIds(m_activeDoodadBrush))
                ensureItemSprites(id);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::onMapLoaded()
{
    // Dane CPU budujemy na watku GUI; teksture atlasu wgrywa MapGLView w sync.
    rebuildFloorIndex();   // pod lockiem + czysci cache quadow
    {   // buildAtlasImage PISZE atlas (m_spriteToSlot) - watek roboczy go czyta
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        buildAtlasImage();
    }
    clearChunkQuadCache(); // nowy atlas -> quady do przeliczenia w tle
    m_atlasDirty = true;  // tekstura do (re)utworzenia
    m_floorDirty = true;  // chunki do przebudowy
    emit atlasChanged();
    centerOnContent();
    emit contentUpdated(); update();
}

void MapView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    emit contentUpdated(); update();
}

void MapView::centerOnContent()
{
    updateCurrentFloor();   // przelicz bbox biezacego pietra (na zadanie - rzadkie)
    if (m_currentFloorTiles.isEmpty() && m_floorChunkTiles.isEmpty()) {
        m_originX = 0;
        m_originY = 0;
        emit contentUpdated(); update();
        return;
    }
    const qreal viewTilesW = width() / std::max(1, m_tileSize);
    const qreal viewTilesH = height() / std::max(1, m_tileSize);
    m_originX = (m_minTileX + m_maxTileX + 1) / 2.0 - viewTilesW / 2.0;
    m_originY = (m_minTileY + m_maxTileY + 1) / 2.0 - viewTilesH / 2.0;
    emit contentUpdated(); update();
}

void MapView::centerOnTile(int x, int y, int z)
{
    if (z >= 0 && z <= 15 && z != m_floor) setFloor(z);
    const qreal viewTilesW = width() / std::max(1, m_tileSize);
    const qreal viewTilesH = height() / std::max(1, m_tileSize);
    m_originX = x + 0.5 - viewTilesW / 2.0;
    m_originY = y + 0.5 - viewTilesH / 2.0;
    emit contentUpdated(); update();
}

