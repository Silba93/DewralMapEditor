import QtQuick
import QtQuick.Controls

MenuItem {
    id: control
    implicitHeight: 24
    padding: 0
    spacing: 0

    contentItem: Item {

        implicitWidth: itemText.implicitWidth + 10 + (control.subMenu !== null ? 22 : 10)
        implicitHeight: 24
        Text {
            id: itemText

            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: !control.enabled ? "#777" : (control.highlighted ? "#eaffea" : "#dcdcdc")
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
        }
    }
    indicator: Item {}
    arrow: Text {
        visible: control.subMenu !== null
        text: ">"
        color: control.highlighted ? "#eaffea" : "#999"
        font.pixelSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
    }
    background: Rectangle {

        color: control.highlighted ? "#807a7d82" : "transparent"
        border.width: control.highlighted ? 1 : 0
        border.color: "#9a9a9a"
    }
}
