import Tibia 1.0
import QtQuick

Item {
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    Rectangle {
        anchors.fill: parent
        visible: parent.githubUi
        radius: 7
        color: "#10151C"
        border {
            width: 1
            color: "#242D38"
        }
    }

    BorderImage {
        anchors.fill: parent
        visible: !parent.githubUi
        source: (Backend.uiTheme.tex + "panel_flat.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }

        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat
    }
}
