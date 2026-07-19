#include "mapglview.h"
#include "mapview.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QMatrix4x4>
#include <QImage>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <memory>

namespace {

class MapGLRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLExtraFunctions
{
public:
    MapGLRenderer() { initializeOpenGLFunctions(); initGL(); }

    ~MapGLRenderer() override
    {
        delete m_prog;
        delete m_flatProg;
        delete m_cursorProg;
        delete m_lightProg;
        m_cursorVbo.destroy();
        m_spawnVbo.destroy();
        m_spawnSelVbo.destroy();
        if (m_tex) glDeleteTextures(1, &m_tex);
        if (m_lightTexId) glDeleteTextures(1, &m_lightTexId);
        m_quadVbo.destroy();
        for (auto &floor : m_chunkBufs)
            for (auto &kv : floor) kv.second->vbo.destroy();
        m_fxVbo.destroy();
        m_selVbo.destroy();
        m_ghostVbo.destroy();
        m_borderVbo.destroy();
        m_vao.destroy();
        m_flatVao.destroy();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        m_fbo = size;
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setSamples(0);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    // Watek renderu, GUI zablokowany - bezpiecznie czytamy dane z MapView.
    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *view = static_cast<MapGLView *>(item);
        view->countFrame();
        MapView *src = view->source();
        if (!src) { for (auto &l : m_drawList) l.clear(); return; }

        // 1) Atlas -> tekstura (tylko gdy sie zmienil).
        const int gen = src->glAtlasGeneration();
        if (gen != m_atlasGen) { m_atlasGen = gen; uploadAtlas(src->glAtlasImage()); }

        // 2) Macierz world(px) -> NDC (ortho y-up: FBO jest pokazywany odwrocony).
        const qreal w = item->width(), h = item->height();
        const int ts = src->tileSize();
        const float scale = static_cast<float>(ts) / 32.0f;
        // Przesuniecie kamery liczymy w pikselach EKRANU i ZAOKRAGLAMY do calego
        // piksela. Bez tego przy plynnym (ulamkowym) przewijaniu strzalkami sprite'y
        // laduja na ulamkowych pozycjach i przy GL_NEAREST migocza (probkowanie
        // texeli skacze co klatke). Snap = pixel-perfect, zero migotania.
        // Filtr atlasu: NEAREST tylko przy skali CALKOWITEJ (1:1, 2:1, 3:1) - wtedy
        // texele mapuja sie 1:1 na piksele i sa ostre. Przy skali ULAMKOWEJ (np. 1.19)
        // nearest powoduje migotanie podczas panningu (texele skacza) - tam LINEAR
        // wyglada gladko. Inset 0.5 texela w shaderze chroni przed przeciekiem sprite'ow.
        m_useLinear = !(scale >= 1.0f && std::fabs(scale - std::round(scale)) < 0.01f);

        const float tpx = std::round(static_cast<float>(src->glOriginX()) * ts);
        const float tpy = std::round(static_cast<float>(src->glOriginY()) * ts);
        m_matrix.setToIdentity();
        m_matrix.ortho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h), -1.0f, 1.0f);
        m_matrix.translate(-tpx, -tpy, 0.0f);   // ekran-px, calkowite
        m_matrix.scale(scale, scale, 1.0f);      // world(32px) -> ekran

        // 3) Biezace pietro -> tylko dobor ktore bufory rysowac (render()).
        // ZMIANA PIETRA nie rusza buforow - to klucz do plynnosci.
        m_curFloor = src->floor();
        m_botFloor = src->glBottomFloor();
        m_showShade = src->glShowShade();

        // 4) Widoczny prostokat CHUNKOW (+margines). Kazdy chunk ma wlasny maly VBO -
        // edycja przebudowuje tylko dotkniety chunk (jego wersja rosnie), a nie caly
        // widoczny bufor pietra. To klucz do braku spadku FPS przy malowaniu.
        const int spill = 4, chunk = 32;
        auto fdiv = [](int a, int b) { int q = a / b, r = a % b;
            if (r != 0 && ((r < 0) != (b < 0))) --q; return q; };
        const double ox = src->glOriginX(), oy = src->glOriginY();
        const int minCX = fdiv(static_cast<int>(std::floor(ox)) - spill, chunk);
        const int minCY = fdiv(static_cast<int>(std::floor(oy)) - spill, chunk);
        const int maxCX = fdiv(static_cast<int>(std::ceil(ox + w / ts)) + 1, chunk);
        const int maxCY = fdiv(static_cast<int>(std::ceil(oy + h / ts)) + 1, chunk);

        // LOD jak RME: przy mocnym oddaleniu (male kafelki) rysuj TYLKO podloge.
        const bool groundOnly = (ts <= 4);

        const int wMin = m_curFloor;
        const int wMax = m_botFloor;

        // 5) Dla kazdego rysowanego pietra: zbuduj liste widocznych chunkow do rysowania,
        // (prze)budowujac VBO tylko tych, ktore sa nowe albo zmienily tresc.
        std::vector<float> tmp;
        for (int f = 0; f < 16; ++f) m_drawList[f].clear();

        // Czy w widocznym zakresie jest cos jeszcze niepoliczone przez watek roboczy -
        // jesli tak, na koncu wymuszamy KOLEJNA klatke (patrz nizej), zamiast polegac
        // WYLACZNIE na tym, ze watek roboczy "obudzi" render po skonczeniu chunka
        // (ten tor budzenia bywa zawodny - stad "itemy pojawiaja sie dopiero po
        // kliknieciu", bo dopiero wtedy cokolwiek wymuszalo kolejny sync()).
        bool anyPending = false;

