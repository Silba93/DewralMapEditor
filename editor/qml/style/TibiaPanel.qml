import QtQuick

// Plaski panel w stylu classic Tibia UI - tlo palety/list/kolumn ustawien.
// Zrodlo: panel_flat.png, wartosci z 10-panels.otui -> FlatPanel: image-border=1.
BorderImage {
    source: (uiTheme.tex + "panel_flat.png")
    smooth: false
    border { left: 1; right: 1; top: 1; bottom: 1 }
    // Domyslny tryb (Stretch) rozciaga srodek na cala powierzchnie - dla malej (68x68),
    // ziarnistej tekstury kamiennej to wyglada jak rozmazana plama przy wiekszym panelu.
    // Repeat kafelkuje ja zamiast rozciagac - tekstura zostaje ostra niezaleznie od rozmiaru.
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
}
