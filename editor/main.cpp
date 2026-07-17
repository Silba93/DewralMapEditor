#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <qqml.h>

#include "sprreader.h"
#include "datreader.h"
#include "otbreader.h"
#include "otbmreader.h"
#include "otfireader.h"
#include "mapview.h"
#include "mapglview.h"
#include "filetools.h"
#include "palettefilter.h"
#include "tilesetstore.h"
#include "brushstore.h"
#include "uitheme.h"
#include "documentmanager.h"
#include "creaturestore.h"

int main(int argc, char *argv[])
{
    // Wymus backend OpenGL Scene Graph - wymagane przez QQuickFramebufferObject
    // (renderer mapy na surowym OpenGL z instancingiem). Musi byc przed oknem.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);

    // Nazwy aplikacji - wymagane przez QML Settings (QtCore) do zapisu preferencji
    // (folder klienta, ostatnio otwarte mapy) w rejestrze/INI.
    QCoreApplication::setOrganizationName(QStringLiteral("Dewral"));
    QCoreApplication::setApplicationName(QStringLiteral("DewralMapEditor"));

    qmlRegisterType<MapView>("Tibia", 1, 0, "MapView");
    qmlRegisterType<MapGLView>("Tibia", 1, 0, "MapGLView");
    qmlRegisterType<PaletteFilter>("Tibia", 1, 0, "PaletteFilter");

    QQmlApplicationEngine engine;

    // Kolejnosc wczytywania: najpierw .dat, .spr, .otb (dane klienta), na koncu
    // .otbm (mapa) - OtbReader odpytuje DatReader, a MapView buduje atlas dopiero
    // gdy wczytana jest mapa.
    SprReader sprReader;
    DatReader datReader;
    OtbReader otbReader;
    otbReader.setDatReader(&datReader);
    DocumentManager docMgr;      // karty map: kazda otwarta mapa = wlasny OtbmReader
    FileTools fileTools;
    TilesetStore tilesetStore;   // tilesety RME (tilesets.xml z folderu klienta)
    BrushStore brushStore;       // silnik ground brushy + auto-bordery (brushes.json)
    CreatureStore creatureStore; // paleta potworow/NPC (creatures.xml, format RME)
    OtfiReader otfiReader;       // nadpisanie autodetekcji .dat/.spr z pliku .otfi
    UiTheme uiTheme;             // motyw: kolor nakladany multiply na tekstury UI

    // Provider MUSI byc dodany przed engine.load - inaczej pierwsze URL-e tekstur
    // (image://tibiaui/...) nie rozwiazuja sie i UI wstaje bez teł.
    engine.addImageProvider(QStringLiteral("tibiaui"), new UiThemeImageProvider(&uiTheme));

    engine.rootContext()->setContextProperty(QStringLiteral("sprReader"), &sprReader);
    engine.rootContext()->setContextProperty(QStringLiteral("datReader"), &datReader);
    engine.rootContext()->setContextProperty(QStringLiteral("otbReader"), &otbReader);
    engine.rootContext()->setContextProperty(QStringLiteral("docMgr"), &docMgr);
    // "otbmReader" wskazuje AKTYWNY dokument i jest przepinane przy zmianie karty.
    // Context properties sa dynamiczne: ponowne setContextProperty re-ewaluuje
    // wszystkie bindingi w QML, wiec menu/dialogi dzialaja bez zmian - widza
    // zawsze biezaca mape. Connect jest Direct (ten sam watek), wiec po powrocie
    // z newDocument()/setCurrentIndex() QML widzi juz nowego readera.
    engine.rootContext()->setContextProperty(QStringLiteral("otbmReader"), docMgr.current());
    QObject::connect(&docMgr, &DocumentManager::currentChanged, &engine, [&engine, &docMgr] {
        engine.rootContext()->setContextProperty(QStringLiteral("otbmReader"), docMgr.current());
    });
    engine.rootContext()->setContextProperty(QStringLiteral("fileTools"), &fileTools);
    engine.rootContext()->setContextProperty(QStringLiteral("tilesetStore"), &tilesetStore);
    engine.rootContext()->setContextProperty(QStringLiteral("brushStore"), &brushStore);
    engine.rootContext()->setContextProperty(QStringLiteral("otfiReader"), &otfiReader);
    engine.rootContext()->setContextProperty(QStringLiteral("uiTheme"), &uiTheme);
    engine.rootContext()->setContextProperty(QStringLiteral("creatureStore"), &creatureStore);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