        for (int z = wMin; z <= wMax; ++z) {
            auto &bufs = m_chunkBufs[z];
            auto &list = m_drawList[z];
            for (int cy = minCY; cy <= maxCY; ++cy)
                for (int cx = minCX; cx <= maxCX; ++cx) {
                    const quint64 key = (static_cast<quint64>(static_cast<quint32>(cx)) << 32)
                                      |  static_cast<quint64>(static_cast<quint32>(cy));
                    const quint32 ver = src->glChunkVersion(z, key);
                    if (ver == MapView::kChunkEmpty) continue;    // nic do rysowania
                    if (ver == MapView::kChunkPending) {          // trzymaj stary VBO, zlec liczenie
                        src->glRequestChunk(z, key);
                        anyPending = true;
                        auto it = bufs.find(key);
                        if (it != bufs.end() && it->second->count > 0) list.push_back(key);
                        continue;
                    }
                    std::unique_ptr<ChunkBuf> &cb = bufs[key];
                    if (!cb) cb = std::make_unique<ChunkBuf>();
                    if (!(cb->valid && cb->version == ver && cb->groundOnly == groundOnly)) {
                        const quint32 got = src->glCollectChunkInstances(z, key, groundOnly, tmp);
                        // 6 floatow/instancje: x,y,slotX,slotY,selected,zoneFlags
                        // (musi byc zgodne z glCollectChunkInstances i drawFloor!).
                        cb->count = static_cast<int>(tmp.size() / 6);
                        if (!cb->vbo.isCreated()) cb->vbo.create();
                        cb->vbo.bind();
                        cb->vbo.allocate(tmp.empty() ? nullptr : tmp.data(),
                                         static_cast<int>(tmp.size() * sizeof(float)));
                        cb->vbo.release();
                        cb->version = (got == MapView::kChunkPending) ? ver : got;
                        cb->groundOnly = groundOnly;
                        cb->valid = true;
                    }
                    if (cb->count > 0) list.push_back(key);
                }
        }

        // Eksmisja chunkow poza widokiem (ogranicza pamiec) - tylko gdy widok sie ruszyl.
        const bool viewMoved = minCX != m_lastMinCX || minCY != m_lastMinCY
                            || maxCX != m_lastMaxCX || maxCY != m_lastMaxCY;
        if (viewMoved) {
            const int m = 8;   // margines chunkow trzymanych poza ekranem
            for (auto &bufs : m_chunkBufs) {
                for (auto it = bufs.begin(); it != bufs.end(); ) {
                    const int cx = static_cast<int>(static_cast<qint32>(it->first >> 32));
                    const int cy = static_cast<int>(static_cast<qint32>(it->first & 0xffffffffu));
                    if (cx < minCX - m || cx > maxCX + m || cy < minCY - m || cy > maxCY + m) {
                        it->second->vbo.destroy();
                        it = bufs.erase(it);
                    } else ++it;
                }
            }
            m_lastMinCX = minCX; m_lastMinCY = minCY; m_lastMaxCX = maxCX; m_lastMaxCY = maxCY;
        }

        // Nakladki (dynamiczne, co klatke): efekty + sylwet zaznaczenia + duch pedzla.
        auto uploadDyn = [&](QOpenGLBuffer &vbo, const std::vector<float> &data, int &count) {
            count = static_cast<int>(data.size() / 4);
            if (count > 0) {
                if (!vbo.isCreated()) vbo.create();
                vbo.bind();
                vbo.allocate(data.data(), static_cast<int>(data.size() * sizeof(float)));
                vbo.release();
            }
        };
        src->glCollectEffectInstances(m_fxInst);          uploadDyn(m_fxVbo, m_fxInst, m_fxCount);
        // Zaznaczenie NIE jest juz osobna warstwa (tintowane w glownym przebiegu, aSel).
        src->glCollectGhostInstances(m_ghostInst);        uploadDyn(m_ghostVbo, m_ghostInst, m_ghostCount);

        // Prostokat zaznaczania (Shift/Ctrl + przeciagniecie) - rysowany jako plaski
        // wypelniony kwadrat + obrys, nie sprite'y (patrz render()).
        double rx0, ry0, rx1, ry1;
        m_rubberActive = src->glRubberBandRect(rx0, ry0, rx1, ry1);
        if (m_rubberActive) { m_rubberRect[0]=rx0; m_rubberRect[1]=ry0; m_rubberRect[2]=rx1; m_rubberRect[3]=ry1; }

        // Kursor pedzla: PER-KAFEL (2 floaty/instancje), nie prostokat otaczajacy -
        // inaczej pedzel "kolo" pokazywalby kwadrat (patrz glCollectBrushCursorInstances).
        src->glCollectBrushCursorInstances(m_cursorInst);
        m_cursorCount = static_cast<int>(m_cursorInst.size() / 4);   // x,y,w,h per instancja
        if (m_cursorCount > 0) {
            if (!m_cursorVbo.isCreated()) m_cursorVbo.create();
            m_cursorVbo.bind();
            m_cursorVbo.allocate(m_cursorInst.data(),
                                 static_cast<int>(m_cursorInst.size() * sizeof(float)));
            m_cursorVbo.release();
        }

        // Oswietlenie (jak TIME): bufor 1px=1kafel przeliczany leniwie w MapView;
        // re-upload tekstury tylko gdy wersja wzrosla. Sam upload robi render()
        // (tam na pewno jest kontekst GL).
        {
            const quint32 lv = src->glUpdateLightGrid();
            src->lightRect(m_lightTX, m_lightTY, m_lightTW, m_lightTH);
            if (lv != m_lightVer) {
                m_lightVer = lv;
                m_lightBuf = src->lightPixels();   // kopia pod zablokowanym GUI
                m_lightUpload = true;
            }
        }

        // Siatka kafli (Show grid) - linie jako plaskie rect-y, format jak kursor.
        src->glCollectGridInstances(m_gridInst);
        uploadDyn(m_gridVbo, m_gridInst, m_gridCount);

