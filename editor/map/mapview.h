#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QQuickItem>
#include <QHash>
#include <QSet>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QElapsedTimer>
#include <QVariantList>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <set>
#include <utility>

#include "otbmreader.h"
#include "otbreader.h"
#include "datreader.h"
#include "sprreader.h"
#include "brushstore.h"
#include "creaturestore.h"

class QTimer;

// -----------------------------------------------------------------------------
// MapView
//
// Renderuje jedno pietro mapy OTBM na GPU przez Qt Scene Graph (NIE QPainter).
// Element UI, dlatego siedzi w aplikacji demo (linkuje Qt6::Quick).
//
// Architektura (skaluje sie na duze mapy):
//   - ATLAS: wszystkie uzyte sprite'y 32x32 pakowane leniwie w jedna teksture
//     (QSGTexture). Geometria odwoluje sie do atlasu przez UV.
//   - CHUNKI: kafelki pogrupowane w bloki kChunkTiles x kChunkTiles. Geometria
//     chunka budowana jest LENIWIE (tylko gdy chunk wejdzie w widok) i
//     cache'owana jako QSGGeometryNode. Niewidoczne chunki nie maja geometrii.
//   - PAN/ZOOM: QSGTransformNode - sama macierz, zero przebudowy geometrii.
//
// Lancuch danych: OtbmTile.items[].server_id -> OtbReader::clientIdForServerId
//   -> DatReader::itemByClientId -> ClientItem.sprite_ids -> SprReader::loadSprite
// -----------------------------------------------------------------------------
class MapView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(OtbmReader *otbm READ otbm WRITE setOtbm NOTIFY readersChanged)
    Q_PROPERTY(OtbReader *otb READ otb WRITE setOtb NOTIFY readersChanged)
    Q_PROPERTY(DatReader *dat READ dat WRITE setDat NOTIFY readersChanged)
    Q_PROPERTY(SprReader *spr READ spr WRITE setSpr NOTIFY readersChanged)
    Q_PROPERTY(int floor READ floor WRITE setFloor NOTIFY floorChanged)
    Q_PROPERTY(int tileSize READ tileSize WRITE setTileSize NOTIFY tileSizeChanged)
    Q_PROPERTY(int spriteCount READ spriteCount NOTIFY atlasChanged)
    // Pokazuj pietra pod spodem. false = tylko biezace pietro (jak RME "Show all floors", Ctrl+W).
    Q_PROPERTY(bool showLowerFloors READ showLowerFloors WRITE setShowLowerFloors NOTIFY showLowerFloorsChanged)
    // Przyciemnienie nizszych pieter (jak RME "Show shade", Q) - NIEZALEZNE od
    // showLowerFloors: to tylko czy pokazane nizsze pietra sa przyciemnione, nie czy
    // sa w ogole pokazane. W RME to dwa osobne przelaczniki, u nas tez.
    Q_PROPERTY(bool showShade READ showShade WRITE setShowShade NOTIFY showShadeChanged)
    // Efekt magiczny przy stawianiu itemu (jednorazowa animacja).
    Q_PROPERTY(bool placeEffect READ placeEffect WRITE setPlaceEffect NOTIFY placeEffectChanged)
    // Rozmiar pedzla jak RME: promien 0..11 (0=1x1, 1=3x3, 2=5x5, 4, 6, 8, 11).
    Q_PROPERTY(int brushSize READ brushSize WRITE setBrushSize NOTIFY brushParamsChanged)
    // Ksztalt pedzla: "square" lub "circle" (jak RME).
    Q_PROPERTY(QString brushShape READ brushShape WRITE setBrushShape NOTIFY brushParamsChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    // Trwa tryb wklejania (podglad schowka pod kursorem, czeka na LPM) - jak RME isPasting().
    Q_PROPERTY(bool pasting READ pasting NOTIFY pastingChanged)
    // "Border Automagic" (klawisz A w RME): czy auto-bordery generuja sie przy rysowaniu.
    // Wylaczenie pozwala klasc grunt bez krawedzi (przydatne przy recznym borderowaniu).
    Q_PROPERTY(bool automagic READ automagic WRITE setAutomagic NOTIFY automagicChanged)
    Q_PROPERTY(QString hoverText READ hoverText NOTIFY hoverChanged)
    // Aktywny "pedzel" - server_id itemu stawianego lewym klikiem. 0 = tryb zaznaczania.
    Q_PROPERTY(int brushServerId READ brushServerId WRITE setBrushServerId NOTIFY brushChanged)
    // Pedzel potwora (nazwa z creatures.xml; "" = brak) i pedzel spawnu (centrum +
    // promien = rozmiar pedzla). Wykluczaja sie z pedzlem itemu/strefy (jak w RME).
    Q_PROPERTY(QString creatureBrush READ creatureBrush WRITE setCreatureBrush NOTIFY brushChanged)
    Q_PROPERTY(bool spawnBrush READ spawnBrush WRITE setSpawnBrush NOTIFY brushChanged)
    Q_PROPERTY(int creatureSpawntime READ creatureSpawntime WRITE setCreatureSpawntime NOTIFY brushChanged)
    Q_PROPERTY(int spawnBrushRadius READ spawnBrushRadius WRITE setSpawnBrushRadius NOTIFY brushChanged)
    // Pedzel domu (id domu z palety House; 0 = brak) i tryb ustawiania wejscia
    // (klik = entry aktywnego domu). Wykluczaja sie z innymi pedzlami (jak RME).
    Q_PROPERTY(int houseBrush READ houseBrush WRITE setHouseBrush NOTIFY brushChanged)
    Q_PROPERTY(bool houseExitMode READ houseExitMode WRITE setHouseExitMode NOTIFY brushChanged)
    // Przelacznik "pochodnia" (topbar, ikony itemow 2058/2059). Sam stan - funkcja
    // zostanie podpieta pozniej (decyzja uzytkownika).
    Q_PROPERTY(bool torchOn READ torchOn WRITE setTorchOn NOTIFY torchChanged)
    // Tryb box-selekcji (RME Select > Selection Mode): ktore pietra zaznacza prostokat.
    // 0 = Current Floor, 1 = Lower Floors (biezace + wszystko pod, do z=15),
    // 2 = Visible Floors (dokladnie to, co widac - renderBottomFloor).
    Q_PROPERTY(int selectionFloors READ selectionFloors WRITE setSelectionFloors NOTIFY selectionOptionsChanged)
    // Compensate Selection (RME): na nizszych pietrach obszar przesuniety zgodnie
    // z ukosna projekcja pieter - zaznacza sie to, co WIDAC w prostokacie.
    Q_PROPERTY(bool compensatedSelect READ compensatedSelect WRITE setCompensatedSelect NOTIFY selectionOptionsChanged)
    // Tryb zaznaczania (jak RME SELECTION_MODE): LPM zaznacza zamiast malowac, a pedzel
    // POZOSTAJE zapamietany. Przelaczany SPACJA. Wybor pedzla z palety wlacza tryb rysowania.
    Q_PROPERTY(bool selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    // Aktywny pedzel STREFY (flagi kafla OTBM, jak RME): 0=brak, 1=PZ, 4=No-PvP,
    // 8=No-Logout, 16=PvP. LPM maluje strefe, Ctrl+LPM ja kasuje.
    Q_PROPERTY(int activeZone READ activeZone WRITE setActiveZone NOTIFY activeZoneChanged)
    // Trwaly tryb GUMKI (przycisk w pasku): LPM kasuje zamiast stawiac - dziala na
    // strefy (gdy wybrana) ORAZ na itemy (wierzchni item kafla, z przeliczeniem
    // borderow/scian). Ctrl przy kliknieciu robi to samo doraznie (jak RME undraw).
    Q_PROPERTY(bool eraseMode READ eraseMode WRITE setEraseMode NOTIFY eraseModeChanged)

public:
    explicit MapView(QQuickItem *parent = nullptr);
    ~MapView() override;

    OtbmReader *otbm() const { return m_otbm; }
    OtbReader *otb() const { return m_otb; }
    DatReader *dat() const { return m_dat; }
    SprReader *spr() const { return m_spr; }
    int floor() const { return m_floor; }
    int tileSize() const { return m_tileSize; }
    int spriteCount() const { return m_atlasSlots.size(); }
    int selectionCount() const { return m_selected.size(); }
    QString hoverText() const { return m_hoverText; }
    int brushServerId() const { return m_brushServerId; }
    bool selectionMode() const { return m_selectionMode; }
    void setSelectionMode(bool on);
    int activeZone() const { return static_cast<int>(m_activeZone); }
    void setActiveZone(int zone);
    bool eraseMode() const { return m_eraseMode; }
    // Implementacja w .cpp - setCursor potrzebuje pelnego QCursor (tu tylko fwd-decl).
    void setEraseMode(bool on);
    // Przelacza tryb rysowanie<->zaznaczanie (SPACJA / przycisk w UI).
    Q_INVOKABLE void toggleSelectionMode() { setSelectionMode(!m_selectionMode); }
    // Property WRITE = tryb SUROWY (pojedynczy item, bez auto-borderow) - uzywane przez
    // palety "All Items"/"RAW" oraz "uzyj jako pedzel" z menu kontekstowego mapy.
    void setBrushServerId(int serverId) { applyBrushServerId(serverId, false); }
    // Tryb BRUSHA (ground + auto-bordery, jesli id nalezy do ground brusha) - uzywane
    // przez palety Terrain/Doodad/Item/My Palettes. Jak w RME: RAW/All to surowe itemy,
    // reszta palet to brushe.
    Q_INVOKABLE void useGroundBrush(int serverId) { applyBrushServerId(serverId, true); }
    // Wpina silnik brushy (ground + auto-bordery). QML wola raz na starcie.
    Q_INVOKABLE void setBrushStore(BrushStore *bs) { m_brushStore = bs; }
    // Wpina liste potworow (creatures.xml) - lookup nazwa -> looktype dla renderu.
    Q_INVOKABLE void setCreatureStore(CreatureStore *cs) { m_creatureStore = cs; }
    // Nazwa brusha (ground/wall/doodad) zawierajacego ten server id, lub "" gdy item
    // nie nalezy do zadnego brusha. Do menu "Select Brush" (jak RME): item z mapy ->
    // pedzel, ktory go stawia.
    Q_INVOKABLE QString brushForServerId(int serverId) const {
        if (!m_brushStore || serverId <= 0) return QString();
        QString n = m_brushStore->groundBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushStore->wallBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushStore->doodadBrushForServerId(serverId);
        return n;
    }

    QString creatureBrush() const { return m_creatureBrush; }
    void setCreatureBrush(const QString &name);
    bool spawnBrush() const { return m_spawnBrush; }
    void setSpawnBrush(bool on);
    int creatureSpawntime() const { return m_creatureSpawntime; }
    void setCreatureSpawntime(int s) {
        s = std::clamp(s, 1, 86400);
        if (m_creatureSpawntime == s) return;
        m_creatureSpawntime = s;
        emit brushChanged();
    }
    bool torchOn() const { return m_torchOn; }
    void setTorchOn(bool on) {
        if (m_torchOn == on) return;
        m_torchOn = on;
        m_lightChunks.clear();   // wlaczenie = policz od zera
        m_lightDirty = true;
        emit torchChanged();
        emit contentUpdated(); update();
    }

    int selectionFloors() const { return m_selectionFloors; }
    void setSelectionFloors(int m) {
        m = std::clamp(m, 0, 2);
        if (m_selectionFloors == m) return;
        m_selectionFloors = m;
        emit selectionOptionsChanged();
    }
    bool compensatedSelect() const { return m_compensatedSelect; }
    void setCompensatedSelect(bool on) {
        if (m_compensatedSelect == on) return;
        m_compensatedSelect = on;
        emit selectionOptionsChanged();
    }

    int houseBrush() const { return m_houseBrush; }
    void setHouseBrush(int id);
    bool houseExitMode() const { return m_houseExitMode; }
    void setHouseExitMode(bool on);

    int spawnBrushRadius() const { return m_spawnBrushRadius; }
    void setSpawnBrushRadius(int r) {
        r = std::clamp(r, 1, 15);
        if (m_spawnBrushRadius == r) return;
        m_spawnBrushRadius = r;
        emit brushChanged();
    }
    // Podglad doodada dla palety: gdy serverId to lookid doodada z compositem, sklada
    // CALY stempel (wszystkie kafle i itemy) w jeden obrazek (data URL). "" gdy to nie
    // composite - paleta rysuje wtedy zwykla ikone itemu.
    Q_INVOKABLE QString doodadPreviewSource(int serverId) const;
    // Nazwa aktywnego ground brusha ("" gdy aktywny pedzel to zwykly item lub brak).
    // Ustawiane automatycznie w setBrushServerId, gdy server id nalezy do ground brusha.
    QString activeGroundBrush() const { return m_activeGroundBrush; }
    bool showLowerFloors() const { return m_showLowerFloors; }
    void setShowLowerFloors(bool on);
    bool showShade() const { return m_showShade; }
    void setShowShade(bool on) {
        if (m_showShade == on) return;
        m_showShade = on;
        emit showShadeChanged();
        emit contentUpdated(); update();
    }
    // Do MapGLRenderer::synchronize() - czy rysowac overlay przyciemnienia.
    bool glShowShade() const { return m_showShade; }
    bool placeEffect() const { return m_placeEffect; }
    void setPlaceEffect(bool on) { if (m_placeEffect != on) { m_placeEffect = on; emit placeEffectChanged(); } }
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int r) { r = std::clamp(r, 0, 11);
        if (m_brushSize != r) { m_brushSize = r; emit brushParamsChanged(); emit contentUpdated(); update(); } }
    QString brushShape() const { return m_brushShape; }
    void setBrushShape(const QString &s) {
        if (m_brushShape != s && (s == QLatin1String("square") || s == QLatin1String("circle")))
            { m_brushShape = s; emit brushParamsChanged(); emit contentUpdated(); update(); } }
    // Czy offset (dx,dy) wpada w footprint pedzla (kwadrat/okrag, jak RME).
    bool brushCovers(int dx, int dy) const {
        if (m_brushShape == QLatin1String("circle"))
            return dx * dx + dy * dy <= m_brushSize * m_brushSize;  // test odleglosci
        return std::abs(dx) <= m_brushSize && std::abs(dy) <= m_brushSize;
    }

    // --- API dla renderera OpenGL (MapGLView) ---
    // Wszystkie wolane z MapGLView::synchronize() (watek renderu, GUI zablokowany).
    const QImage &glAtlasImage() const { return m_atlasImage; }
    int glAtlasGeneration() const { return m_atlasGeneration; } // rosnie po przebudowie
    Q_INVOKABLE double glOriginX() const { return m_originX; }  // Q_INVOKABLE tez dla QML (Go To Position)
    Q_INVOKABLE double glOriginY() const { return m_originY; }
    int glBottomFloor() const { return renderBottomFloor(); }
    // Rosnie gdy watek roboczy dostarczy nowe quady. MapGLView sprawdza gotowosc
    // pietra TYLKO gdy to sie zmienilo (nie co klatke) - inaczej skan calej mapy
    // co klatke zabija FPS podczas budowy gestego pietra.
    int glQuadCacheVersion() const { return m_quadCacheVer.load(std::memory_order_relaxed); }
    // Wersja TRESCI (nie pozycji): zmiana => bufor instancji trzeba przebudowac.
    // Laczy pietro, zakres pieter, generacje atlasu i wersje danych (edycje).
    quint64 glContentVersion() const;
    // Zbiera instancje JEDNEGO pietra z dla prostokata chunkow (na out: x,y,slotX,
    // slotY - surowe pozycje, bez offsetu pietra). complete=false gdy brakuje quadow.
    // groundOnly=true => tylko podloga (LOD przy oddaleniu, jak RME) = mniej instancji.
    void glCollectFloorInstances(int z, int cMinX, int cMinY, int cMaxX, int cMaxY,
                                 bool groundOnly, std::vector<float> &out, bool &complete);
    // TANI test: czy wszystkie quady JEDNEGO pietra dla prostokata sa w cache.
    // Brakujace zakolejkowuje do watku. true => mozna skladac bufor tego pietra.
    bool glFloorChunksReady(int z, int cMinX, int cMinY, int cMaxX, int cMaxY);

    // --- Rendering per-chunk (VBO na chunk, jak sektory RME): edycja przebudowuje
    // tylko dotkniety chunk, nie caly widoczny bufor pietra. ---
    static constexpr quint32 kChunkEmpty   = 0;            // chunk bez kafelkow na tym pietrze
    static constexpr quint32 kChunkPending = 0xFFFFFFFFu;  // ma kafelki, quady jeszcze liczone
    // TANI peek wersji tresci chunka (do decyzji czy przebudowac jego VBO). Zwraca:
    // kChunkEmpty (nic do rysowania), kChunkPending (zlecono watkowi - trzymaj stary
    // VBO), albo wersje >=1 (rosnie przy edycji chunka).
    quint32 glChunkVersion(int z, quint64 chunkKey);
    // Zakolejkuj policzenie quadow chunka (gdy glChunkVersion == kChunkPending).
    void glRequestChunk(int z, quint64 chunkKey) { requestChunkQuads(z, chunkKey); }
    // Instancje POJEDYNCZEGO chunka (x,y,slotX,slotY). Wola tylko gdy wersja != stan
    // VBO renderera. Zwraca wersje tresci uzyta (do zapisania w ChunkBuf renderera).
    quint32 glCollectChunkInstances(int z, quint64 chunkKey, bool groundOnly,
                                    std::vector<float> &out);
    // Instancje aktywnych efektow magicznych (animacja przy stawianiu itemu) na
    // biezacym pietrze: x,y,slotX,slotY (biezaca klatka). Usuwa skonczone efekty.
    void glCollectEffectInstances(std::vector<float> &out);
    // Czy gra jakas animacja efektu - MapGLView renderuje wtedy ciagle (klatki
    // animacji zmieniaja sie w czasie, bez zadnego sygnalu contentUpdated).
    bool hasActiveEffects() const { return !m_activeEffects.empty(); }
    // Sylwet ZAZNACZONYCH itemow (wierzchni item kazdego kafelka) - do przyciemnienia.
    void glCollectSelectionInstances(std::vector<float> &out);
    // Kursor pedzla PER-KAFEL (jak RME DrawBrush): 2 floaty/kafel (world px) dla kazdego
    // kafla footprintu. Dzieki temu kolo wyglada jak kolo, a nie jak prostokat otaczajacy.
    void glCollectBrushCursorInstances(std::vector<float> &out);
    // Markery spawnow (fioletowe): centrum + obrys promienia. Format x,y,w,h jak
    // kursor pedzla - rysowane tym samym programem, innym kolorem. Rozdzielone na
    // niezaznaczone/zaznaczone (drugi zestaw rysowany przyciemniony, jak itemy).
    void glCollectSpawnMarkInstances(std::vector<float> &out, std::vector<float> &outSel);

    // --- Oswietlenie (jak TIME LightManager): bufor 1 piksel = 1 kafel dla
    // widocznego zakresu. Przeliczany leniwie (edycja/zmiana widoku/pietra);
    // zwraca wersje - MapGLView robi re-upload tekstury tylko gdy wzrosla.
    quint32 glUpdateLightGrid();
    const std::vector<uint32_t> &lightPixels() const { return m_lightPixels; }
    void lightRect(int &tx, int &ty, int &tw, int &th) const {
        tx = m_lightTX; ty = m_lightTY; tw = m_lightTW; th = m_lightTH;
    }
    // Duch PRZENOSZONEGO/pedzlowanego itemu na kursorze - do polprzezroczystego podgladu.
    void glCollectGhostInstances(std::vector<float> &out);
    // Prostokat zaznaczania (Shift/Ctrl + przeciagniecie na pustym) w px (world, biezace
    // pietro): x0,y0,x1,y1. Zwraca false gdy nie trwa zaznaczanie (nic do rysowania).
    bool glRubberBandRect(double &x0, double &y0, double &x1, double &y1) const {
        if (!m_selecting) return false;
        x0 = std::min(m_anchorX, m_rubberX) * kSprite;
        y0 = std::min(m_anchorY, m_rubberY) * kSprite;
        x1 = (std::max(m_anchorX, m_rubberX) + 1) * kSprite;
        y1 = (std::max(m_anchorY, m_rubberY) + 1) * kSprite;
        return true;
    }
    // Prostokat footprintu aktywnego pedzla (kursor-box jak w RME) w px (world,
    // biezace pietro): x0,y0,x1,y1. false gdy brak aktywnego pedzla / hoveru / trwa
    // przenoszenie. Rysowany w GL (nie nakladka QML) - szybka sciezka bez kompozycji.
    bool glBrushRect(double &x0, double &y0, double &x1, double &y1) const {
        if (m_movingSel || m_selecting || m_selectionMode || m_hoverX < 0) return false;
        if (m_brushServerId <= 0 && m_activeZone == 0 && !m_eraseMode) return false;  // strefa i gumka tez maja kursor-box
        const int r = m_brushSize;
        x0 = static_cast<double>((m_hoverX - r) * kSprite);
        y0 = static_cast<double>((m_hoverY - r) * kSprite);
        x1 = static_cast<double>((m_hoverX + r + 1) * kSprite);
        y1 = static_cast<double>((m_hoverY + r + 1) * kSprite);
        return true;
    }

    void setOtbm(OtbmReader *reader);
    void setOtb(OtbReader *reader);
    void setDat(DatReader *reader);
    void setSpr(SprReader *reader);
    void setFloor(int floor);
    void setTileSize(int size);

    // Wczytuje mape (otbmReader->loadFile) pod m_dataMutex - inaczej watek roboczy
    // moze wtedy czytac (przez wskazniki OtbmTile* w m_floorChunkTiles) kafelki
    // WLASNIE kasowane/realokowane przez reset+reload w OtbmReader - realny wyscig
    // (use-after-free), objawiajacy sie jako "ta sama mapa czasem sie wczytuje,
    // czasem nie" (zalezne od przypadkowego timingu watkow). QML MA wolac TO,
    // nie otbmReader.loadFile() bezposrednio, dla mapy uzywanej przez ten MapView.
    Q_INVOKABLE bool loadMap(const QString &path);
    // Przebudowa atlasu sprite'ow PO zaladowaniu dat/spr/otb. W flow "mapa najpierw"
    // (wykrycie wersji klienta z naglowka OTBM) loadMap() buduje atlas ZANIM dat/spr/otb
    // sa wczytane - powstaje PUSTY atlas. QML musi wywolac to po zaladowaniu klienta,
    // inaczej mapa jest pusta (tiles sa, ale zero grafiki - worker liczy 0 quadow).
    Q_INVOKABLE void rebuildAtlas();
    Q_INVOKABLE void centerOnContent();
    // Przejdz na pietro z i wycentruj widok na kafelku (x,y) - do "Go to" (towns/waypointy).
    Q_INVOKABLE void centerOnTile(int x, int y, int z);
    // Zoom o "steps" kroki (jak scroll), wysrodkowany na srodku widoku - do skrotow
    // klawiszowych Ctrl+ / Ctrl- / Ctrl+0 (Zoom In/Out/Normal, jak RME).
    Q_INVOKABLE void zoomSteps(int steps) { zoomAt(steps, width() / 2.0, height() / 2.0); }
    Q_INVOKABLE void clearSelection();
    // Szczegoly zaznaczenia dla panelu: lista {x,y,z,items:[{serverId,clientId,name}]}.
    Q_INVOKABLE QVariantList selectionDetails() const;

    // Menu kontekstowe (PPM): info o kliknietym kafelku/wierzchnim itemie.
    Q_INVOKABLE QVariantMap contextInfo() const;
    // Ustawia count wierzchniego itemu na kafelku z PPM (Properties). Dziala tylko dla
    // stackowalnych; zwraca true gdy cos sie zmienilo. Zakres 1-100 (limit sterty w Tibii).
    Q_INVOKABLE bool setContextItemCount(int count);
    // Spawntime potwora na kaflu z PPM (Properties). RME tez pozwala go zmieniac
    // na istniejacym potworze. Zwraca true gdy cos sie zmienilo.
    Q_INVOKABLE bool setContextCreatureSpawntime(int seconds);
    // Promien ISTNIEJACEGO centrum spawnu na kaflu z PPM (Properties).
    Q_INVOKABLE bool setContextSpawnRadius(int radius);
    // Usuwa wierzchni item z kazdego zaznaczonego kafelka.
    Q_INVOKABLE void deleteSelectedTop();
    // Stawia item (z poprawna kolejnoscia rysowania) na kafelku biezacego pietra.
    Q_INVOKABLE void placeItemAt(int x, int y, int serverId);
    // Jak wyzej, ale na WSKAZANYM pietrze - doodady wielopoziomowe (wodospad, rampa,
    // wysokie drzewa) maja czesci na z +/- 1 wzgledem kursora.
    void placeItemOnFloor(int x, int y, int z, int serverId);
    // Wariant zachowujacy atrybuty itemu (count/action id/unique id/zawartosc kontenera).
    // Move i paste musza isc tedy - inaczej przenoszony item odradza sie z count=1.
    void placeItemOnFloor(int x, int y, int z, const OtbmMapItem &item);

    // --- Schowek regionu (copy/cut/paste wielu kafelkow, jak RME) ---
    // Kopiuje CALE stosy itemow zaznaczonych kafelkow do schowka (offsety wzgl. rogu).
    Q_INVOKABLE void copySelection();
    // Kopiuje + czysci zaznaczone kafelki (usuwa wszystkie itemy, z groundem).
    Q_INVOKABLE void cutSelection();
    // Ctrl+V: wchodzi w TRYB WKLEJANIA (jak RME StartPasting) - schowek wisi
    // polprzezroczysto pod kursorem, LPM zatwierdza, Esc/PPM anuluje. Samo Ctrl+V
    // niczego jeszcze nie zmienia na mapie.
    Q_INVOKABLE void startPasting();
    Q_INVOKABLE void cancelPasting();
    Q_INVOKABLE bool hasClipboard() const { return !m_clipboard.empty(); }
    bool pasting() const { return m_pasting; }
    bool automagic() const { return m_automagic; }
    void setAutomagic(bool on) {
        if (m_automagic == on) return;
        m_automagic = on;
        emit automagicChanged();
    }
    // Przesuwa CALE zaznaczenie o (dx,dy) - jak RME editor.moveSelection. Przenosi
    // pelne stosy zaznaczonych kafli i aktualizuje zaznaczenie na nowe pozycje.
    void moveSelection(int dx, int dy);

    // --- Menu "Select" (jak RME) - operacje na zaznaczeniu, kazda = jedno cofniecie ---
    // Przelicza auto-bordery na zaznaczonych kaflach (RME Editor::borderizeSelection).
    Q_INVOKABLE void borderizeSelection();
    // Losuje na nowo wariant gruntu na zaznaczonych kaflach nalezacych do ground brusha
    // (RME Editor::randomizeSelection - kafle bez ground brusha pomija).
    Q_INVOKABLE void randomizeSelection();
    // Usuwa wszystkie itemy o danym server-id z zaznaczonych kafli. Zwraca licznik.
    Q_INVOKABLE int removeItemOnSelection(int serverId);
    // Podmienia itemy fromId -> toId na zaznaczonych kaflach (w miejscu, zachowujac
    // pozycje w stosie). Zwraca licznik podmian.
    Q_INVOKABLE int replaceItemsOnSelection(int fromId, int toId);
    // Ile itemow o danym server-id jest na zaznaczeniu (RME "Find Item on Selection").
    Q_INVOKABLE int countItemOnSelection(int serverId) const;

    // --- Menu "Edit" (jak RME) - operacje na CALEJ mapie -------------------------
    // Przelicza auto-bordery na calej mapie (RME "Borderize Map").
    Q_INVOKABLE void borderizeMap();
    // Losuje na nowo warianty gruntu na calej mapie (RME "Randomize Map").
    Q_INVOKABLE void randomizeMap();
    Q_INVOKABLE int replaceItemsOnMap(int fromId, int toId);
    Q_INVOKABLE int removeItemsOnMap(int serverId);
    // Skacze do pierwszego wystapienia itemu na mapie. false = nie znaleziono.
    Q_INVOKABLE bool jumpToItemOnMap(int serverId);

    // "Go to Previous Position" (P) - RME pamieta poprzedni srodek widoku.
    Q_INVOKABLE void centerOnPosition(int x, int y, int z);
    Q_INVOKABLE bool goToPreviousPosition();
    Q_INVOKABLE bool hasPreviousPosition() const { return m_prevCenterValid; }
    // Cofa ostatnia zmiane (Ctrl+Z) i odswieza render.
    Q_INVOKABLE void undo();
    // Ponawia cofnieta zmiane (Ctrl+Y) i odswieza render.
    Q_INVOKABLE void redo();
    // Punktowe synchroniczne przeliczenie chunkow kafli dotknietych przez ostatnie
    // undo/redo (wolane pod m_dataMutex).
    void refreshUndoRedoTilesLocked();

signals:
    void readersChanged();
    void floorChanged();
    void tileSizeChanged();
    void atlasChanged();
    void selectionChanged();
    void selectionModeChanged();
    void selectionOptionsChanged();   // tryb box-selekcji / kompensacja pieter
    void torchChanged();              // przelacznik-pochodnia (topbar)
    void activeZoneChanged();
    void eraseModeChanged();
    void clipboardChanged();
    void pastingChanged();
    void automagicChanged();
    void hoverChanged();
    void brushChanged();
    void showLowerFloorsChanged();
    void showShadeChanged();
    void placeEffectChanged();
    void brushParamsChanged();
    // Emitowany (na watku GUI), gdy watek roboczy dostarczyl nowe quady chunka albo
    // po edycji/wczytaniu. MapGLView na to reaguje wlasnym update() - bez tego
    // renderer (osobny item z ItemHasContents=false po stronie MapView) nie budzil
    // sie sam po async-liczeniu chunkow (itemy pojawialy sie dopiero przy interakcji).
    void contentUpdated();
    void contextMenuRequested(qreal x, qreal y); // PPM - otworz menu w tym punkcie

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    // Zoom multiplikatywny o "steps" (+ = przyblizenie), wysrodkowany na (px,py)
    // (wspolrzedne ekranu). Wspolne dla scrolla i skrotow klawiszowych.
    void zoomAt(int steps, qreal px, qreal py);
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void onMapLoaded();

private:
    static constexpr int kSprite = 32;       // rozmiar sprite'a w px
    static constexpr int kChunkTiles = 32;   // bok chunka w kafelkach

    // Jeden sprite-cell do wyrysowania: pozycja kafelka w swiecie + atlas slot.
    struct QuadRef {
        int worldX;        // px (tileX * 32)
        int worldY;
        int atlasSlot;     // indeks w m_atlasSlots
        bool ground;       // czy to podloga (do LOD przy oddaleniu - jak RME)
        int tileX = 0, tileY = 0;  // kafelek-kotwica (do sprawdzenia zaznaczenia per-instancja)
        bool topItem = false;      // czy nalezy do WIERZCHNIEGO itemu kafla (tint zaznaczenia jak RME)
        int zoneFlags = 0;         // flagi strefy kafla (PZ/PvP/...) - tint podlogi jak RME
    };

    void buildStaticIndex();    // RAZ: indeks [z][chunk] -> kafelki (O(cala mapa))
    void updateCurrentFloor();  // szybkie: lookup/bbox biezacego pietra
    void rebuildFloorIndex();   // = buildStaticIndex + updateCurrentFloor (po edycji)
    bool chunkHasContent(quint64 chunkKey) const; // czy chunk ma cos w zakresie pieter
    void buildAtlasImage();     // pelny atlas sprite'ow mapy (po wczytaniu)
    void addSpritesToAtlas(const QSet<uint32_t> &sids); // append-only: dokladaj nowe
    void ensureItemSprites(int serverId);               // dodaj sprite'y itemu do atlasu
    int  atlasSlotForSprite(uint32_t spriteId) const; // -1 jesli brak
    // Dokleja quady (pozycja + atlas slot) wszystkich itemow kafelka.
    void appendItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const;
    // Tylko WIERZCHNI (renderowalny) item kafelka - do podswietlania zaznaczenia.
    void appendTopItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const;
    // Zbiera quady JEDNEGO pietra (z) dla danego chunka, na surowych pozycjach
    // swiata (bez offsetu pietra). CIEZKIE (lookupy) - liczone na watku w tle.
    // Wywolywane pod m_dataMutex (czyta kafelki/atlas).
    void collectFloorChunkQuads(int z, quint64 chunkKey, std::vector<QuadRef> &out);

    // --- Async budowa geometrii w tle (jak "Async Background Loading" w TIME) ---
    void startWorker();
    void stopWorker();
    void workerLoop();
    void requestChunkQuads(int z, quint64 chunkKey);          // zakolejkuj do watku
    // Gotowe quady chunka albo nullptr. Zwraca WSKAZNIK do niemutowalnego wpisu
    // cache (pod lockiem kopiuje sie tylko shared_ptr, nie tysiace QuadRef) -
    // wpisy sa const i nigdy nie modyfikowane w miejscu (store wstawia NOWY
    // wektor), wiec czytanie po zwolnieniu locka jest bezpieczne.
    std::shared_ptr<const std::vector<QuadRef>> takeChunkQuads(int z, quint64 chunkKey);
    void storeChunkQuads(int z, quint64 chunkKey, std::vector<QuadRef> &&q); // wpis do cache
    void invalidateChunkQuads(int z, quint64 chunkKey);       // po edycji jednego chunka
    // Zaznaczenie tintowane W GLOWNYM przebiegu (jak RME) - nie osobna warstwa. Po
    // zmianie m_selected bumpuje wersje chunkow ktore zyskaly/stracily zaznaczone kafle,
    // co wymusza re-upload ich instancji (z flaga selected), BEZ przeliczania quadow.
    void refreshSelectionTint();
    void notifySelectionChanged() { refreshSelectionTint(); emit selectionChanged(); }
    void clearChunkQuadCache();                               // po zmianie atlasu

    // Najglebsze rysowane pietro. Domyslnie = biezace (tylko ono, jak RME).
    // Z opcja "pokazuj nizsze" - nad ziemia do z=7, pod ziemia 2 w dol.
    int renderBottomFloor() const {
        if (!m_showLowerFloors) return m_floor;
        return (m_floor < 8) ? 7 : std::min(15, m_floor + 2);
    }

    static quint64 chunkKey(int cx, int cy) {
        return (static_cast<quint64>(static_cast<uint32_t>(cx)) << 32)
             | static_cast<uint32_t>(cy);
    }
    static quint64 posKey(int x, int y) {
        return (static_cast<quint64>(static_cast<uint32_t>(x)) << 32)
             | static_cast<uint32_t>(y);
    }
    // Klucz SELEKCJI - z pietrem (z), zeby zaznaczenie moglo obejmowac wiele pieter
    // i przetrwac zmiane pietra (jak RME). Format jak OtbmReader::posKey3d.
    static quint64 selKey(int x, int y, int z) {
        return (static_cast<quint64>(static_cast<uint32_t>(z)) << 48)
             | (static_cast<quint64>(static_cast<uint32_t>(y)) << 24)
             | static_cast<uint32_t>(x);
    }
    static int selX(quint64 k) { return static_cast<int>(k & 0xffffffu); }
    static int selY(quint64 k) { return static_cast<int>((k >> 24) & 0xffffffu); }
    static int selZ(quint64 k) { return static_cast<int>((k >> 48) & 0xffffu); }

    // Kafelek pod punktem ekranu (we wspolrzednych kafelkow biezacego pietra).
    QPoint tileAtScreen(const QPointF &p) const;
    const OtbmTile *currentFloorTileAt(int x, int y) const;
    void applyRubberBand();      // zaznaczenie = baza + kafelki w prostokacie
    void updateHoverText();

    // Wspolna implementacja pedzla. asBrush=true => jesli server id nalezy do ground
    // brusha, aktywuj tryb brusha (auto-bordery); false => zawsze surowy pojedynczy item.
    void applyBrushServerId(int serverId, bool asBrush);
    void paintAt(int x, int y);          // wejscie z myszy: interpolacja linii + dedup
    // Maluje JEDEN footprint pedzla na (x,y) - ground brush (auto-bordery) albo surowy
    // item. Wolane per krok linii przez paintAt (bez wlasnego batcha/undo/refresh -
    // te robi paintAt raz na cale zdarzenie myszy).
    void paintFootprint(int x, int y);
    // Malowanie ground brushem (auto-bordery): stawia losowy ground na footprincie
    // pedzla, potem przelicza bordery na pomalowanych kafelkach i ich sasiadach.
    void paintGroundBrushAt(int cx, int cy);
    // Przelicza auto-bordery jednego kafelka (port RME doBorders przez BrushStore):
    // kasuje stare kafle bordera, wstawia nowe wg brusha srodka i 8 sasiadow.
    void recomputeBordersAt(int x, int y);

    // Malowanie wall brushem (RME WallBrush): stawia marker sciany na footprincie,
    // potem przelicza wyrownanie sciany na pomalowanych kaflach i 4 sasiadach ortog.
    void paintWallBrushAt(int cx, int cy);
    // Przelicza wyrownanie sciany jednego kafelka (port RME doWalls): jesli kafel ma
    // sciane danego brusha, dobiera wlasciwy ksztalt wg 4 sasiadow (N/W/E/S) i podmienia.
    void recomputeWallAt(int x, int y, const QString &name);
    // Czy kafel (x,y) ma sciane danego wall brusha.
    bool tileHasWallBrush(int x, int y, const QString &name) const;

    // Malowanie doodad brushem (RME DoodadBrush): na kazdym kaflu footprintu losuje
    // wariant (single = losowy item, composite = stempel wielokaflowy) i stawia.
    void paintDoodadBrushAt(int cx, int cy);

    // Malowanie STREFY (flagi kafla): ustawia/kasuje bit na kaflach footprintu.
    void paintZoneAt(int cx, int cy);
    // GUMKA na itemy: zdejmuje wierzchni item z kafli footprintu i przelicza skutki
    // (bordery gdy zniknal ground, re-join scian gdy zniknal kawalek muru).
    void eraseAt(int cx, int cy);
    // Server id ground-itemu (kategoria 0) na kafelku lub 0.
    int groundServerIdAt(const OtbmTile *tile) const;
    // Nazwa ground brusha kafelka (przez groundServerIdAt + BrushStore) lub "".
    QString groundBrushNameAt(int x, int y) const;
    // Punktowa aktualizacja po edycji JEDNEGO kafelka: dopisuje wskaznik do indeksu
    // (jesli nowy) i oznacza chunk jako oczekujacy przeliczenia (BEZ skanu calej mapy).
    // NIE przelicza od razu - patrz beginEditBatch/endEditBatch ponizej. Wolane pod
    // m_dataMutex.
    void onTileEdited(int x, int y, int z);
    // Grupuje wiele edycji (np. caly footprint pedzla) w JEDNO przeliczenie chunkow:
    // beginEditBatch() wstrzymuje przeliczanie, endEditBatch() przelicza kazdy dotkniety
    // chunk RAZ (a nie raz na kafelek - przy duzym pedzlu bylo to np. 529x ten sam chunk,
    // co zabijalo FPS przy malowaniu). Zagniezdzalne (licznik glebokosci).
    void beginEditBatch() { ++m_editBatchDepth; }
    void endEditBatch();
    // Przelicza quady wszystkich chunkow oznaczonych przez onTileEdited od ostatniego
    // flusha i czysci liste. Wymaga trzymanego m_dataMutex (wolane wewnatrz locka).
    void flushEditedChunksLocked();
    void refreshAfterEdit(uint16_t serverId); // sprawdza atlas + odswieza widok po edycji
    // Kategoria rysowania itemu wg flag .dat: 0=ground, 1=onBottom (border), 2=normal/onTop.
    int itemCategory(uint16_t serverId) const;

    OtbmReader *m_otbm = nullptr;
    OtbReader *m_otb = nullptr;
    DatReader *m_dat = nullptr;
    SprReader *m_spr = nullptr;

    int m_floor = 7;
    int m_tileSize = 32;
    qreal m_originX = 0; // lewy-gorny widoczny kafelek (w kafelkach)
    qreal m_originY = 0;
    QPointF m_lastMouse;

    // Statyczny indeks [pietro z][chunk] -> kafelki. Budowany RAZ (wczytanie/edycja),
    // NIE przy zmianie pietra - dzieki temu zmiana pietra jest tania.
    QHash<int, QHash<quint64, std::vector<const OtbmTile *>>> m_floorChunkTiles;
    // Pozycje juz obecne w m_floorChunkTiles, per pietro. Sluzy WYLACZNIE do testu
    // "czy ten kafel jest juz w indeksie" w onTileEdited. Wczesniej robil to std::find
    // po wektorze chunka - a chunk ma 32x32=1024 kafle, wiec zapelnianie go kosztowalo
    // ~500k porownan (i to raz na KAZDY postawiony item, nie raz na kafel). Przy
    // Shift+drag na duzym prostokacie to byl kwadratowy zabojca FPS.
    // Trzymane w sync z m_floorChunkTiles: budowane w buildStaticIndex, czyszczone razem.
    QHash<int, QSet<quint64>> m_floorChunkTileSet;
    // Chunki do punktowej przebudowy po edycji: pietro -> zbior kluczy chunkow.
    QHash<int, QSet<quint64>> m_dirtyFloorChunks;

    // Batchowanie edycji (patrz beginEditBatch/endEditBatch): chunki oznaczone przez
    // onTileEdited, ale jeszcze nie przeliczone (deduplikacja - jeden przelicz per
    // chunk niezaleznie od liczby edytowanych w nim kafelkow w tym batchu).
    std::set<std::pair<int, quint64>> m_pendingChunkRecompute;
    int m_editBatchDepth = 0;

    // --- Async: watek roboczy liczy quady per chunk poza watkiem renderu ---
    // m_dataMutex chroni DANE MAPY (m_floorChunkTiles, kafelki, atlas) miedzy
    // watkiem roboczym (czyta) a watkiem GUI (edycje/atlas pisza). Render czyta
    // m_floorChunkTiles tylko w sync (GUI zablokowany) - bez locka.
    // REKURENCYJNY: loadMap() trzyma lock przez caly czas m_otbm->loadFile(), ktore
    // NA KONCU synchronicznie emituje loadedChanged -> onMapLoaded() -> rebuildFloorIndex/
    // buildAtlasImage() TEZ chca ten sam lock (na tym samym watku GUI) - zwykly
    // std::mutex zrobilby tu natychmiastowy deadlock.
    // m_quadMutex chroni cache gotowych quadow (worker pisze, render czyta).
    std::recursive_mutex m_dataMutex;
    std::mutex m_quadMutex;
    // (z,chunk) -> quady. Wpisy przez shared_ptr<const...>: konsumenci (render)
    // biora wskaznik pod lockiem i iteruja PO zwolnieniu - zero kopii wektorow.
    QHash<int, QHash<quint64, std::shared_ptr<const std::vector<QuadRef>>>> m_quadCache;
    // Wersja tresci chunka (rosnie przy kazdym storeChunkQuads). Renderer trzyma VBO
    // per chunk i przebudowuje tylko gdy wersja sie zmieni. Spojna z m_quadCache
    // (klucz istnieje <=> quady w cache). Chroniona m_quadMutex.
    QHash<int, QHash<quint64, quint32>> m_chunkVer;
    std::thread m_worker;
    std::condition_variable m_reqCv;
    std::mutex m_reqMutex;
    std::deque<std::pair<int, quint64>> m_reqQueue;     // kolejka chunkow do policzenia
    std::set<std::pair<int, quint64>> m_reqPending;     // dedup (w kolejce / w toku)
    std::atomic<bool> m_workerStop{false};
    std::atomic<int> m_quadCacheVer{0};   // ++ przy kazdym storeChunkQuads

    // Efekt magiczny przy stawianiu itemu (jednorazowa animacja na itemie).
    static constexpr int kPlaceEffectId = 3;     // numer efektu (1-indeksowany)
    struct ActiveEffect { int x, y, z; qint64 startMs; };
    std::vector<ActiveEffect> m_activeEffects;
    bool m_placeEffect = true;                   // czy odpalac efekt przy stawianiu
    int m_brushSize = 0;                          // promien pedzla (0 = 1x1), jak RME
    QString m_brushShape = QStringLiteral("square");
    QElapsedTimer m_effectClock;                 // zegar do klatek animacji efektow

    // Stan widoku z poprzedniej klatki. Prefetch pozostalych pieter robimy TYLKO
    // gdy widok jest stabilny (nie podczas panningu/zmiany pietra) - inaczej
    // budowa 16 pieter obciazalaby panning. Po zatrzymaniu widoku prefetch dochodzi.
    qreal m_prevOriginX = 1e18, m_prevOriginY = 1e18;
    int m_prevFloor = -1, m_prevTileSize = -1, m_prevW = -1, m_prevH = -1;
    // Szybki lookup kafelka biezacego pietra po pozycji (do zaznaczania).
    QHash<quint64, const OtbmTile *> m_currentFloorTiles;
    int m_minTileX = 0, m_minTileY = 0, m_maxTileX = 0, m_maxTileY = 0;
    bool m_floorDirty = true;

    // --- Zaznaczanie / interakcja (jak w edytorze map) ---
    QSet<quint64> m_selected;       // pozycje zaznaczonych kafelkow (posKey)
    QSet<quint64> m_selChunks;      // chunki biezacego pietra z >=1 zaznaczonym kaflem (do bumpu wersji)
    // Czy zaznaczenie obejmuje CALE stosy (Shift+drag = region), czy tylko wierzchni
    // item (pojedynczy grab). Decyduje co przenosza/kopiuja move/copy/cut (jak RME:
    // box-select relokuje wszystko, chwyt itemu bierze tylko jego).
    // RME dragging_draw (map_display.cpp): Shift + pedzel = wypelnij PROSTOKAT miedzy
    // kafelkiem wcisniecia a puszczenia; zatwierdzane dopiero na puszczeniu, do tego
    // czasu widac tylko podglad. Doodady tego NIE maja (RME DoodadBrush::canDrag()
    // == false) - one "smaruja" stemple wzdluz sciezki, prostokat nie mialby sensu.
    bool m_dragDraw = false;
    int m_dragStartX = 0, m_dragStartY = 0;
    bool brushCanDrag() const;
    void drawDragRect(int x0, int y0, int x1, int y1);
    // Aktywne podczas wypelniania prostokata groundem: paintGroundBrushAt POMIJA
    // zbieranie kandydatow na bordery (9 insertow per kafel), bo drawDragRect wie
    // lepiej - bordery ma sens liczyc tylko na granicy prostokata (wnetrze jednolite).
    bool m_dragFillActive = false;
    // Usuwa z kafla itemy borderow zarzadzanych przez silnik (bez liczenia nowych).
    // Dla wnetrza wypelnionego prostokata to zastepuje pelny recomputeBordersAt:
    // wynik i tak bylby pusty, a pelna wersja miele QStringi 9 sasiadow per kafel.
    void cleanManagedBordersAt(int x, int y);

    bool m_selWholeStack = false;
    QSet<quint64> m_rubberBase;     // zaznaczenie sprzed biezacego przeciagania
    bool m_selecting = false;       // trwa zaznaczanie prostokatem (LPM)

    // Schowek regionu: kafle wzgledem lewego-gornego rogu zaznaczenia; dz = offset
    // pietra wzgledem PIETRA AKTYWNEGO przy kopiowaniu (wklejanie: m_floor + dz, jak
    // RME - struktura pieter podaza za kursorem). Schowek trzyma PELNE itemy (nie
    // same server-id): inaczej wklejony gold coin gubilby count, a kontener -
    // zawartosc. Niesie tez potwora i centrum spawnu kafla.
    struct ClipTile {
        int dx, dy, dz = 0;
        std::vector<OtbmMapItem> items;
        QString creature; int spawntime = 60; bool npc = false;
        int spawnRadius = 0;
    };
    std::vector<ClipTile> m_clipboard;
    bool m_pasting = false;      // tryb wklejania: podglad pod kursorem, LPM zatwierdza
    bool m_automagic = true;     // auto-bordery przy rysowaniu (RME "Border Automagic", A)
    // Poprzedni srodek widoku - dla "Go to Previous Position" (P). Zapisywany przy
    // kazdym skoku (goto/jump/center), nie przy zwyklym przewijaniu.
    int m_prevCenterX = 0, m_prevCenterY = 0, m_prevCenterZ = 0;
    bool m_prevCenterValid = false;
    // Faktyczne wklejenie schowka rogiem w (px,py) - wolane po LPM w trybie wklejania.
    void commitPasteAt(int px, int py);
    bool m_panning = false;         // trwa panning (PPM/srodkowy)

    // Plynny ruch strzalkami: sledzimy STAN klawiszy (nie auto-repeat) i przesuwamy
    // kamere co tick timera o predkosc*dt - ruch sub-kafelkowy jak map-forge/RME,
    // zamiast skokow o 2 kafelki na kazde zdarzenie auto-repeat (~30 Hz = szarpanie).
    QSet<int> m_heldArrows;
    QTimer *m_arrowTimer = nullptr;
    QElapsedTimer m_arrowClock;     // dt miedzy tickami (predkosc niezalezna od FPS)
    int m_brushServerId = 0;        // aktywny pedzel (0 = zaznaczanie)
    BrushStore *m_brushStore = nullptr;  // silnik ground brushy (auto-bordery)
    CreatureStore *m_creatureStore = nullptr; // lista potworow (creatures.xml)
    QString m_creatureBrush;             // nazwa potwora do stawiania ("" = brak)
    bool m_spawnBrush = false;           // pedzel centrum spawnu
    int m_creatureSpawntime = 60;        // spawntime stawianych potworow (s)
    int m_spawnBrushRadius = 3;          // promien stawianych spawnow (kafle)
    bool m_torchOn = false;              // przelacznik OSWIETLENIA (pochodnia w topbar)
    // Bufor swiatel widocznego zakresu (RGBA, 1px = 1 kafel) + jego polozenie.
    std::vector<uint32_t> m_lightPixels;
    int m_lightTX = 0, m_lightTY = 0, m_lightTW = 0, m_lightTH = 0;
    quint32 m_lightVersion = 0;
    bool m_lightDirty = true;
    int m_lightAmbient = 40;             // 0-255; poziom swiatla otoczenia ("noc")
    // Cache swiatla per CHUNK WIDOKU (jak TIME LightCache): bufor 32x32 RGBA z juz
    // zmiksowanym ambientem + wszystkimi swiatlami (multi-floor z projekcja).
    // Edycja unieważnia tylko dotkniety chunk + sasiadow, nie caly widok - inaczej
    // stawianie kazdego kafla przeliczaloby cale oswietlenie ekranu.
    QHash<quint64, std::vector<uint32_t>> m_lightChunks;
    void computeLightChunk(int cx, int cy, std::vector<uint32_t> &out) const;
    void invalidateLightAround(int x, int y, int z);   // z onTileEdited
    int m_selectionFloors = 0;           // 0=Current, 1=Lower, 2=Visible (box-select)
    bool m_compensatedSelect = true;     // RME: domyslnie wlaczone
    int m_houseBrush = 0;                // id malowanego domu (0 = brak)
    bool m_houseExitMode = false;        // klik = ustaw wejscie aktywnego domu
    void placeHouseAt(int x, int y);
    QSet<int> m_ensuredOutfits;          // looktype'y ze sprite'ami juz w atlasie
    void ensureOutfitSprites(int lookType);
    void placeSpawnAt(int x, int y);
    void placeCreatureBrushAt(int x, int y);
    bool tileInAnySpawn(int x, int y) const;   // czy kafel lezy w promieniu centrum
    // Centra spawnow biezacego pietra {x, y, radius} - cache przebudowywany leniwie.
    // Instancje markerow generowane per klatka z TEJ listy (mala, wiec tanio) -
    // dzieki temu tint zaznaczenia reaguje na selekcje bez zadnych rebuildow.
    // tileInAnySpawn tez liczy po tej liscie (nie skanem 31x31 kafli per kafel).
    struct SpawnCenter { int x, y, r; };
    std::vector<SpawnCenter> m_spawnCentersFloor;
    bool m_spawnMarksDirty = true;
    void rebuildSpawnMarks();
    // Dopisuje JEDEN nowy spawn punktowo (bez pelnego rebuildu calego pietra).
    // Malowanie potworow po dziewiczym terenie tworzy auto-spawn per kafel -
    // pelny rebuild per kafel zjadal FPS.
    void appendSpawnMark(int x, int y, int r);
    QString m_activeGroundBrush;    // nazwa aktywnego ground brusha ("" = zwykly item)
    QString m_activeWallBrush;      // nazwa aktywnego wall brusha ("" = nie sciana)
    QString m_activeDoodadBrush;    // nazwa aktywnego doodad brusha ("" = nie doodad)
    int m_doodadVariant = -1;       // wybrany wariant doodada (R rotuje; -1 = losowy)
    // Deduplikacja w obrebie CALEGO pociagniecia (press->release; czyszczone przy
    // wcisnieciu LPM). Footprinty kafli linii mocno na siebie zachodza, a przy
    // "wezykowaniu" linia wraca po tych samych kaflach - bez dedupu surowy item byl
    // DOKLADANY na stos przy kazdej rewizycie (dziesiatki duplikatow na kaflu), przez
    // co snapshoty undo i przeliczenia chunkow rosly lawinowo => spadek do kilku FPS.
    // QSet<posKey>, NIE std::set<pair>: te zbiory dostaja setki tysiecy insertow przy
    // Shift+drag (kazdy kafel prostokata + 9 kafli borderow na kafel), a std::set
    // alokuje wezel drzewa na kazdy insert i szuka w O(log n). Hash na gotowym kluczu
    // 64-bit jest tanszy i bez alokacji per wpis.
    QSet<quint64> m_strokePlaced;       // kafle juz obsluzone w pociagnieciu
    QSet<quint64> m_strokeBorderTiles;  // bordery do przeliczenia (raz/zdarzenie)
    // Cache nazwy ground brusha per kafel, aktywny TYLKO na czas przebiegu borderow
    // (paintAt/drawDragRect). recomputeBordersAt pyta o 9 nazw per kafel, a sasiednie
    // kafle bordera dziela sasiadow - bez cache te same nazwy (lookup kafla + hash
    // serverId->QString + kopia QString) liczyly sie ~9x kazda w jednym zdarzeniu
    // myszy, co przy szybkim ciagnieciu duzym pedzlem dawalo widoczna scinke.
    // Cache jest POPRAWNY przez caly przebieg: recomputeBordersAt zmienia wylacznie
    // itemy borderow (nie ground), wiec nazwy brushy kafli sie nie zmieniaja.
    mutable QHash<quint64, QString> m_groundNameCache;
    mutable bool m_groundNameCacheOn = false;
    bool m_selectionMode = true;    // tryb zaznaczania (RME domyslnie SELECTION_MODE); wybor pedzla -> drawing
    quint32 m_activeZone = 0;       // aktywny pedzel strefy (flaga OTBM); 0 = brak
    bool m_eraseMode = false;       // trwaly tryb gumki (przycisk "Erase") - itemy i strefy
    bool m_eraseStroke = false;     // kasowanie dla BIEZACEGO pociagniecia (tryb LUB Ctrl)
    // Tryb masowej edycji (malowanie ground brushem): placeItemAt POMIJA per-item
    // refreshAfterEdit (emit contentUpdated + update) - inaczej pociagniecie pedzlem
    // emitowaloby dziesiatki sygnalow na ruch myszy = drastyczny spadek FPS. Jedno
    // odswiezenie robi paintGroundBrushAt na koncu.
    bool m_bulkEdit = false;
    bool m_painting = false;        // trwa malowanie (LPM z aktywnym pedzlem)
    int m_paintLastX = -2000000, m_paintLastY = -2000000; // ostatnio pomalowany kafelek
    int m_anchorX = 0, m_anchorY = 0;   // poczatek prostokata zaznaczenia
    int m_rubberX = 0, m_rubberY = 0;   // biezacy rog prostokata zaznaczenia
    int m_hoverX = -1, m_hoverY = -1;   // kafelek pod kursorem (-1 = brak)
    int m_contextX = 0, m_contextY = 0; // kafelek klikniety PPM (dla menu)
    // Przenoszenie itemu lewym przyciskiem (drag z kafelka z itemem).
    bool m_movingSel = false;
    bool m_moveMoved = false;           // kursor opuscil kafelek zrodlowy
    int m_moveSrcX = 0, m_moveSrcY = 0;
    int m_moveServerId = 0;             // server id przenoszonego itemu (duch)
    QString m_hoverText;

    // Atlas (CPU): obraz + sloty. m_spriteToSlot mapuje sprite_id -> indeks slotu.
    QImage m_atlasImage;
    int m_atlasGeneration = 0;   // ++ po kazdej przebudowie atlasu (dla MapGLView)
    int m_dataVersion = 0;       // ++ po kazdej zmianie danych mapy (edycja/indeks)
    // Drugi atlas: te same sprite'y przekolorowane na szary polprzezroczysty
    // sylwet (alpha oryginalu * wspolczynnik) - do podswietlania itemow.
    QImage m_highlightImage;
    QHash<uint32_t, int> m_spriteToSlot;
    // Server id, dla ktorych sprite'y sa juz w atlasie. ensureItemSprites jest wolane
    // per POSTAWIONY item (przy Shift+drag setki tysiecy razy z tym samym id) i bez
    // cache za kazdym razem budowalo QSet ze sprite_ids. Atlas jest przyrostowy
    // (m_spriteToSlot nigdy nie maleje), wiec "raz zapewniony" zostaje prawdziwe;
    // czyszczone w buildAtlasImage razem z ewentualna przebudowa.
    QSet<int> m_ensuredServerIds;
    std::vector<QRect> m_atlasSlots; // prostokat slotu w px wewnatrz atlasu
    bool m_atlasDirty = true;

    // Oba domyslnie WLACZONE: nizsze pietra widoczne i przyciemnione. Przelaczniki
    // zostaly tylko w menu View (Q / Ctrl+W) - w topbarze byly zbedne.
    bool m_showLowerFloors = true;
    bool m_showShade = true;
};

#endif // MAPVIEW_H
