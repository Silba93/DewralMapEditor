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

    BorderImage {
        anchors.fill: parent
        source: (Backend.uiTheme.tex + "textedit.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
    }

    Text {
        id: placeholder
        anchors {
            left: parent.left
            leftMargin: 6
            verticalCenter: parent.verticalCenter
        }
        color: "#777"
        font.pixelSize: 12
        visible: input.text.length === 0
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        verticalAlignment: TextInput.AlignVCenter
        color: "#c0c0c0"
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
