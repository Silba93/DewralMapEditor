#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include "backend.h"
#include "mapview.h"
#include "mapglview.h"
#include "minimapview.h"
#include "palettefilter.h"

int main(int argc, char *argv[])
{

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("Dewral"));
    QCoreApplication::setApplicationName(QStringLiteral("DewralMapEditor"));

    Backend backend(nullptr);

    QQmlApplicationEngine engine;

    engine.addImageProvider(QStringLiteral("tibiaui"),
                            new UiThemeImageProvider(backend.uiTheme()));

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
