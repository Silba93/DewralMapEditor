import QtQuick
import QtQuick.Controls
import "../style"

// Dialog operacji na itemach (menu "Select" = zakres zaznaczenia, menu "Edit" = cala
// mapa; jak RME). Jeden dialog, bo tryby roznia sie tylko drugim polem i akcja.
//
// mode:  "find" | "remove" | "replace"
// scope: "selection" (menu Select) | "map" (menu Edit)
TibiaDialog {
    id: root
    property string mode: "find"
    property string scope: "selection"
    property var mapCtrl: null

    // Podpowiedz: aktywny pedzel to zwykle to, czego user szuka/usuwa.
    property int defaultFrom: (mapCtrl && mapCtrl.brushServerId > 0) ? mapCtrl.brushServerId : 100

    readonly property string scopeLabel: scope === "map" ? "mapie" : "zaznaczeniu"
    title: (mode === "find"   ? "Find Item"
          : mode === "remove" ? "Remove Item"
                              : "Replace Items")
           + (scope === "map" ? " (cala mapa)" : " on Selection")
    anchors.centerIn: parent
    width: 340


    // Wynik ostatniej operacji (licznik) - pokazujemy zamiast zamykac od razu.
    property string resultText: ""

    onOpened: {
        fromField.value = defaultFrom
        toField.value = defaultFrom
        resultText = ""
    }

    contentItem: Column {
        spacing: 8
        padding: 10

        Text {
            text: root.mode === "replace" ? ("Podmien itemy na " + root.scopeLabel + ":")
                                          : (root.mode === "remove"
                                             ? ("Usun itemy o server-id z " + root.scopeLabel + ":")
                                             : ("Znajdz itemy o server-id na " + root.scopeLabel + ":"))
            color: "#c0c0c0"; font.pixelSize: 12
        }

        Row {
            spacing: 6
            Text {
                text: root.mode === "replace" ? "Z (server id)" : "Server id"
                color: "#999"; font.pixelSize: 11
                width: 80
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox { id: fromField; width: 110; from: 100; to: 65535 }
            Text {
                text: (root.mapCtrl && otbReader) ? otbReader.nameForServerId(fromField.value) : ""
                color: "#7f9f7f"; font.pixelSize: 10
                width: 120; elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 6
            visible: root.mode === "replace"
            Text {
                text: "Na (server id)"; color: "#999"; font.pixelSize: 11
                width: 80
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox { id: toField; width: 110; from: 100; to: 65535 }
            Text {
                text: (root.mapCtrl && otbReader) ? otbReader.nameForServerId(toField.value) : ""
                color: "#7f9f7f"; font.pixelSize: 10
                width: 120; elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Text {
            text: root.resultText
            visible: root.resultText.length > 0
            color: "#eaffea"; font.pixelSize: 11; font.bold: true
        }

        Row {
            spacing: 6
            TibiaButton {
                text: root.mode === "find" ? "Policz" : (root.mode === "remove" ? "Usun" : "Podmien")
                width: 100
                onClicked: {
                    if (!root.mapCtrl) return
                    const onMap = root.scope === "map"
                    if (root.mode === "find") {
                        const n = onMap ? otbmReader.countItemsOnMap(fromField.value)
                                        : root.mapCtrl.countItemOnSelection(fromField.value)
                        // Na mapie od razu skaczemy do pierwszego trafienia (jak RME).
                        if (onMap && n > 0) root.mapCtrl.jumpToItemOnMap(fromField.value)
                        root.resultText = n > 0
                            ? ("Znaleziono: " + n + (onMap ? " (skok do pierwszego)" : ""))
                            : "Nie znaleziono"
                    } else if (root.mode === "remove") {
                        const n = onMap ? root.mapCtrl.removeItemsOnMap(fromField.value)
                                        : root.mapCtrl.removeItemOnSelection(fromField.value)
                        root.resultText = "Usunieto: " + n
                    } else {
                        const n = onMap ? root.mapCtrl.replaceItemsOnMap(fromField.value, toField.value)
                                        : root.mapCtrl.replaceItemsOnSelection(fromField.value, toField.value)
                        root.resultText = "Podmieniono: " + n
                    }
                }
            }
            TibiaButton { text: "Zamknij"; width: 100; onClicked: root.close() }
        }
    }
}
