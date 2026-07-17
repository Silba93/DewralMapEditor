import QtQuick
import QtQuick.Controls
import "../style"

// File > New (Ctrl+N): nowa pusta mapa w nowej karcie, jak RME - wybor wersji
// klienta i rozmiaru. Domyslny rozmiar 2048x2048 (standard RME).
TibiaDialog {
    id: root
    // Okno glowne: clientPaths (skonfigurowane wersje), versionLabel, createNewMap.
    required property var app

    title: "New Map"

    // Wersje do wyboru: skonfigurowane foldery klienta + typowe stare ery, zeby
    // dalo sie zaczac nawet bez zadnej konfiguracji (dialog folderu otworzy sie sam).
    property var versions: []
    onAboutToShow: {
        var set = {}
        for (var k in app.clientPaths) set[parseInt(k)] = true
        set[760] = true
        set[772] = true
        versions = Object.keys(set).map(Number).sort(function(a, b) { return a - b })
        var idx = versions.indexOf(app.loadedClientVersion)
        verCombo.currentIndex = idx >= 0 ? idx : versions.length - 1
    }

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 6
            Text {
                text: "Wersja klienta"; color: "#999"; font.pixelSize: 11
                width: 90; anchors.verticalCenter: parent.verticalCenter
            }
            TibiaComboBox {
                id: verCombo
                width: 140
                model: root.versions.map(function(v) { return root.app.versionLabel(v) })
            }
        }

        Row {
            spacing: 6
            Text {
                text: "Rozmiar"; color: "#999"; font.pixelSize: 11
                width: 90; anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox { id: wField; width: 80; from: 256; to: 65535; value: 2048 }
            Text { text: "x"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            TibiaSpinBox { id: hField; width: 80; from: 256; to: 65535; value: 2048 }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            TibiaButton {
                text: "Utworz"
                width: 90
                enabled: verCombo.currentIndex >= 0
                onClicked: {
                    root.close()
                    root.app.createNewMap(root.versions[verCombo.currentIndex],
                                          wField.value, hField.value)
                }
            }
            TibiaButton { text: "Anuluj"; width: 90; onClicked: root.close() }
        }
    }
}
