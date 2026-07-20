#include "minimapview.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

MinimapView::MinimapView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);

    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

void MinimapView::setSource(MapView *s)
{
    if (m_source == s) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = s;
    if (m_source) {

        connect(m_source, &MapView::contentUpdated, this, [this] { update(); });
        connect(m_source, &MapView::floorChanged, this, [this] { update(); });
    }
    emit sourceChanged();
    update();
}

void MinimapView::setPxPerTile(int v)
{
    v = std::clamp(v, 1, 8);
    if (m_pxPerTile == v) return;
    m_pxPerTile = v;
    emit pxPerTileChanged();
    update();
}

void MinimapView::paint(QPainter *p)
{
    p->fillRect(QRectF(0, 0, width(), height()), QColor(12, 14, 18));
    if (!m_source) return;

    const QImage &img = m_source->minimapImage();
    m_paintedVer = m_source->minimapVersion();
    if (img.isNull()) return;

    const double ts = std::max(1, m_source->tileSize());
    const double cx = m_source->glOriginX() + m_source->width() / (2.0 * ts);
    const double cy = m_source->glOriginY() + m_source->height() / (2.0 * ts);

    const double tilesW = width() / double(m_pxPerTile);
    const double tilesH = height() / double(m_pxPerTile);

    const double sx = (cx - m_source->minimapOriginX()) - tilesW / 2.0;
    const double sy = (cy - m_source->minimapOriginY()) - tilesH / 2.0;

    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    p->drawImage(QRectF(0, 0, width(), height()),
                 img, QRectF(sx, sy, tilesW, tilesH));

    const double viewW = m_source->width() / ts * m_pxPerTile;
    const double viewH = m_source->height() / ts * m_pxPerTile;
    p->setPen(QPen(QColor(255, 255, 255, 190), 1));
    p->setBrush(Qt::NoBrush);
    p->drawRect(QRectF(width() / 2.0 - viewW / 2.0, height() / 2.0 - viewH / 2.0,
                       viewW, viewH));
}

void MinimapView::centerMapAt(const QPointF &pos)
{
    if (!m_source) return;
    const double ts = std::max(1, m_source->tileSize());
    const double cx = m_source->glOriginX() + m_source->width() / (2.0 * ts);
    const double cy = m_source->glOriginY() + m_source->height() / (2.0 * ts);

    const int tx = static_cast<int>(std::floor(cx + (pos.x() - width() / 2.0) / m_pxPerTile));
    const int ty = static_cast<int>(std::floor(cy + (pos.y() - height() / 2.0) / m_pxPerTile));
    m_source->centerOnTile(tx, ty, m_source->floor());
}

void MinimapView::mousePressEvent(QMouseEvent *e)
{
    centerMapAt(e->position());
    e->accept();
}

void MinimapView::mouseMoveEvent(QMouseEvent *e)
{

    centerMapAt(e->position());
    e->accept();
}

void MinimapView::wheelEvent(QWheelEvent *e)
{
    const int steps = e->angleDelta().y() / 120;
    if (steps != 0) setPxPerTile(m_pxPerTile + steps);
    e->accept();
}
