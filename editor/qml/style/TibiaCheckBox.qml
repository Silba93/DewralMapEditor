import Tibia 1.0
import QtQuick

Item {
    id: root
    signal clicked
    property bool checked: false
    property alias text: label.text
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    implicitWidth: (root.githubUi ? githubBox.width : box.width) + (label.text.length > 0 ? label.implicitWidth + 8 : 0)
    implicitHeight: Math.max(root.githubUi ? githubBox.height : box.height, label.implicitHeight)

    Image {
        id: box
        width: 12
        height: 12
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.githubUi
        smooth: false
        source: root.checked ? (Backend.uiTheme.tex + "checkbox_on.png") : (Backend.uiTheme.tex + "checkbox_off.png")
    }

    Item {
        id: githubBox
        width: 14
        height: 14
        anchors.verticalCenter: parent.verticalCenter
        visible: root.githubUi

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: root.checked ? "#D6D6D6" : "#343434"
            border {
                width: 1
                color: root.checked ? "#D6D6D6" : "#888888"
            }
        }

        Text {
            anchors.centerIn: parent
            visible: root.checked
            text: "\u2713"
            color: "#343434"
            font {
                pixelSize: 11
                weight: Font.DemiBold
            }
        }
    }

    Text {
        id: label
        anchors {
            left: root.githubUi ? githubBox.right : box.right
            leftMargin: 8
            verticalCenter: parent.verticalCenter
        }
        color: Backend.uiTheme.style === "github-dark" ? "#D6D6D6" : "#c0c0c0"
        font.pixelSize: 12
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
