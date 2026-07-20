
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

QPoint MapView::tileAtScreen(const QPointF &p) const
{
    const qreal ts = std::max(1, m_tileSize);
    return QPoint(static_cast<int>(std::floor(m_originX + p.x() / ts)),
                  static_cast<int>(std::floor(m_originY + p.y() / ts)));
}

const OtbmTile *MapView::currentFloorTileAt(int x, int y) const
{

    return m_otbm ? m_otbm->tileAt(x, y, m_floor) : nullptr;
}

void MapView::clearSelection()
{
    if (m_selected.isEmpty()) return;
    m_selected.clear();
    notifySelectionChanged();
    emit contentUpdated(); update();
}

void MapView::applyRubberBand()
{
    m_selected = m_rubberBase;
    const int x0 = std::min(m_anchorX, m_rubberX), x1 = std::max(m_anchorX, m_rubberX);
    const int y0 = std::min(m_anchorY, m_rubberY), y1 = std::max(m_anchorY, m_rubberY);

    int zBottom = m_floor;
    if (m_selectionFloors == 1)      zBottom = 15;
    else if (m_selectionFloors == 2) zBottom = renderBottomFloor();

    for (int z = m_floor; z <= zBottom; ++z) {

        const int comp = m_compensatedSelect ? (z - m_floor) : 0;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int tx = x - comp, ty = y - comp;
                if (m_otbm && m_otbm->tileAt(tx, ty, z))
                    m_selected.insert(selKey(tx, ty, z));
            }
    }
    notifySelectionChanged();
}

void MapView::updateHoverText()
{
    QString t;
    if (m_hoverX >= 0) {
        t = QStringLiteral("%1, %2, %3").arg(m_hoverX).arg(m_hoverY).arg(m_floor);
        const OtbmTile *tile = currentFloorTileAt(m_hoverX, m_hoverY);
        if (tile && !tile->items.empty()) {
            const OtbmMapItem &top = tile->items.back();
            const QString name = m_otb ? m_otb->nameForServerId(top.server_id) : QString();
            t += QStringLiteral("  -  %1 item(s)").arg(static_cast<int>(tile->items.size()));
            t += name.isEmpty() ? QStringLiteral(" [id %1]").arg(top.server_id)
                                : QStringLiteral(": %1").arg(name);
        }
    }
    if (t != m_hoverText) { m_hoverText = t; emit hoverChanged(); }
}

