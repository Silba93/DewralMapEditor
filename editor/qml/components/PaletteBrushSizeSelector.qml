import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Column {
    id: root

    required property var mapCtrl
    required property bool githubUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    spacing: githubUi ? 9 : 3

    Rectangle {
        visible: root.githubUi
        width: parent.width
        height: visible ? 1 : 0
        color: root.grayUi ? "#3A3A3A" : "#242D38"
    }

    Text {
        text: "Brush size"
        color: root.grayUi ? "#F0F0F0" : (root.githubUi ? "#E6EDF3" : "#ddd")
        font.pixelSize: root.githubUi ? 12 : 11
        font.bold: true
    }

    Flow {
        width: parent.width
        spacing: root.githubUi ? 5 : 3

        Repeater {
            model: ["square", "circle"]
            delegate: PaletteBrushButton {
                required property string modelData
                githubStyle: root.githubUi
                active: root.mapCtrl.brushShape === modelData
                round: modelData === "circle"
                iconSize: 14
                onClicked: root.mapCtrl.brushShape = modelData
            }
        }

        Item {
            width: root.githubUi ? 6 : 10
            height: 26
        }

        Repeater {
            model: [0, 1, 2, 4, 6, 8, 11]
            delegate: PaletteBrushButton {
                required property int modelData
                required property int index
                githubStyle: root.githubUi
                active: root.mapCtrl.brushSize === modelData
                round: root.mapCtrl.brushShape === "circle"
                iconSize: 6 + index * 2
                onClicked: root.mapCtrl.brushSize = modelData
                ToolTip.visible: !root.githubUi && hovered
                ToolTip.text: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)

                GithubToolTip {
                    targetItem: hoverArea
                    targetHovered: hovered
                    message: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                }
            }
        }
    }

    Row {
        visible: root.mapCtrl.doodadBrush.length > 0
        width: parent.width
        height: visible ? 26 : 0
        spacing: root.githubUi ? 6 : 4

        Text {
            id: densityLabel
            text: "Density"
            color: root.grayUi ? "#F0F0F0" : (root.githubUi ? "#E6EDF3" : "#ddd")
            font.pixelSize: root.githubUi ? 12 : 11
            anchors.verticalCenter: parent.verticalCenter
        }

        Slider {
            id: doodadDensitySlider
            width: Math.max(45, parent.width - densityLabel.implicitWidth - densityValue.implicitWidth - parent.spacing * 2)
            from: 1
            to: 10
            stepSize: 1
            snapMode: Slider.SnapAlways
            value: root.mapCtrl.doodadDensity
            anchors.verticalCenter: parent.verticalCenter
            onMoved: root.mapCtrl.doodadDensity = Math.round(value)
            ToolTip.visible: hovered
            ToolTip.text: "Doodad density: " + Math.round(value) + "/10"
        }

        Text {
            id: densityValue
            text: Math.round(doodadDensitySlider.value) + "/10"
            color: root.grayUi ? "#B8B8B8" : (root.githubUi ? "#8B949E" : "#aaa")
            font.pixelSize: root.githubUi ? 11 : 10
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
