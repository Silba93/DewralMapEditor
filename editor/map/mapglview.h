#ifndef MAPGLVIEW_H
#define MAPGLVIEW_H

#include <QQuickFramebufferObject>
#include <QTimer>
#include <atomic>
#include "mapview.h"

// -----------------------------------------------------------------------------
// MapGLView
//
// Renderer mapy na SUROWYM OpenGL z instancingiem (jak edytor TIME), osadzony
// w QML przez QQuickFramebufferObject. Renderuje do tekstury (FBO), ktora Qt
// Quick komponuje z reszta UI - paleta/menu zostaja w QML.
//
// Nie duplikuje danych: czyta je z istniejacego MapView (atlas + quady, ktore
// watek w tle juz liczy) przez wlasciwosc 'source'. Caly widoczny zakres pieter
// rysowany jest JEDNYM wywolaniem instancjonowanym (1 sprite = 1 instancja).
// -----------------------------------------------------------------------------
class MapGLView : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)
    // Limit FPS renderera OpenGL. 0 = bez limitu (renderuje co klatke okna).
    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
public:
    explicit MapGLView(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    MapView *source() const { return m_source; }
    void setSource(MapView *s);

    int fps() const { return m_fps; }
    void countFrame() { m_frameCount.fetch_add(1, std::memory_order_relaxed); } // z watku renderu

    int maxFps() const { return m_maxFps; }
    void setMaxFps(int v);

signals:
    void sourceChanged();
    void fpsChanged();
    void maxFpsChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void updateRenderDriver();   // wybiera: afterAnimating (bez limitu) lub timer (limit)
    MapView *m_source = nullptr;
    QMetaObject::Connection m_frameConn;
    std::atomic<int> m_frameCount{0};
    int m_fps = 0;
    int m_maxFps = 0;            // 0 = bez limitu
    QTimer m_fpsTimer;
    QTimer m_renderTimer;        // pompuje update() przy limicie FPS
};

#endif // MAPGLVIEW_H