QVariantList MapView::selectionDetails() const
{
    QVariantList out;
    for (quint64 key : m_selected) {
        const int x = selX(key);
        const int y = selY(key);
        const int z = selZ(key);
        const OtbmTile *tile = m_otbm ? m_otbm->tileAt(x, y, z) : nullptr;

        QVariantMap m;
        m.insert(QStringLiteral("x"), x);
        m.insert(QStringLiteral("y"), y);
        m.insert(QStringLiteral("z"), z);

        QVariantList items;
        if (tile) {
            for (const OtbmMapItem &it : tile->items) {
                QVariantMap im;
                im.insert(QStringLiteral("serverId"), it.server_id);
                im.insert(QStringLiteral("clientId"),
                          m_otb ? m_otb->clientIdForServerId(it.server_id) : 0);
                im.insert(QStringLiteral("name"),
                          m_otb ? m_otb->nameForServerId(it.server_id) : QString());
                im.insert(QStringLiteral("isGround"), it.is_ground);
                items.append(im);
            }
        }
        m.insert(QStringLiteral("items"), items);
        out.append(m);
    }
    return out;
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus();
    m_lastMouse = event->position();

    if (m_pasting) {
        if (event->button() == Qt::LeftButton) {
            const QPoint t = tileAtScreen(event->position());
            commitPasteAt(t.x(), t.y());
            cancelPasting();
        } else if (event->button() == Qt::RightButton) {
            cancelPasting();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && !m_selectionMode
        && (m_brushServerId > 0 || m_activeZone != 0 || m_eraseMode
            || m_spawnBrush || !m_creatureBrush.isEmpty() || m_houseBrush > 0)) {

        m_eraseStroke = m_eraseMode || (event->modifiers() & Qt::ControlModifier) != 0;
        m_painting = true;
        m_paintLastX = m_paintLastY = -2000000;
        m_strokePlaced.clear();
        const QPoint t = tileAtScreen(event->position());

        m_dragDraw = (event->modifiers() & Qt::ShiftModifier) != 0 && brushCanDrag();
        if (m_dragDraw) {
            m_dragStartX = t.x();
            m_dragStartY = t.y();
            m_hoverX = t.x(); m_hoverY = t.y();
            emit contentUpdated(); update();
            event->accept();
            return;
        }

        if (m_otbm) m_otbm->beginUndoGroup();
        paintAt(t.x(), t.y());
    } else if (event->button() == Qt::LeftButton) {

        const QPoint t = tileAtScreen(event->position());
        const int tx = t.x(), ty = t.y();
        const quint64 k = selKey(tx, ty, m_floor);
        const OtbmTile *tile = currentFloorTileAt(tx, ty);

        const bool onItem = tile && (!tile->items.empty() || !tile->creature_name.isEmpty()
                                     || tile->spawn_radius > 0);
        const bool shift = event->modifiers() & Qt::ShiftModifier;
        const bool ctrl = event->modifiers() & Qt::ControlModifier;

        if (shift) {

            m_selWholeStack = true;
            m_anchorX = m_rubberX = tx;
            m_anchorY = m_rubberY = ty;
            m_selecting = true;
            if (!ctrl) m_selected.clear();
            m_rubberBase = m_selected;
            applyRubberBand();
            emit contentUpdated(); update();
        } else if (ctrl) {

            if (onItem) {
                m_selWholeStack = false;
                if (m_selected.contains(k)) m_selected.remove(k);
                else m_selected.insert(k);
                notifySelectionChanged();
                emit contentUpdated(); update();
            }
        } else if (!onItem) {

            if (!m_selected.isEmpty()) {
                m_selected.clear();
                notifySelectionChanged();
                emit contentUpdated(); update();
            }
        } else {

            if (!m_selected.contains(k)) {
                m_selected.clear();
                m_selected.insert(k);
                m_selWholeStack = false;
                notifySelectionChanged();
            }
            m_movingSel = true;
            m_moveMoved = false;
            m_moveSrcX = tx;
            m_moveSrcY = ty;

            m_moveServerId = tile->items.empty() ? 0 : tile->items.back().server_id;
            emit contentUpdated(); update();
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = true;

        if (m_hoverX != -1) { m_hoverX = m_hoverY = -1; updateHoverText(); }
    } else if (event->button() == Qt::RightButton && !m_selectionMode
               && (m_brushServerId > 0 || m_activeZone != 0 || m_eraseMode
                   || m_spawnBrush || !m_creatureBrush.isEmpty() || m_houseBrush > 0)) {

        if (m_activeZone != 0) setActiveZone(0);
        else if (m_spawnBrush) setSpawnBrush(false);
        else if (!m_creatureBrush.isEmpty()) setCreatureBrush(QString());
        else if (m_houseBrush > 0) { setHouseExitMode(false); setHouseBrush(0); }
        else setBrushServerId(0);
    } else if (event->button() == Qt::RightButton) {

        const QPoint t = tileAtScreen(event->position());
        m_contextX = t.x();
        m_contextY = t.y();
        const quint64 k = selKey(t.x(), t.y(), m_floor);
        if (currentFloorTileAt(t.x(), t.y())) {
            if (!m_selected.contains(k)) {
                m_selected.clear();
                m_selected.insert(k);
                notifySelectionChanged();
                emit contentUpdated(); update();
            }
        } else if (!m_selected.isEmpty()) {
            m_selected.clear();
            notifySelectionChanged();
            emit contentUpdated(); update();
        }
        emit contextMenuRequested(event->position().x(), event->position().y());
    }
    event->accept();
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();

    if (m_panning) {

        const QPointF delta = pos - m_lastMouse;
        const qreal ts = std::max(1, m_tileSize);
        m_originX -= delta.x() / ts;
        m_originY -= delta.y() / ts;
        m_lastMouse = pos;
        emit contentUpdated(); update();
        event->accept();
        return;
    }

    m_lastMouse = pos;

    if (m_movingSel) {

        const QPoint t = tileAtScreen(pos);
        if (t.x() != m_moveSrcX || t.y() != m_moveSrcY) m_moveMoved = true;
        if (t.x() != m_hoverX || t.y() != m_hoverY) {
            m_hoverX = t.x(); m_hoverY = t.y(); updateHoverText(); emit contentUpdated(); update();
        }
        event->accept();
        return;
    }

    if (m_painting) {
        const QPoint t = tileAtScreen(pos);

        if (!m_dragDraw) paintAt(t.x(), t.y());

        if (t.x() != m_hoverX || t.y() != m_hoverY) {
            m_hoverX = t.x(); m_hoverY = t.y(); updateHoverText();
            if (m_dragDraw) { emit contentUpdated(); update(); }
        }
        event->accept();
        return;
    }

    if (m_selecting) {
        const QPoint t = tileAtScreen(pos);
        if (t.x() != m_rubberX || t.y() != m_rubberY) {
            m_rubberX = t.x();
            m_rubberY = t.y();
            applyRubberBand();
            emit contentUpdated(); update();
        }
        event->accept();
        return;
    }

    const QPoint h = tileAtScreen(pos);
    if (h.x() != m_hoverX || h.y() != m_hoverY) {
        m_hoverX = h.x();
        m_hoverY = h.y();
        updateHoverText();
        if (!m_selectionMode || m_pasting) {
            emit contentUpdated();
            update();
        }
    }
    event->accept();
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_painting) {
        m_painting = false;
        if (m_dragDraw) {

            const QPoint t = tileAtScreen(event->position());
            drawDragRect(m_dragStartX, m_dragStartY, t.x(), t.y());
            m_dragDraw = false;
            emit contentUpdated(); update();
        } else if (m_otbm) {
            m_otbm->endUndoGroup();
        }
    } else if (event->button() == Qt::LeftButton && m_movingSel) {
        m_movingSel = false;
        const QPoint t = tileAtScreen(event->position());
        if (m_moveMoved && (t.x() != m_moveSrcX || t.y() != m_moveSrcY)) {

            moveSelection(t.x() - m_moveSrcX, t.y() - m_moveSrcY);
        }

        emit contentUpdated(); update();
    } else if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        emit contentUpdated(); update();
    } else if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
    }
    event->accept();
}

