#ifndef MINIMAPVIEW_H
#define MINIMAPVIEW_H

#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>
#include "mapview.h"

class MinimapView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MinimapView)
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)

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
    void centerMapAt(const QPointF &pos);

    MapView *m_source = nullptr;
    int m_pxPerTile = 1;
    quint32 m_paintedVer = 0;
};

#endif
