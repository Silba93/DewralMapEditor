import Tibia 1.0
import QtQuick

Item {
    id: root
    property bool vertical: false
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    implicitWidth: vertical ? 2 : 100
    implicitHeight: vertical ? 100 : 2

    BorderImage {
        anchors.fill: parent
        visible: !root.githubUi
        source: root.vertical ? (Backend.uiTheme.tex + "separator_vertical.png") : (Backend.uiTheme.tex + "separator_horizontal.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
    }

    Rectangle {
        anchors.centerIn: parent
        visible: root.githubUi
        width: root.vertical ? 1 : parent.width
        height: root.vertical ? parent.height : 1
        color: "#646464"
    }
}
