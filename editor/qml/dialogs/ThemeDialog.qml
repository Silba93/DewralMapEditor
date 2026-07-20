import Tibia 1.0
import QtQuick
import QtQuick.Controls
import "../style"

TibiaDialog {
    id: root

    title: "UI Theme"
    width: 320

    readonly property color draft: Qt.rgba(rField.value / 255, gField.value / 255, bField.value / 255, 1)

    function loadFromTheme() {
        rField.value = Math.round(Backend.uiTheme.tint.r * 255);
        gField.value = Math.round(Backend.uiTheme.tint.g * 255);
        bField.value = Math.round(Backend.uiTheme.tint.b * 255);
    }
    onAboutToShow: loadFromTheme()

    contentItem: Column {
        spacing: 10

        Text {
            text: "Application style"
            color: "#999"
            font.pixelSize: 11
        }
        TibiaComboBox {
            id: styleCombo
            width: parent.width - 24
            height: 23
            model: Backend.uiTheme.styles.map(function (s) {
                return s.name;
            })
            currentIndex: {
                for (var i = 0; i < Backend.uiTheme.styles.length; ++i)
                    if (Backend.uiTheme.styles[i].id === Backend.uiTheme.style)
                        return i;
                return 0;
            }
            onActivated: Backend.uiTheme.style = Backend.uiTheme.styles[currentIndex].id
        }

        TibiaSeparator {
            width: parent.width - 24
        }

        Text {
            text: "Presets"
            color: "#999"
            font.pixelSize: 11
        }

        Grid {
            columns: 4
            spacing: 4
            Repeater {
                model: Backend.uiTheme.presets
                delegate: TibiaButton {
                    required property var modelData
                    text: modelData.name
                    width: 68
                    onClicked: {
                        Backend.uiTheme.tint = modelData.color;
                        root.loadFromTheme();
                    }
                }
            }
        }

        TibiaSeparator {
            width: parent.width - 24
        }

        Text {
            text: "Custom color (RGB)"
            color: "#999"
            font.pixelSize: 11
        }

        Row {
            spacing: 6
            Text {
                text: "R"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                id: rField
                width: 62
                from: 0
                to: 255
            }
            Text {
                text: "G"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                id: gField
                width: 62
                from: 0
                to: 255
            }
            Text {
                text: "B"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                id: bField
                width: 62
                from: 0
                to: 255
            }
        }

        Row {
            spacing: 6
            Text {
                text: "Preview"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                width: 40
                height: 18
                color: root.draft
                border {
                    width: 1
                    color: "#555"
                }
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.draft.toString()
                color: "#7f9f7f"
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            TibiaButton {
                text: "Apply"
                width: 90
                onClicked: Backend.uiTheme.tint = root.draft
            }
            TibiaButton {
                text: "Close"
                width: 90
                onClicked: root.close()
            }
        }
    }
}
