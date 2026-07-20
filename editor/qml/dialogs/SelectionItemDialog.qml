import Tibia 1.0
import QtQuick
import QtQuick.Controls
import "../style"

TibiaDialog {
    id: root
    property string mode: "find"
    property string scope: "selection"
    property var mapCtrl: null

    property int defaultFrom: (mapCtrl && mapCtrl.brushServerId > 0) ? mapCtrl.brushServerId : 100

    readonly property string scopeLabel: scope === "map" ? "map" : "selection"
    title: (mode === "find" ? "Find Item" : mode === "remove" ? "Remove Item" : "Replace Items") + (scope === "map" ? " (entire map)" : " on selection")
    anchors.centerIn: parent
    width: 340

    property string resultText: ""

    onOpened: {
        fromField.value = defaultFrom;
        toField.value = defaultFrom;
        resultText = "";
    }

    contentItem: Column {
        spacing: 8
        padding: 10

        Text {
            text: root.mode === "replace" ? ("Replace items in " + root.scopeLabel + ":") : (root.mode === "remove" ? ("Remove items by server ID from " + root.scopeLabel + ":") : ("Find items by server ID in " + root.scopeLabel + ":"))
            color: "#c0c0c0"
            font.pixelSize: 12
        }

        Row {
            spacing: 6
            Text {
                text: root.mode === "replace" ? "From (server ID)" : "Server ID"
                color: "#999"
                font.pixelSize: 11
                width: 80
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                id: fromField
                width: 110
                from: 100
                to: 65535
            }
            Text {
                text: (root.mapCtrl && Backend.otbReader) ? Backend.otbReader.nameForServerId(fromField.value) : ""
                color: "#7f9f7f"
                font.pixelSize: 10
                width: 120
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 6
            visible: root.mode === "replace"
            Text {
                text: "To (server ID)"
                color: "#999"
                font.pixelSize: 11
                width: 80
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                id: toField
                width: 110
                from: 100
                to: 65535
            }
            Text {
                text: (root.mapCtrl && Backend.otbReader) ? Backend.otbReader.nameForServerId(toField.value) : ""
                color: "#7f9f7f"
                font.pixelSize: 10
                width: 120
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Text {
            text: root.resultText
            visible: root.resultText.length > 0
            color: "#eaffea"
            font.pixelSize: 11
            font.bold: true
        }

        Row {
            spacing: 6
            TibiaButton {
                text: root.mode === "find" ? "Count" : (root.mode === "remove" ? "Remove" : "Replace")
                width: 100
                onClicked: {
                    if (!root.mapCtrl)
                        return;
                    const onMap = root.scope === "map";
                    if (root.mode === "find") {
                        const n = onMap ? Backend.otbmReader.countItemsOnMap(fromField.value) : root.mapCtrl.countItemOnSelection(fromField.value);

                        if (onMap && n > 0)
                            root.mapCtrl.jumpToItemOnMap(fromField.value);
                        root.resultText = n > 0 ? ("Znaleziono: " + n + (onMap ? " (skok do pierwszego)" : "")) : "Not found";
                    } else if (root.mode === "remove") {
                        const n = onMap ? root.mapCtrl.removeItemsOnMap(fromField.value) : root.mapCtrl.removeItemOnSelection(fromField.value);
                        root.resultText = "Removed: " + n;
                    } else {
                        const n = onMap ? root.mapCtrl.replaceItemsOnMap(fromField.value, toField.value) : root.mapCtrl.replaceItemsOnSelection(fromField.value, toField.value);
                        root.resultText = "Replaced: " + n;
                    }
                }
            }
            TibiaButton {
                text: "Close"
                width: 100
                onClicked: root.close()
            }
        }
    }
}
