import Tibia 1.0
import QtQuick

Item {
    id: root
    signal clicked
    property alias text: label.text

    property bool checked: false
    property string variant: "default"

    implicitWidth: Math.max(60, label.implicitWidth + 16)
    implicitHeight: root.githubUi ? 30 : 22
    opacity: enabled ? 1.0 : 0.5

    readonly property bool active: checked || mouseArea.pressed
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    BorderImage {
        anchors.fill: parent
        visible: !root.githubUi
        source: root.active ? (Backend.uiTheme.tex + "button_active.png") : (Backend.uiTheme.tex + "button_normal.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.githubUi
        radius: 6
        color: {
            if (root.checked)
                return "#174D2B";
            if (root.variant === "primary")
                return mouseArea.pressed ? "#1F6F35" : (mouseArea.containsMouse ? "#2EA043" : "#238636");
            if (root.variant === "danger")
                return mouseArea.pressed ? "#8E1F22" : (mouseArea.containsMouse ? "#DA3633" : "#B62324");
            return mouseArea.pressed ? "#30363D" : (mouseArea.containsMouse ? "#252C35" : "#21262D");
        }
        border {
            width: 1
            color: {
                if (root.checked)
                    return "#3FB950";
                if (root.variant === "primary")
                    return mouseArea.containsMouse ? "#56D364" : "#2EA043";
                if (root.variant === "danger")
                    return mouseArea.containsMouse ? "#F85149" : "#DA3633";
                return mouseArea.containsMouse ? "#8B949E" : "#30363D";
            }
        }
    }

    Text {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.active ? 1 : 0
        color: Backend.uiTheme.style === "github-dark"
               ? (root.enabled ? "#F0F6FC" : "#7D8590")
               : (root.enabled ? "#c0c0c0" : "#777")
        font.weight: root.githubUi ? Font.DemiBold : Font.Bold
        font.pixelSize: 12
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