void MapView::mouseUngrabEvent()
{
    // Always close an active stroke so focus loss cannot leave an undo group open.
    if (m_painting && !m_dragDraw && m_otbm)
        m_otbm->endUndoGroup();

    m_painting = false;
    m_dragDraw = false;
    m_movingSel = false;
    m_moveMoved = false;
    m_selecting = false;
    m_panning = false;
    m_eraseStroke = false;
    m_paintLastX = m_paintLastY = -2000000;
    m_strokePlaced.clear();

    emit contentUpdated();
    update();
    QQuickItem::mouseUngrabEvent();
}

void MapView::hoverMoveEvent(QHoverEvent *event)
{
    if (m_panning || m_selecting) return;
    const QPoint h = tileAtScreen(event->position());
    if (h.x() != m_hoverX || h.y() != m_hoverY) {
        m_hoverX = h.x();
        m_hoverY = h.y();
        updateHoverText();
        if (!m_selectionMode || m_pasting) {
            emit contentUpdated();
            update();
        }
    }
    event->accept();
}

void MapView::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        event->ignore();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        setFloor(m_floor - steps);
    } else if (event->modifiers() & Qt::AltModifier) {

        static constexpr int kSizes[] = {0, 1, 2, 4, 6, 8, 11};
        int idx = 0;
        for (int i = 0; i < 7; ++i) if (kSizes[i] == m_brushSize) { idx = i; break; }
        idx = std::clamp(idx + (steps > 0 ? 1 : -1), 0, 6);
        setBrushSize(kSizes[idx]);
    } else {
        zoomAt(steps, event->position().x(), event->position().y());
    }
    event->accept();
}