        // Kafle stref/domow bez itemow - plaskie kwadraty (patrz MapView).
        src->glCollectZoneMarkInstances(m_zoneHouseInst, m_zoneFlagInst);
        uploadDyn(m_zoneHouseVbo, m_zoneHouseInst, m_zoneHouseCount);
        uploadDyn(m_zoneFlagVbo, m_zoneFlagInst, m_zoneFlagCount);

        // Markery spawnow (centrum + obrys promienia) - ten sam format i program co
        // kursor, osobne VBO i kolory (fiolet; zaznaczone przyciemnione jak itemy).
        src->glCollectSpawnMarkInstances(m_spawnInst, m_spawnSelInst);
        m_spawnCount = static_cast<int>(m_spawnInst.size() / 4);
        if (m_spawnCount > 0) {
            if (!m_spawnVbo.isCreated()) m_spawnVbo.create();
            m_spawnVbo.bind();
            m_spawnVbo.allocate(m_spawnInst.data(),
                                static_cast<int>(m_spawnInst.size() * sizeof(float)));
            m_spawnVbo.release();
        }
        m_spawnSelCount = static_cast<int>(m_spawnSelInst.size() / 4);
        if (m_spawnSelCount > 0) {
            if (!m_spawnSelVbo.isCreated()) m_spawnSelVbo.create();
            m_spawnSelVbo.bind();
            m_spawnSelVbo.allocate(m_spawnSelInst.data(),
                                   static_cast<int>(m_spawnSelInst.size() * sizeof(float)));
            m_spawnSelVbo.release();
        }

