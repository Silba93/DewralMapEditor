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

    // Profile do wyboru: skonfigurowane (bazowe + custom, np. "Midhem") + typowe
    // stare ery, zeby dalo sie zaczac nawet bez konfiguracji (dialog folderu
    // otworzy sie sam). Klucze jak w Main.qml: "772" / "Midhem".
    property var profileKeys: []
    onAboutToShow: {
        var keys = []
        var seen = {}
        function push(k) { if (!seen[k]) { seen[k] = true; keys.push(k) } }
        push("760"); push("772")
        Object.keys(app.clientPaths).forEach(push)
        keys.sort(function(a, b) {
            var na = Number(a), nb = Number(b)
            var ca = isNaN(na), cb = isNaN(nb)
            if (ca !== cb) return ca ? 1 : -1           // customy na koncu
            return ca ? a.localeCompare(b) : na - nb
        })
        profileKeys = keys
        var idx = profileKeys.indexOf(app.loadedClientKey)
        verCombo.currentIndex = idx >= 0 ? idx : profileKeys.length - 1
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
                width: 160
                model: root.profileKeys.map(function(k) { return root.app.profileLabel(k) })
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
                    root.app.createNewMap(root.profileKeys[verCombo.currentIndex],
                                          wField.value, hField.value)
                }
            }
            TibiaButton { text: "Anuluj"; width: 90; onClicked: root.close() }
        }
    }
}
