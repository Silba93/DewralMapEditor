import Tibia 1.0
import QtQuick

Item {
    id: root
    property int topBorder: 27
    property url frameSource: (Backend.uiTheme.tex + "popupwindow.png")
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    Rectangle {
        anchors.fill: parent
        visible: root.githubUi
        radius: 7
        color: "#161B22"
        border {
            width: 1
            color: "#30363D"
        }
    }

    BorderImage {
        anchors.fill: parent
        visible: !root.githubUi
        source: root.frameSource
        smooth: false
        border {
            left: 6
            right: 6
            top: root.topBorder
            bottom: 6
        }

        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat
    }
}