        // Jesli w widocznym zakresie jest jeszcze cos niepoliczone przez watek roboczy,
        // wymuszamy KOLEJNA klatke SAMI (Renderer::update() - oficjalny, watkowo-bezpieczny
        // sposob wg dokumentacji Qt na "obudz render, gdy dane async sa gotowe").
        // Bez tego jedynym torem budzenia bylo posrednie MapView::update() wolane przez
        // watek roboczy po KAZDYM ukonczonym chunku - gdy Qt Quick zdazylo juz wejsc w
        // stan bezczynnosci (render loop idle) miedzy jednym a drugim wywolaniem, klatki
        // przestawaly sie odswiezac samoistnie i itemy zostawaly "w zawieszeniu" (widac
        // ID/dane, brak grafiki) az do nastepnej interakcji uzytkownika (klik/pan/zoom),
        // ktora przypadkiem wymuszala kolejny sync() i dociagala kilka gotowych miedzyczasie
        // chunkow. To wlasnie ten efekt "klikam i nagle kilka itemow sie doladowuje".
        if (anyPending) update();
    }

    void render() override
    {
        glViewport(0, 0, m_fbo.width(), m_fbo.height());
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // FBO ma byc KRYJACE wszedzie (alpha=1 z clear) - blokujemy zapisy alpha na
        // CALY render. Bez tego krawedzie sprite'ow przy filtrze LINEAR (ulamkowy
        // zoom) zapisywaly ulamkowa alphe (dst_a = a^2 + dst*(1-a) < 1 nawet nad
        // kryjacym groundem), a Qt Quick komponuje FBO nad kamiennym panelem - kazdy
        // texel z alpha<1 przepuszczal jasna teksture panelu. Za dnia niewidoczne,
        // noca (lighting) wychodzilo jako BIALE OBRYSY na krawedziach itemow.
        // Maska nie psuje blendingu: wspolczynniki SRC_ALPHA biora alphe FRAGMENTU,
        // ColorMask gate'uje tylko ZAPIS do bufora.
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

        if (!m_prog || !m_prog->isLinked() || !m_tex) return;

        m_prog->bind();
        m_prog->setUniformValue("uMatrix", m_matrix);
        m_prog->setUniformValue("uAtlasSize", QVector2D(m_atlasW, m_atlasH));
        m_prog->setUniformValue("uSprite", 32.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        // Filtr zalezny od zoomu (patrz synchronize): ostry przy skali calkowitej,
        // liniowy (bez migotania) przy ulamkowej.
        const GLint filt = m_useLinear ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
        m_prog->setUniformValue("uAtlas", 0);

        m_vao.bind();

        // Piętra: 5 floatow/instancje (x,y,slotX,slotY,selected) - aSel(loc2) wlaczony,
        // fragment sciemnia zaznaczone kafle w miejscu.
        // Stride 6 floatow: x,y,slotX,slotY | selected | zoneFlags.
        auto drawFloor = [&](QOpenGLBuffer &vbo, int count) {
            if (count <= 0 || !vbo.isCreated()) return;
            const int stride = 6 * sizeof(float);
            vbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
            glVertexAttribDivisor(1, 1);
            glEnableVertexAttribArray(2);   // aSel
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                                  reinterpret_cast<void *>(4 * sizeof(float)));
            glVertexAttribDivisor(2, 1);
            glEnableVertexAttribArray(3);   // aZone
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                                  reinterpret_cast<void *>(5 * sizeof(float)));
            glVertexAttribDivisor(3, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
        };
        // Nakladki (efekty/duch): 4 floaty/instancje, bez zaznaczenia (aSel=0 na stale).
        auto drawOverlay = [&](QOpenGLBuffer &vbo, int count) {
            if (count <= 0 || !vbo.isCreated()) return;
            glDisableVertexAttribArray(2);
            glVertexAttrib1f(2, 0.0f);      // brak zaznaczenia
            glDisableVertexAttribArray(3);
            glVertexAttrib1f(3, 0.0f);      // brak strefy
            vbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
        };

        // Mapa: pelny kolor. Od najglebszego pietra (pod spodem) do biezacego (na wierzchu).
        // Kazde pietro = zbior malych VBO chunkow (draw-lista zbudowana w synchronize).
        // Piatra PONIZEJ biezacego (z > m_curFloor) najpierw - miedzy nimi a biezacym
        // wchodzi shade (patrz nizej), jak w RME.
        m_prog->setUniformValue("uTint", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        for (int z = m_botFloor; z > m_curFloor; --z) {
            if (z < 0 || z > 15 || m_drawList[z].empty()) continue;
            const float off = static_cast<float>((z - m_curFloor) * 32);
            m_prog->setUniformValue("uFloorOff", QVector2D(off, off));
            auto &bufs = m_chunkBufs[z];
            for (quint64 key : m_drawList[z]) {
                auto it = bufs.find(key);
                if (it != bufs.end()) drawFloor(it->second->vbo, it->second->count);
            }
        }

        // Shade (Q / "Show lower floors") - jak RME MapDrawer::DrawShade: JEDEN plaski
        // czarny prostokat na caly ekran (alpha 128/255), wstawiony MIEDZY nizsze
        // pietra a biezace. Rysowany raz (nie per-pietro!), wiec cokolwiek jest pod
        // nim (nizsze pietra, juz narysowane wyzej) wyglada przyciemnione przez jedna
        // "szybe", a biezace pietro (rysowane PO tym) zostaje w pelni jasne.
        if (m_botFloor != m_curFloor && m_showShade && m_flatProg && m_flatProg->isLinked()) {
            m_flatProg->bind();
            QMatrix4x4 identity;   // brak transformacji - rysujemy wprost w NDC (caly ekran)
            m_flatProg->setUniformValue("uMatrix", identity);
            m_flatProg->setUniformValue("uRect", QVector4D(-1.0f, -1.0f, 1.0f, 1.0f));
            m_flatProg->setUniformValue("uColor", QVector4D(0.0f, 0.0f, 0.0f, 128.0f / 255.0f));
            // Maska alpha jest juz ustawiona GLOBALNIE na poczatku render() (patrz
            // komentarz przy glClear) - shade nie musi nia zarzadzac.
            m_flatVao.bind();
            m_quadVbo.bind();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            m_flatVao.release();
            m_flatProg->release();

            // Wracamy do programu/VAO sprite'ow - flatVao.release() odwiazuje VAO (0),
            // wiec trzeba je odtworzyc przed dalszymi rysowaniami instancjonowanymi.
            m_prog->bind();
            m_vao.bind();
            m_prog->setUniformValue("uTint", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        }

        // Biezace pietro - zawsze w pelni jasne, na wierzchu shade'a.
        {
            const int z = m_curFloor;
            if (z >= 0 && z <= 15 && !m_drawList[z].empty()) {
                m_prog->setUniformValue("uFloorOff", QVector2D(0.0f, 0.0f));
                auto &bufs = m_chunkBufs[z];
                for (quint64 key : m_drawList[z]) {
                    auto it = bufs.find(key);
                    if (it != bufs.end()) drawFloor(it->second->vbo, it->second->count);
                }
            }
        }

        // Oswietlenie (multiply, jak TIME LightOverlay: GL_DST_COLOR * GL_ZERO) -
        // NA mapie, ale POD nakladkami edytorskimi (kursor/markery czytelne w nocy).
        if (m_lightTW > 0 && m_lightProg && m_lightProg->isLinked()) {
            if (m_lightUpload) {
                m_lightUpload = false;
                if (m_lightTexId == 0) {
                    glGenTextures(1, &m_lightTexId);
                    glBindTexture(GL_TEXTURE_2D, m_lightTexId);
                    // LINEAR: 1px = 1 kafel, interpolacja daje miekkie przejscia swiatla.
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                } else {
                    glBindTexture(GL_TEXTURE_2D, m_lightTexId);
                }
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_lightTW, m_lightTH, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, m_lightBuf.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            if (m_lightTexId != 0) {
                m_lightProg->bind();
                m_lightProg->setUniformValue("uMatrix", m_matrix);
                // uRect = (x0,y0,x1,y1) w world-px, jak flatProg.
                m_lightProg->setUniformValue("uRect",
                    QVector4D(m_lightTX * 32.0f, m_lightTY * 32.0f,
                              (m_lightTX + m_lightTW) * 32.0f, (m_lightTY + m_lightTH) * 32.0f));
                m_lightProg->setUniformValue("uTex", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_lightTexId);
                glBlendFunc(GL_DST_COLOR, GL_ZERO);   // multiply
                // IZOLOWANY VAO (jak shade pass) - NIE m_vao sprite'ow: mieszanie
                // psuloby stan atrybutow glownego VAO i kompozycje Qt Quick (cale UI
                // bielalo). m_quadVbo = 6 wierzcholkow corner [0,0]..[1,1].
                m_flatVao.bind();
                m_quadVbo.bind();
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                m_flatVao.release();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // przywroc standard
                glBindTexture(GL_TEXTURE_2D, 0);   // atlas re-bindnie kolejny pass
                m_lightProg->release();
                // flatVao.release() odbindowal VAO - nakladki (fx/kursor) rysuja
                // instancjonowanie przez m_vao, wiec odtworz jak po shade passie.
                m_prog->bind();
                m_vao.bind();
                m_prog->setUniformValue("uTint", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }

        // Nakladki - biezace pietro, offset 0. Zaznaczenie NIE jest juz osobna warstwa -
        // tintowane w glownym przebiegu wyzej (flaga aSel per-instancja).
        m_prog->setUniformValue("uFloorOff", QVector2D(0.0f, 0.0f));
        // Efekty magiczne (pelny kolor, na wierzchu).
        drawOverlay(m_fxVbo, m_fxCount);
        // Duch przenoszonego zaznaczenia - SCIEMNIONY i polprzezroczysty (jak RME).
        m_prog->setUniformValue("uTint", QVector4D(0.5f, 0.5f, 0.5f, 0.55f));
        drawOverlay(m_ghostVbo, m_ghostCount);

        m_vao.release();
        m_prog->release();

        // Prostokat zaznaczania (Shift/Ctrl + przeciagniecie): plaski szary, bez
        // tekstury - osobny minimalny shader (fill polprzezroczysty + jasniejszy obrys).
        if (m_rubberActive && m_flatProg && m_flatProg->isLinked()) {
            m_flatProg->bind();
            m_flatProg->setUniformValue("uMatrix", m_matrix);
            m_flatProg->setUniformValue("uRect", QVector4D(m_rubberRect[0], m_rubberRect[1],
                                                            m_rubberRect[2], m_rubberRect[3]));
            m_flatVao.bind();
            // Wypelnienie: szary, mocno przezroczysty.
            m_flatProg->setUniformValue("uColor", QVector4D(0.6f, 0.6f, 0.6f, 0.18f));
            m_quadVbo.bind();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            // Obrys: szary, mniej przezroczysty niz wypelnienie.
            m_flatProg->setUniformValue("uColor", QVector4D(0.75f, 0.75f, 0.75f, 0.85f));
            m_borderVbo.bind();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            glLineWidth(1.5f);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            m_flatVao.release();
            m_flatProg->release();
        }

        // Kursor aktywnego pedzla: po JEDNYM polprzezroczystym kwadracie na KAFEL
        // footprintu (jak RME DrawBrush) - dzieki temu pedzel "kolo" widac jako kolo.
        // Jeden instancjonowany draw call, wiec nawet promien 11 (~380 kafli) jest tani.
        // Markery spawnow POD kursorem pedzla (kursor ma byc zawsze widoczny).
        // Dwa passy: zwykle + zaznaczone (przyciemniony fiolet, jak tint itemow).
        auto drawSpawnMarks = [&](QOpenGLBuffer &vbo, int count, const QVector4D &color) {
            if (count <= 0 || !m_cursorProg || !m_cursorProg->isLinked()) return;
            m_cursorProg->bind();
            m_cursorProg->setUniformValue("uMatrix", m_matrix);
            m_cursorProg->setUniformValue("uColor", color);
            m_cursorProg->setUniformValue("uBorder", 0.0f);   // spawn marks: plaskie
            m_vao.bind();
            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(3);
            vbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
            m_vao.release();
            m_cursorProg->release();
        };
        // Kafle stref/domow bez itemow: kolory zblizone do tintu shadera (dom =
        // niebieskawy, strefa = zielonkawy PZ; szczegolowe flagi i tak widac dopiero
        // gdy kafel dostanie podloge). POD siatka i markerami spawnow.
        drawSpawnMarks(m_zoneHouseVbo, m_zoneHouseCount, QVector4D(0.25f, 0.35f, 0.9f, 0.26f));
        drawSpawnMarks(m_zoneFlagVbo, m_zoneFlagCount, QVector4D(0.2f, 0.75f, 0.3f, 0.20f));

        // Siatka POD markerami spawnow (siatka to najnizsza nakladka edytorska).
        drawSpawnMarks(m_gridVbo, m_gridCount, QVector4D(0.0f, 0.0f, 0.0f, 0.35f));
        drawSpawnMarks(m_spawnVbo, m_spawnCount, QVector4D(0.72f, 0.35f, 0.86f, 0.45f));
        drawSpawnMarks(m_spawnSelVbo, m_spawnSelCount, QVector4D(0.36f, 0.17f, 0.43f, 0.6f));

        if (m_cursorCount > 0 && m_cursorProg && m_cursorProg->isLinked()) {
            m_cursorProg->bind();
            m_cursorProg->setUniformValue("uMatrix", m_matrix);
            // Szary fill + jasniejszy obrys, jak prostokat przeciagniecia (rubber band).
            m_cursorProg->setUniformValue("uColor", QVector4D(0.6f, 0.6f, 0.6f, 0.18f));
            m_cursorProg->setUniformValue("uBorderColor", QVector4D(0.8f, 0.8f, 0.8f, 0.75f));
            m_cursorProg->setUniformValue("uBorder", 1.0f);
            m_vao.bind();   // aCorner (loc0) juz wpiete z m_quadVbo
            // loc2/loc3 (aSel/aZone) sa z programu sprite'ow - tu nieuzywane, wylacz.
            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(3);
            m_cursorVbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, m_cursorCount);
            m_vao.release();
            m_cursorProg->release();
        }

        // Przywroc pelna maske kolorow - nie zostawiamy zmienionego stanu GL po sobie
        // (Qt Quick wspoldzieli kontekst z wlasnym rendererem scene grapha).
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

private:
    void initGL()
    {
        m_prog = new QOpenGLShaderProgram;
        m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;   // [0,0]..[1,1]
            layout(location=1) in vec4 aInst;     // x,y (world px), slotX,slotY (atlas px)
            layout(location=2) in float aSel;     // 1 = kafel zaznaczony (tint w fragmencie)
            layout(location=3) in float aZone;    // flagi strefy kafla (PZ/PvP/...) - tint podlogi
            out vec2 vUV;
            out float vSel;
            out float vZone;
            uniform mat4 uMatrix;
            uniform vec2 uAtlasSize;
            uniform float uSprite;
            uniform vec2 uFloorOff;   // offset ukosny pietra (px)
            void main() {
                vec2 worldPx = aInst.xy + uFloorOff + aCorner * uSprite;
                gl_Position = uMatrix * vec4(worldPx, 0.0, 1.0);
                vec2 uv = aInst.zw + vec2(0.5) + aCorner * (uSprite - 1.0);
                vUV = uv / uAtlasSize;
                vSel = aSel;
                vZone = aZone;
            }
        )");
        m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            in vec2 vUV;
            in float vSel;
            in float vZone;
            out vec4 FragColor;
            uniform sampler2D uAtlas;
            uniform vec4 uTint;   // (1,1,1,1)=mapa; (0,0,0,a)=przyciemnienie; (1,1,1,a)=duch
            // Strefy tintuja PODLOGE mnoznikami 1:1 z RME map_drawer.cpp DrawTile:
            //   Dom(0x40): r/2,g/2 (niebieski) PZ(0x01): r/2,b/2 (zielony)
            //   PvP(0x10): r,b *2/3 (pomaranczowy)  NoLogout(0x08): b/2 (zolty)
            //   NoPvP(0x04): g/2 (rozowy)
            vec3 zoneTint(float zf) {
                int f = int(zf + 0.5);
                vec3 c = vec3(1.0);
                // Dom i "goly" PZ wzajemnie sie wykluczaja (RME if/else if) - dom ma
                // pierwszenstwo, mimo ze house brush TEZ ustawia flage PZ na kaflu
                // (inaczej kazdy dom wygladalby jak zwykla strefa PZ, nie dom).
                if ((f & 64) != 0)     { c.r *= 0.5;  c.g *= 0.5; }
                else if ((f & 1) != 0) { c.r *= 0.5;  c.b *= 0.5; }
                // PvP/NoLogout/NoPvp dokladaja sie NIEZALEZNIE (RME: osobne if-y).
                if ((f & 16) != 0) { c.r *= 0.66; c.b *= 0.66; }
                if ((f & 8) != 0)  { c.b *= 0.5; }
                if ((f & 4) != 0)  { c.g *= 0.5; }
                return c;
            }
            void main() {
                vec4 c = texture(uAtlas, vUV);
                if (c.a < 0.01) discard;
                // Zaznaczenie tintowane W MIEJSCU (jak RME) - item sciemniany x0.5 gdy
                // zaznaczony. Dzieki temu jest w tej samej, poprawnej kolejnosci co mapa
                // (bez osobnej warstwy nakladajacej wysokie sprite'y).
                float sf = (vSel > 0.5) ? 0.5 : 1.0;
                FragColor = vec4(c.rgb * uTint.rgb * sf * zoneTint(vZone), c.a * uTint.a);
            }
        )");
        m_prog->bindAttributeLocation("aCorner", 0);
        m_prog->bindAttributeLocation("aInst", 1);
        m_prog->bindAttributeLocation("aSel", 2);
        m_prog->bindAttributeLocation("aZone", 3);
        m_prog->link();

        m_vao.create();
        m_vao.bind();
        static const float quad[] = {
            0.f, 0.f,  1.f, 0.f,  1.f, 1.f,
            0.f, 0.f,  1.f, 1.f,  0.f, 1.f,
        };
        m_quadVbo.create();
        m_quadVbo.bind();
        m_quadVbo.allocate(quad, sizeof(quad));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        m_vao.release();
        m_quadVbo.release();

        // --- Flat shader: prostokat zaznaczania (Shift/Ctrl+drag), plaski kolor bez
        // tekstury. uRect=(x0,y0,x1,y1) w world-px; aCorner (0..1) interpoluje miedzy
        // rogami - ten sam quad(6w) daje wypelnienie, osobny 4-wierzcholkowy bufor
        // (w poprawnej kolejnosci obwodu) daje obrys przez GL_LINE_LOOP.
        m_flatProg = new QOpenGLShaderProgram;
        m_flatProg->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;
            uniform mat4 uMatrix;
            uniform vec4 uRect;   // x0,y0,x1,y1
            void main() {
                vec2 p = mix(uRect.xy, uRect.zw, aCorner);
                gl_Position = uMatrix * vec4(p, 0.0, 1.0);
            }
        )");
        m_flatProg->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            out vec4 FragColor;
            uniform vec4 uColor;
            void main() { FragColor = uColor; }
        )");
        m_flatProg->bindAttributeLocation("aCorner", 0);
        m_flatProg->link();

        // --- Kursor pedzla: INSTANCJONOWANE plaskie kwadraty (1 na kafel footprintu).
        // Oswietlenie: teksturowany prostokat w world-space (uRect px), multiply
        // nakladany na gotowa mape. Tekstura 1px=1kafel z LINEAR = miekkie swiatlo.
        m_lightProg = new QOpenGLShaderProgram;
        m_lightProg->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;   // [0,0]..[1,1] (ten sam quad co flat)
            uniform mat4 uMatrix;
            uniform vec4 uRect;   // x0, y0, x1, y1 (world px) - jak flatProg
            out vec2 vUv;
            void main() {
                vUv = aCorner;
                vec2 p = mix(uRect.xy, uRect.zw, aCorner);
                gl_Position = uMatrix * vec4(p, 0.0, 1.0);
            }
        )");
        m_lightProg->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            in vec2 vUv;
            out vec4 FragColor;
            uniform sampler2D uTex;
            void main() { FragColor = texture(uTex, vUv); }
        )");
        m_lightProg->bindAttributeLocation("aCorner", 0);
        m_lightProg->link();

        // Osobny program, bo flatProg rysuje pojedynczy prostokat z uniformu, a kursor
        // musi oddac KSZTALT pedzla (kolo!) - czyli wiele kafli w jednym draw callu.
        m_cursorProg = new QOpenGLShaderProgram;
        m_cursorProg->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;   // [0,0]..[1,1]
            // xy = pozycja (world px), zw = ROZMIAR prostokata (px). Rozmiar per
            // instancja (nie uniform 32): podglad Shift+drag to JEDNA instancja na
            // caly prostokat - per-kafel przy 400x300 dawalo 120k instancji budowanych
            // na CPU co ruch myszy (lag przy szybkim przeciaganiu).
            layout(location=1) in vec4 aInst;
            uniform mat4 uMatrix;
            out vec2 vCorner;       // [0,0]..[1,1] w obrebie kafla (do borderu)
            out vec2 vSizePx;       // rozmiar kafla w px (grubosc borderu w pikselach)
            void main() {
                vCorner = aCorner;
                vSizePx = aInst.zw;
                vec2 p = aInst.xy + aCorner * aInst.zw;
                gl_Position = uMatrix * vec4(p, 0.0, 1.0);
            }
        )");
        m_cursorProg->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            in vec2 vCorner;
            in vec2 vSizePx;
            out vec4 FragColor;
            uniform vec4 uColor;         // wypelnienie
            uniform vec4 uBorderColor;   // obrys (uzywany gdy uBorder > 0.5)
            uniform float uBorder;       // 0 = plaski (spawn marks), 1 = fill+obrys
            void main() {
                if (uBorder > 0.5) {
                    // Odleglosc od najblizszej krawedzi kafla w PIKSELACH.
                    vec2 dpx = min(vCorner, vec2(1.0) - vCorner) * vSizePx;
                    float edge = min(dpx.x, dpx.y);
                    FragColor = (edge < 2.0) ? uBorderColor : uColor;   // 2px obrys
                } else {
                    FragColor = uColor;
                }
            }
        )");
        m_cursorProg->bindAttributeLocation("aCorner", 0);
        m_cursorProg->bindAttributeLocation("aInst", 1);
        m_cursorProg->link();

        m_flatVao.create();
        m_flatVao.bind();
        static const float borderCorners[] = {
            0.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f,   // obwod w kolejnosci (LINE_LOOP)
        };
        m_borderVbo.create();
        m_borderVbo.bind();
        m_borderVbo.allocate(borderCorners, sizeof(borderCorners));
        m_flatVao.release();
        m_borderVbo.release();
    }

    void uploadAtlas(const QImage &img)
    {
        if (img.isNull()) return;
        const QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
        m_atlasW = rgba.width();
        m_atlasH = rgba.height();
        if (!m_tex) glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasW, m_atlasH, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
        glBindTexture(GL_TEXTURE_2D, 0);
        // Atlas PRZYROSTOWY (stabilne sloty) - rosnie tylko o nowe sprite'y, wiec
        // istniejace bufory pozostaja wazne. NIE unieważniamy ich (zero "smieci").
    }

    QOpenGLShaderProgram *m_prog = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_quadVbo{QOpenGLBuffer::VertexBuffer};

    // VBO per chunk (jak sektory RME): edycja przebudowuje tylko dotkniety chunk.
    struct ChunkBuf {
        QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
        int count = 0;
        quint32 version = 0;     // wersja tresci chunka, dla ktorej zbudowano VBO
        bool groundOnly = false; // czy zbudowano w trybie LOD (tylko podloga)
        bool valid = false;
    };
    std::unordered_map<quint64, std::unique_ptr<ChunkBuf>> m_chunkBufs[16];  // per pietro
    std::vector<quint64> m_drawList[16];   // klucze chunkow do rysowania (per pietro)
    int m_lastMinCX = 1, m_lastMinCY = 1, m_lastMaxCX = 0, m_lastMaxCY = 0;  // do eksmisji

    QOpenGLBuffer m_fxVbo;           // dynamiczne bufory nakladek (efekt/zaznaczenie/duch)
    std::vector<float> m_fxInst;
    int m_fxCount = 0;
    QOpenGLBuffer m_selVbo;
    std::vector<float> m_selInst;
    int m_selCount = 0;
    QOpenGLBuffer m_ghostVbo;
    std::vector<float> m_ghostInst;
    int m_ghostCount = 0;

    // Kursor pedzla per-kafel (2 floaty/instancje) - oddaje ksztalt (kolo/kwadrat).
    QOpenGLShaderProgram *m_cursorProg = nullptr;
    QOpenGLBuffer m_cursorVbo;
    std::vector<float> m_cursorInst;
    int m_cursorCount = 0;
    QOpenGLBuffer m_spawnVbo;          // markery spawnow (ten sam program, inny kolor)
    std::vector<float> m_spawnInst;
    int m_spawnCount = 0;
    QOpenGLBuffer m_spawnSelVbo;       // markery ZAZNACZONYCH spawnow (przyciemnione)
    std::vector<float> m_spawnSelInst;
    int m_spawnSelCount = 0;
    QOpenGLBuffer m_gridVbo;           // siatka kafli (Show grid) - linie jako plaskie rect-y
    std::vector<float> m_gridInst;
    int m_gridCount = 0;
    // Kafle stref/domow BEZ itemow - plaskie kolorowe kwadraty (tint quadow nie ma
    // sie tam na czym zawiesic; patrz glCollectZoneMarkInstances).
    QOpenGLBuffer m_zoneHouseVbo;
    std::vector<float> m_zoneHouseInst;
    int m_zoneHouseCount = 0;
    QOpenGLBuffer m_zoneFlagVbo;
    std::vector<float> m_zoneFlagInst;
    int m_zoneFlagCount = 0;

    // Oswietlenie (jak TIME): tekstura 1px=1kafel nakladana multiply na mape.
    QOpenGLShaderProgram *m_lightProg = nullptr;
    unsigned int m_lightTexId = 0;
    std::vector<uint32_t> m_lightBuf;
    int m_lightTX = 0, m_lightTY = 0, m_lightTW = 0, m_lightTH = 0;
    quint32 m_lightVer = 0;
    bool m_lightUpload = false;

    // Prostokat zaznaczania (Shift/Ctrl+drag) - plaski kolor, osobny mini-shader.
    QOpenGLShaderProgram *m_flatProg = nullptr;
    QOpenGLVertexArrayObject m_flatVao;
    QOpenGLBuffer m_borderVbo{QOpenGLBuffer::VertexBuffer};   // 4 rogi (LINE_LOOP)
    bool m_rubberActive = false;
    double m_rubberRect[4] = {0, 0, 0, 0};   // x0,y0,x1,y1 (world px)
    bool m_brushRectActive = false;          // kursor-box aktywnego pedzla
    double m_brushRect[4] = {0, 0, 0, 0};    // x0,y0,x1,y1 (world px)

    GLuint m_tex = 0;
    int m_atlasGen = -1;
    float m_atlasW = 1, m_atlasH = 1;
    QMatrix4x4 m_matrix;
    QSize m_fbo;
    int m_curFloor = 7, m_botFloor = 7;
    bool m_useLinear = false;   // filtr atlasu wg zoomu (ostry vs gladki)
    bool m_showShade = true;   // "Show shade" (Q) - czy przyciemniac nizsze pietra

