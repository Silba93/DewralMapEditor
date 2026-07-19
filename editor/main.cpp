#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <qqml.h>

#include "sprreader.h"
#include "datreader.h"
#include "otbreader.h"
#include "otfireader.h"
#include "itemsxmlreader.h"
#include "mapview.h"
#include "mapglview.h"
#include "minimapview.h"
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
    qmlRegisterType<MinimapView>("Tibia", 1, 0, "MinimapView");
    qmlRegisterType<PaletteFilter>("Tibia", 1, 0, "PaletteFilter");

    // Kolejnosc wczytywania: najpierw .dat, .spr, .otb (dane klienta), na koncu
    // .otbm (mapa) - OtbReader odpytuje DatReader, a MapView buduje atlas dopiero
    // gdy wczytana jest mapa.
    SprReader sprReader;
    DatReader datReader;
    OtbReader otbReader;
    ItemsXmlReader itemsXml;     // nazwy + typy itemow (items.xml serwera)
    otbReader.setDatReader(&datReader);
    // items.xml uzupelnia OTB o nazwy i typy (teleport/depot/drzwi) - bez tego
    // paleta pokazuje same id, a edytor nie rozpoznaje teleportu. Jak RME, ktore
    // laczy items.otb + items.xml w jedna baze itemow.
    otbReader.setItemsXml(&itemsXml);
    DocumentManager docMgr;      // karty map: kazda otwarta mapa = wlasny OtbmReader
    FileTools fileTools;
    TilesetStore tilesetStore;   // tilesety RME (tilesets.xml z folderu klienta)
    BrushStore brushStore;       // silnik ground brushy + auto-bordery (brushes.json)
    CreatureStore creatureStore; // paleta potworow/NPC (creatures.xml, format RME)
    OtfiReader otfiReader;       // nadpisanie autodetekcji .dat/.spr z pliku .otfi
    UiTheme uiTheme;             // motyw: kolor nakladany multiply na tekstury UI

    // Engine deklarowany PO wszystkich obiektach domenowych - destrukcja idzie w
    // odwrotnej kolejnosci, wiec engine umiera PIERWSZY. Wczesniej bylo odwrotnie:
    // przy zamykaniu aplikacji obiekty (uiTheme, docMgr, fileTools...) umieraly
    // przed silnikiem QML, ktory kazda smierc zamienial na null w context property
    // i re-ewaluowal WSZYSTKIE bindingi - stad lawina "TypeError: Cannot read
    // property 'tex'/'loaded'/... of null" na wyjsciu z programu.
    QQmlApplicationEngine engine;

    // Provider MUSI byc dodany przed engine.load - inaczej pierwsze URL-e tekstur
    // (image://tibiaui/...) nie rozwiazuja sie i UI wstaje bez teł.
    engine.addImageProvider(QStringLiteral("tibiaui"), new UiThemeImageProvider(&uiTheme));

    // GOLE literaly (nie QStringLiteral): analizator QML w Qt Creatorze rozpoznaje
    // context property tylko przy literale lancuchowym - z makrem nie podpowiadal
    // tych obiektow w QML i sypal ostrzezeniami "musi byc literalem lancuchowym".
    // To wywolania jednorazowe przy starcie, wiec QStringLiteral i tak nic nie dawal.
    engine.rootContext()->setContextProperty("sprReader", &sprReader);
    engine.rootContext()->setContextProperty("datReader", &datReader);
    engine.rootContext()->setContextProperty("otbReader", &otbReader);
    engine.rootContext()->setContextProperty("docMgr", &docMgr);
    // "otbmReader" wskazuje AKTYWNY dokument i jest przepinane przy zmianie karty.
    // Context properties sa dynamiczne: ponowne setContextProperty re-ewaluuje
    // wszystkie bindingi w QML, wiec menu/dialogi dzialaja bez zmian - widza
    // zawsze biezaca mape. Connect jest Direct (ten sam watek), wiec po powrocie
    // z newDocument()/setCurrentIndex() QML widzi juz nowego readera.
    engine.rootContext()->setContextProperty("otbmReader", docMgr.current());
    QObject::connect(&docMgr, &DocumentManager::currentChanged, &engine, [&engine, &docMgr] {
        engine.rootContext()->setContextProperty("otbmReader", docMgr.current());
    });
    engine.rootContext()->setContextProperty("fileTools", &fileTools);
    engine.rootContext()->setContextProperty("tilesetStore", &tilesetStore);
    engine.rootContext()->setContextProperty("brushStore", &brushStore);
    engine.rootContext()->setContextProperty("otfiReader", &otfiReader);
    engine.rootContext()->setContextProperty("uiTheme", &uiTheme);
    engine.rootContext()->setContextProperty("creatureStore", &creatureStore);
    engine.rootContext()->setContextProperty("itemsXml", &itemsXml);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
