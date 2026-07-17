#ifndef MAPVIEW_P_H
#define MAPVIEW_P_H

// Prywatne helpery wspolne dla plikow implementacyjnych MapView (mapview*.cpp).
// NIE dolaczac poza editor/map/.

#include "datreader.h"

#include <algorithm>
#include <cstdint>

// Dzielenie calkowite z zaokragleniem W DOL (takze dla ujemnych) - do mapowania
// wspolrzednych kafelkow na wspolrzedne chunkow.
inline int floorDiv(int a, int b)
{
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

// Wylicza sprite ID dla komorki (ww,hh,layer) itemu na pozycji (posX,posY,posZ).
// Pattern wybierany z pozycji kafelka (jak w Tibii: px=x%pattern_x itd.) - dzieki
// temu gory/sciany/bordery/ground pokazuja wlasciwe warianty sprite'a. Frame 0.
//
// WYJATEK: itemy stackowalne (gold coin, itp.) biora pattern z COUNT, nie z pozycji.
// W .dat maja siatke 4x2 = 8 wariantow sterty (1,2,3,4,5-9,10-24,25-49,50+). Gdyby
// bralo z pozycji, wielkosc sterty zalezalaby od tego GDZIE item lezy i zmienialaby
// sie po kazdym przesunieciu. Mapowanie 1:1 z otclient Item::calculatePatterns
// (rownowaznik RME MapDrawer::BlitItem subtype 0..7 na siatce 4x2).
inline uint32_t cellSpriteId(const ClientItem *ci, int ww, int hh, int layer, int w, int h,
                             int posX, int posY, int posZ, int count = 1)
{
    const int patX = std::max(1, static_cast<int>(ci->pattern_x));
    const int patY = std::max(1, static_cast<int>(ci->pattern_y));
    const int patZ = std::max(1, static_cast<int>(ci->pattern_z));
    const int layers = std::max(1, static_cast<int>(ci->layers));
    int px, py;
    // Warunek 4x2 jak w otclient - chroni stackowalne bez pelnej siatki wariantow.
    if (ci->is_stackable && patX == 4 && patY == 2) {
        if (count <= 1)      { px = 0;         py = 0; }
        else if (count < 5)  { px = count - 1; py = 0; }
        else if (count < 10) { px = 0;         py = 1; }
        else if (count < 25) { px = 1;         py = 1; }
        else if (count < 50) { px = 2;         py = 1; }
        else                 { px = 3;         py = 1; }
    } else {
        px = ((posX % patX) + patX) % patX;
        py = ((posY % patY) + patY) % patY;
    }
    const int pz = ((posZ % patZ) + patZ) % patZ;
    const int idx = ((((pz * patY + py) * patX + px) * layers + layer) * h + hh) * w + ww;
    if (idx < 0 || idx >= static_cast<int>(ci->sprite_ids.size())) {
        return 0;
    }
    return ci->sprite_ids[static_cast<size_t>(idx)];
}

#endif // MAPVIEW_P_H
