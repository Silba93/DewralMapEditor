#ifndef MINIMAPVIEW_H
#define MINIMAPVIEW_H

#include <QQuickPaintedItem>
#include "mapview.h"

// -----------------------------------------------------------------------------
// MinimapView
//
// Okno minimapy (jak RME "Minimap" / TIME MinimapWindow): wycinek obrazu
// 1 px = 1 kafel biezacego pietra (cache w MapView::minimapImage), wysrodkowany
// na srodku widoku mapy, z ramka pokazujaca widoczny obszar.
//
// QQuickPaintedItem zamiast Image+dataURL: obraz minimapy zmienia sie czesto
// (kazda edycja = piksel), a kodowanie PNG/base64 per zmiana byloby absurdalnie
// drogie. Tutaj rysujemy QImage bezposrednio (drawImage z podprostokatem).
//
// Interakcja: LPM/przeciaganie = centruj widok mapy na wskazanym kaflu;
// kolko = zoom minimapy (px na kafel, 1..8).
// -----------------------------------------------------------------------------
class MinimapView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)
    // Piksele na kafel (zoom minimapy). 1 = klasyczny widok 1:1.
    Q_PROPERTY(int pxPerTile READ pxPerTile WRITE setPxPerTile NOTIFY pxPerTileChanged)

public:
    explicit MinimapView(QQuickItem *parent = nullptr);

    MapView *source() const { return m_source; }
    void setSource(MapView *s);

    int pxPerTile() const { return m_pxPerTile; }
    void setPxPerTile(int v);

    void paint(QPainter *p) override;

signals:
    void sourceChanged();
    void pxPerTileChanged();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void centerMapAt(const QPointF &pos);   // klik minimapy -> centrowanie mapy

    MapView *m_source = nullptr;
    int m_pxPerTile = 1;
    quint32 m_paintedVer = 0;   // wersja obrazu przy ostatnim paint (unik zbednych repaintow)
};

#endif // MINIMAPVIEW_H
