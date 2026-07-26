#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QSurfaceFormat>

#include "backend.h"
#include "mapview.h"
#include "mapglview.h"
#include "minimapview.h"
#include "palettefilter.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));

    QSettings startupSettings(QSettings::NativeFormat, QSettings::UserScope,
                              QStringLiteral("Dewral"),
                              QStringLiteral("DewralMapEditor"));
    const bool vsyncEnabled =
        startupSettings.value(QStringLiteral("vsyncEnabled"), true).toBool();

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(vsyncEnabled ? 1 : 0);
    QSurfaceFormat::setDefaultFormat(format);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/ui/github/app-icon.png")));

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