void MapView::zoomAt(int steps, qreal px, qreal py)
{
    if (steps == 0) return;
    const qreal ts = std::max(1, m_tileSize);
    const qreal worldX = m_originX + px / ts;
    const qreal worldY = m_originY + py / ts;

    int newSize = static_cast<int>(std::lround(m_tileSize * std::pow(1.2, steps)));
    if (newSize == m_tileSize) newSize += (steps > 0 ? 1 : -1);
    newSize = std::clamp(newSize, 1, 256);
    if (newSize != m_tileSize) {
        m_tileSize = newSize;
        emit tileSizeChanged();
        m_originX = worldX - px / newSize;
        m_originY = worldY - py / newSize;
        emit contentUpdated(); update();
    }
}

void MapView::keyPressEvent(QKeyEvent *event)
{

    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        setFloor(m_floor - 1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        setFloor(m_floor + 1);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Q) {
        setShowShade(!m_showShade);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
        setShowLowerFloors(!m_showLowerFloors);
        event->accept();
        return;
    }

    if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_F) {
            setShowCreatures(!m_showCreatures);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_S) {
            setShowSpawns(!m_showSpawns);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_E) {
            setShowZones(!m_showZones);
            event->accept();
            return;
        }

        if (event->key() == Qt::Key_M) {
            setMinimapOn(!m_minimapOn);
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Space && !(event->modifiers() & Qt::ControlModifier)) {
        toggleSelectionMode();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_R && !m_activeDoodadBrush.isEmpty() && m_brushStore) {
        const int cnt = m_brushStore->doodadVariantCount(m_activeDoodadBrush);
        if (cnt > 1) {
            m_doodadVariant = (m_doodadVariant + 1) % cnt;
            emit contentUpdated(); update();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        if (!m_selected.isEmpty()) deleteSelectedTop();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_pasting) {
        cancelPasting();
        event->accept();
        return;
    }

    const int k = event->key();
    if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
        if (!event->isAutoRepeat() && !m_heldArrows.contains(k)) {
            m_heldArrows.insert(k);
            if (!m_arrowTimer) {
                m_arrowTimer = new QTimer(this);
                m_arrowTimer->setTimerType(Qt::PreciseTimer);
                m_arrowTimer->setInterval(8);
                connect(m_arrowTimer, &QTimer::timeout, this, [this] {
                    const double dt = m_arrowClock.nsecsElapsed() / 1e9;
                    m_arrowClock.restart();

                    const bool fast = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
                    const double speed = (fast ? 60.0 : 25.0) * dt;
                    if (m_heldArrows.contains(Qt::Key_Left))  m_originX -= speed;
                    if (m_heldArrows.contains(Qt::Key_Right)) m_originX += speed;
                    if (m_heldArrows.contains(Qt::Key_Up))    m_originY -= speed;
                    if (m_heldArrows.contains(Qt::Key_Down))  m_originY += speed;
                    emit contentUpdated(); update();
                });
            }
            if (!m_arrowTimer->isActive()) {
                m_arrowClock.restart();
                m_arrowTimer->start();
            }
        }
        event->accept();
        return;
    }
    QQuickItem::keyPressEvent(event);
}

void MapView::keyReleaseEvent(QKeyEvent *event)
{
    const int k = event->key();
    if (!event->isAutoRepeat() && m_heldArrows.remove(k)) {
        if (m_heldArrows.isEmpty() && m_arrowTimer) m_arrowTimer->stop();
        event->accept();
        return;
    }
    QQuickItem::keyReleaseEvent(event);
}

void MapView::focusOutEvent(QFocusEvent *event)
{

    m_heldArrows.clear();
    if (m_arrowTimer) m_arrowTimer->stop();
    QQuickItem::focusOutEvent(event);
}
