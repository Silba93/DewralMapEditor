import Tibia 1.0
import QtQuick

Item {
    id: root

    property alias text: input.text
    property alias placeholderText: placeholder.text
    signal accepted

    signal editingFinished

    implicitWidth: 140
    implicitHeight: 22
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    BorderImage {
        anchors.fill: parent
        visible: !root.githubUi
        source: (Backend.uiTheme.tex + "textedit.png")
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
        color: "#0D1117"
        border {
            width: 1
            color: input.activeFocus ? "#2EA043" : "#30363D"
        }
    }

    Text {
        id: placeholder
        anchors {
            left: parent.left
            leftMargin: 6
            verticalCenter: parent.verticalCenter
        }
        color: Backend.uiTheme.style === "github-dark" ? "#7D8590" : "#777"
        font.pixelSize: 12
        visible: input.text.length === 0
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        verticalAlignment: TextInput.AlignVCenter
        color: Backend.uiTheme.style === "github-dark" ? "#C9D1D9" : "#c0c0c0"
        font.pixelSize: 12
        clip: true
        selectByMouse: true
        onAccepted: root.accepted()
        onEditingFinished: root.editingFinished()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: input.forceActiveFocus()
    }
}
