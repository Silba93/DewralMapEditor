// MapView - czesc ATLASU: przyrostowy atlas sprite'ow (CPU, QImage) ze
// stabilnymi slotami + wariant highlight (szary sylwet do podswietlen).
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

void MapView::addSpritesToAtlas(const QSet<uint32_t> &sids)
{
    if (!m_spr) return;
    std::vector<uint32_t> toAdd;
    for (uint32_t sid : sids)
        if (sid != 0 && !m_spriteToSlot.contains(sid)) toAdd.push_back(sid);
    if (toAdd.empty()) return;

    constexpr int cols = 64;                 // staly -> pozycje slotow (px) stabilne
    constexpr int headroom = 1024;           // zapas slotow na dokladanie bez resize
    const int oldCount = static_cast<int>(m_atlasSlots.size());
    const int newCount = oldCount + static_cast<int>(toAdd.size());
    const int curRows = m_atlasImage.isNull() ? 0 : m_atlasImage.height() / kSprite;
    const int capacity = curRows * cols;

    // Resize TYLKO gdy braknie miejsca (rzadko). Zmiana ROZMIARU atlasu zmienia UV
    // istniejacych sprite'ow (UV=px/rozmiar), wiec wtedy wymuszamy przebudowe buforow
    // (bump dataVersion). W zwyklym przypadku rozmiar staly => UV stabilne => zero smieci.
    bool grew = false;
    if (m_atlasImage.isNull() || newCount > capacity) {
        const int rows = (newCount + headroom + cols - 1) / cols;
        QImage img(cols * kSprite, std::max(1, rows) * kSprite, QImage::Format_RGBA8888);
        img.fill(Qt::transparent);
        if (!m_atlasImage.isNull()) { QPainter cp(&img); cp.drawImage(0, 0, m_atlasImage); }
        m_atlasImage = img;
        grew = true;
    }

    QPainter p(&m_atlasImage);
    int slot = oldCount;
    for (uint32_t sid : toAdd) {
        const int sx = (slot % cols) * kSprite;
        const int sy = (slot / cols) * kSprite;
        auto sprite = m_spr->loadSprite(sid);
        if (sprite && !sprite->image.isNull())
            p.drawImage(sx, sy, sprite->image);
        m_spriteToSlot.insert(sid, slot);
        m_atlasSlots.push_back(QRect(sx, sy, kSprite, kSprite));
        ++slot;
    }
    p.end();
    ++m_atlasGeneration;        // tekstura zmieniona -> MapGLView przesle ja na nowo
    if (grew) ++m_dataVersion;  // zmiana rozmiaru -> UV inne -> przebuduj bufory (rzadko)
}

void MapView::ensureItemSprites(int serverId)
{
    // Cache: to leci raz na KAZDY postawiony item, a wynik dla danego server id sie
    // nie zmienia (atlas przyrostowy). Bez tego Shift+drag budowal QSet ze sprite_ids
    // setki tysiecy razy dla tego samego itemu.
    if (m_ensuredServerIds.contains(serverId)) return;
    m_ensuredServerIds.insert(serverId);

    const int cid = m_otb ? m_otb->clientIdForServerId(serverId) : 0;
    const ClientItem *ci = (m_dat && cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    if (!ci) return;
    QSet<uint32_t> sids;
    for (uint32_t sid : ci->sprite_ids) if (sid != 0) sids.insert(sid);
    addSpritesToAtlas(sids);
}

void MapView::buildAtlasImage()
{
    // Pierwsze wywolanie (po wczytaniu) buduje atlas ze WSZYSTKICH sprite'ow mapy +
    // efekt #3. Atlas jest przyrostowy, wiec kolejne edycje tylko dokladaja nowe.
    if (!m_otbm || !m_otbm->isLoaded() || !m_otb || !m_dat || !m_spr) return;

    // Nowa mapa/wersja klienta = te same server id moga wskazywac inne sprite'y.
    m_ensuredServerIds.clear();
    m_ensuredOutfits.clear();

    QSet<uint32_t> used;
    for (const OtbmTile &tile : m_otbm->tiles()) {
        for (const OtbmMapItem &item : tile.items) {
            const int cid = m_otb->clientIdForServerId(item.server_id);
            if (cid <= 0) continue;
            const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(cid));
            if (!ci) continue;
            for (uint32_t sid : ci->sprite_ids) if (sid != 0) used.insert(sid);
        }
        // Outfity potworow wczytanych ze spawns.xml - bez tego pierwszy render
        // mapy ze spawnami pokazalby puste kafle zamiast potworow.
        if (!tile.creature_name.isEmpty() && m_creatureStore) {
            if (const auto *ct = m_creatureStore->byName(tile.creature_name))
                if (const ClientItem *of = m_dat->outfitByLookType(static_cast<uint16_t>(ct->lookType)))
                    for (uint32_t sid : of->sprite_ids) if (sid != 0) used.insert(sid);
        }
    }
    if (const ClientItem *fx = m_dat->effectById(kPlaceEffectId))
        for (uint32_t sid : fx->sprite_ids) if (sid != 0) used.insert(sid);

    addSpritesToAtlas(used);
}

int MapView::atlasSlotForSprite(uint32_t spriteId) const
{
    return m_spriteToSlot.value(spriteId, -1);
}

