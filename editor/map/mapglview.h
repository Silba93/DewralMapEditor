#ifndef MAPGLVIEW_H
#define MAPGLVIEW_H

#include <QQuickFramebufferObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include "mapview.h"

class MapGLView : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MapGLView)
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)

    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
public:
    explicit MapGLView(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    MapView *source() const { return m_source; }
    void setSource(MapView *s);

    int fps() const { return m_fps; }
    void countFrame() { m_frameCount.fetch_add(1, std::memory_order_relaxed); }

    int maxFps() const { return m_maxFps; }
    void setMaxFps(int v);
    void markFramePending() { m_framePending.store(true, std::memory_order_relaxed); }

signals:
    void sourceChanged();
    void fpsChanged();
    void maxFpsChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void updateRenderDriver();
    MapView *m_source = nullptr;
    QMetaObject::Connection m_frameConn;
    std::atomic<int> m_frameCount{0};
    int m_fps = 0;
    int m_maxFps = 0;

    std::atomic_bool m_framePending{true};

    QElapsedTimer m_animClock;

    void driverTick();
    QTimer m_fpsTimer;
    QTimer m_renderTimer;
};

#endif