public:
    MapGLRenderer(const MapGLRenderer &) = delete;
};

} // namespace

MapGLView::MapGLView(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    m_fpsTimer.setInterval(1000);
    connect(&m_fpsTimer, &QTimer::timeout, this, [this] {
        m_fps = m_frameCount.exchange(0, std::memory_order_relaxed);
        emit fpsChanged();
    });
    m_fpsTimer.start();

    m_renderTimer.setTimerType(Qt::PreciseTimer);
    m_animClock.start();
    connect(&m_renderTimer, &QTimer::timeout, this, [this] { driverTick(); });
}

QQuickFramebufferObject::Renderer *MapGLView::createRenderer() const
{
    return new MapGLRenderer;
}

void MapGLView::setSource(MapView *s)
{
    if (m_source == s) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = s;
    if (m_source) {
        // Klucz: gdy watek roboczy MapView policzy chunk (albo po edycji/wczytaniu),
        // MapView emituje contentUpdated -> MY (renderer z realnym wezlem) robimy
        // update(). MapView::update() jest no-opem (ItemHasContents=false), wiec bez
        // tego sygnalu chunki czekaly na przypadkowa interakcje ("itemy pojawiaja
        // sie dopiero jak klikam"). update() jest watkowo-bezpieczne (kolejkowane).
        // Z LIMITEM FPS klatki pompuje WYLACZNIE m_renderTimer - bezposredni update()
        // per sygnal przebijal limit przy malowaniu (kazdy ruch myszy/edycja = extra
        // klatka: realnie ~700 fps przy ustawionych 240). Timer i tak podniesie
        // zmiane na najblizszym ticku (max ~4 ms opoznienia przy 240).
        connect(m_source, &MapView::contentUpdated, this, [this] {
            m_framePending = true;          // sterowniki klatek wyrenderuja przy nastepnej okazji
            if (m_maxFps <= 0) update();    // bez limitu: od razu (afterAnimating podtrzyma)
        });
    }
    emit sourceChanged();
    update();
}

