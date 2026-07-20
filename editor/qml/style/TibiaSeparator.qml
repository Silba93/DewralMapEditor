import Tibia 1.0
import QtQuick

Item {
    id: root
    property bool vertical: false

    implicitWidth: vertical ? 2 : 100
    implicitHeight: vertical ? 100 : 2

    BorderImage {
        anchors.fill: parent
        source: root.vertical ? (Backend.uiTheme.tex + "separator_vertical.png") : (Backend.uiTheme.tex + "separator_horizontal.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
    }
}
