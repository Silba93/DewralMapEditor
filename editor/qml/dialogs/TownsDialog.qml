import QtQuick
import QtQuick.Controls
import "../style"

// Map > Edit Towns: lista miast + Add/Remove + edycja nazwy i pozycji swiatyni
// (jak RME). Edycje sa natychmiastowe w OtbmReader - Cancel nie cofa zmian.
TibiaDialog {
    id: townsDialog
    // Okno glowne (flaga dirty po zmianach).
    required property var app
    // MapView - "Go To" na swiatynie.
    required property var mapCtrl

    title: "Towns"
    width: 340

    property var towns: []          // snapshot [{name,id,x,y,z}] - odswiezany po kazdej zmianie
    property int selectedId: -1     // id wybranego miasta (-1 = brak)

    function refresh(keepId) {
        towns = otbmReader.townsList()
        selectedId = keepId !== undefined ? keepId : -1
        if (selectedId >= 0) {
            for (var i = 0; i < towns.length; ++i)
                if (towns[i].id === selectedId) { townsList.currentIndex = i; return }
        }
        townsList.currentIndex = -1
    }
    function selected() {
        return selectedId >= 0 ? towns.find(function(t) { return t.id === selectedId }) : null
    }
    // TibiaTextField oddaje nazwe dopiero na Enter/utrate focusu, a TibiaButton focusu
    // nie zabiera - bez tego "wpisz nazwe i kliknij OK" gubiloby zmiane.
    function commitName() {
        if (townsDialog.selectedId >= 0 && nameField.text !== "")
            otbmReader.renameTown(townsDialog.selectedId, nameField.text)
    }

    onAboutToShow: refresh()

    contentItem: Column {
        spacing: 8

        // Lista miast na teksturze panelu - wczesniej byl goly ciemny prostokat.
        TibiaPanel {
            width: parent.width
            height: 140

            ListView {
                id: townsList
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                model: townsDialog.towns
                highlightMoveDuration: 0
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: townsList.width - 4
                    height: 24
                    // Zaznaczenie/hover jak w menu classic UI: szare podswietlenie.
                    Rectangle {
                        anchors.fill: parent
                        visible: townsList.currentIndex === index || tma.containsMouse
                        color: townsList.currentIndex === index ? "#585858" : "#454545"
                        border { width: 1; color: "#6a6a6a" }
                    }
                    Text {
                        anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                        text: modelData.name
                        color: "#c0c0c0"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: tma
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            townsList.currentIndex = index
                            townsDialog.selectedId = modelData.id
                        }
                    }
                }
            }

            // TibiaScrollBar to Item sterowany wlasnoscia "flickable" (nie podklasa
            // ScrollBar), wiec wpina sie obok listy - tak samo jak w PalettePanel.
            TibiaScrollBar {
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                anchors.margins: 2
                flickable: townsList
            }
        }

        Row {
            spacing: 6
            TibiaButton {
                text: "Add"
                width: 90
                onClicked: townsDialog.refresh(otbmReader.addTown())
            }
            TibiaButton {
                text: "Remove"
                width: 90
                enabled: townsDialog.selectedId >= 0
                onClicked: {
                    otbmReader.removeTown(townsDialog.selectedId)
                    townsDialog.refresh()
                }
            }
        }

        // --- Name / ID ---
        Text { text: "Name / ID"; color: "#999"; font.pixelSize: 11 }
        Row {
            id: nameRow
            spacing: 6
            property var t: townsDialog.selected()
            enabled: t !== null && t !== undefined
            TibiaTextField {
                id: nameField
                width: 210
                text: parent.t ? parent.t.name : ""
                onEditingFinished: {
                    townsDialog.commitName()
                    townsDialog.refresh(townsDialog.selectedId)
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: parent.t ? ("" + parent.t.id) : ""
                color: "#888"; font.pixelSize: 12
            }
        }

        // --- Temple Position ---
        Text { text: "Temple Position"; color: "#999"; font.pixelSize: 11 }
        Row {
            spacing: 6
            property var t: townsDialog.selected()
            property bool hasSel: t !== null && t !== undefined

            function applyTemple() {
                if (townsDialog.selectedId < 0) return
                otbmReader.setTownTemple(townsDialog.selectedId, xField.value, yField.value, zField.value)
                townsDialog.refresh(townsDialog.selectedId)
            }

            TibiaSpinBox {
                id: xField; width: 78; from: 0; to: 65535
                value: parent.t ? parent.t.x : 0
                enabled: parent.hasSel
                onValueModified: parent.applyTemple()
            }
            TibiaSpinBox {
                id: yField; width: 78; from: 0; to: 65535
                value: parent.t ? parent.t.y : 0
                enabled: parent.hasSel
                onValueModified: parent.applyTemple()
            }
            TibiaSpinBox {
                id: zField; width: 62; from: 0; to: 15
                value: parent.t ? parent.t.z : 0
                enabled: parent.hasSel
                onValueModified: parent.applyTemple()
            }
            TibiaButton {
                text: "Go To"
                width: 70
                enabled: parent.hasSel
                onClicked: townsDialog.mapCtrl.centerOnTile(xField.value, yField.value, zField.value)
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            TibiaButton {
                text: "OK"
                width: 90
                onClicked: {
                    townsDialog.commitName()   // patrz komentarz przy commitName()
                    // dirty ustawia sam reader (mapChanged -> dirty) - karty map.
                    townsDialog.close()
                }
            }
            // Edycje sa natychmiastowe w modelu - "Anuluj" ich NIE cofa, tylko zamyka okno.
            TibiaButton { text: "Anuluj"; width: 90; onClicked: townsDialog.close() }
        }
    }
}
