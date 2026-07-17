import QtQuick
import QtQuick.Controls
import "../style"

// Wybor motywu UI. Kolor jest nakladany MULTIPLY na tekstury classic UI, wiec
// dziala jak barwiona folia: przebarwia zachowujac cieniowanie, ale nie rozjasnia.
// Stad presety sa jasne - im ciemniejszy kolor, tym ciemniejszy caly UI.
TibiaDialog {
    id: root

    title: "Motyw UI"
    width: 320


    // Kolor budowany ze skladowych - podglad zyje na biezaco, zatwierdzamy przyciskiem.
    readonly property color draft: Qt.rgba(rField.value / 255, gField.value / 255,
                                           bField.value / 255, 1)

    function loadFromTheme() {
        rField.value = Math.round(uiTheme.tint.r * 255)
        gField.value = Math.round(uiTheme.tint.g * 255)
        bField.value = Math.round(uiTheme.tint.b * 255)
    }
    onAboutToShow: loadFromTheme()

    contentItem: Column {
        spacing: 10

        Text {
            text: "Gotowe motywy"
            color: "#999"; font.pixelSize: 11
        }

        Grid {
            columns: 4
            spacing: 4
            Repeater {
                model: uiTheme.presets
                delegate: TibiaButton {
                    required property var modelData
                    text: modelData.name
                    width: 68
                    onClicked: { uiTheme.tint = modelData.color; root.loadFromTheme() }
                }
            }
        }

        TibiaSeparator { width: parent.width - 24 }

        Text {
            text: "Wlasny kolor (RGB)"
            color: "#999"; font.pixelSize: 11
        }

        Row {
            spacing: 6
            Text { text: "R"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            TibiaSpinBox { id: rField; width: 62; from: 0; to: 255 }
            Text { text: "G"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            TibiaSpinBox { id: gField; width: 62; from: 0; to: 255 }
            Text { text: "B"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
            TibiaSpinBox { id: bField; width: 62; from: 0; to: 255 }
        }

        Row {
            spacing: 6
            Text {
                text: "Podglad"
                color: "#999"; font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                width: 40; height: 18
                color: root.draft
                border { width: 1; color: "#555" }
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.draft.toString()
                color: "#7f9f7f"; font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            TibiaButton { text: "Zastosuj"; width: 90; onClicked: uiTheme.tint = root.draft }
            TibiaButton { text: "Zamknij"; width: 90; onClicked: root.close() }
        }
    }
}
