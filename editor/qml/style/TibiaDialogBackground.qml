import QtQuick

// Tlo okna/dialogu w stylu classic Tibia UI. Zrodlo: popupwindow.png (236x207),
// wartosci bazowe z 10-windows.otui -> Window: image-border=6, image-border-top=27
// (grubszy gorny margines mieszczacy pasek tytulu). topBorder parametryzowany,
// zeby okna z wyzszym wlasnym titleBar (np. Main.qml) mogly dac wiecej miejsca
// bez zmiany wygladu innych okien (np. StartupWindow, ktory zostaje przy 27).
BorderImage {
    property int topBorder: 27
    // Domyslnie klasyczny popupwindow.png (naglowek 27px). Okna z wyzszym paskiem
    // tytulu (Main.qml) moga podac popupwindow_tall.png - wersje z doklejonym szumem
    // naglowka (ostry bevel u gory + separator u dolu zachowane), border-top 45.
    property url frameSource: (uiTheme.tex + "popupwindow.png")
    source: frameSource
    smooth: false
    border { left: 6; right: 6; top: topBorder; bottom: 6 }
    // Kafelkuj srodek zamiast rozciagac (patrz TibiaPanel.qml - ten sam problem
    // przy duzych oknach: ziarnista tekstura rozmazana przez domyslny Stretch).
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
}