// Wspolne cialo obu sterownikow klatek (timer przy limicie FPS / afterAnimating).
// Render tylko gdy cos sie zmienilo (dirty) albo gra JAKAS animacja (efekt
// magiczny / wlaczone animacje itemow) - bezwarunkowy update() co tick mielil
// CPU/GPU na identycznych klatkach idle.
//
// ANIMACJE ITEMOW: klatka tyka TUTAJ (co >=500 ms, RME ITEM_FRAME_DURATION), nie
// wlasnym QTimerem MapView. Powod: rytm klatek jedzie na dokladnie tym samym
// mechanizmie co render - wczesniej osobny timer + sztafeta dirty->pending->
// wymuszone klatki bywaly zawodne w bezczynnosci i itemy tykaly dopiero przy
// ruchu myszy / malowaniu (kazdy sync "przy okazji" dociagal nowe quady).
// animTick() podbija licznik klatek i uniewaznia cache quadow; update() zaraz
// potem gwarantuje sync, ktory zleci przeliczenie widocznych chunkow workerowi.
void MapGLView::driverTick()
{
    if (!isVisible()) return;
    const bool itemAnims = m_source && m_source->showAnimations();
    if (itemAnims && m_animClock.elapsed() >= 500) {
        m_animClock.restart();
        m_source->animTick();   // ++klatka + inwalidacja quadow (emituje contentUpdated)
    }
    const bool animating = itemAnims || (m_source && m_source->hasActiveEffects());
    if (m_framePending || animating) { m_framePending = false; update(); }
}

void MapGLView::setMaxFps(int v)
{
    v = qMax(0, v);
    if (m_maxFps == v) return;
    m_maxFps = v;
    updateRenderDriver();
    emit maxFpsChanged();
}

void MapGLView::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickFramebufferObject::itemChange(change, value);
    if (change == ItemSceneChange)
        updateRenderDriver();
}

void MapGLView::updateRenderDriver()
{
    // Bez limitu: render co klatke okna (afterAnimating). Z limitem: timer pompuje
    // update() z zadana czestotliwoscia (mniej obciazenia GPU).
    disconnect(m_frameConn);
    m_renderTimer.stop();
    if (!window()) return;

    if (m_maxFps <= 0) {
        // Bez limitu tez dirty-gating: afterAnimating podtrzymuje petle renderu
        // tylko dopoki cos sie dzieje (pending/efekty/animacje itemow); potem okno
        // idzie spac, a nastepny contentUpdated budzi je bezposrednim update().
        m_frameConn = connect(window(), &QQuickWindow::afterAnimating,
                              this, [this] { driverTick(); });
    } else {
        m_renderTimer.setInterval(qMax(1, 1000 / m_maxFps));
        m_renderTimer.start();
    }
}
