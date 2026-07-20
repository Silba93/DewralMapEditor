import QtQuick
import QtQuick.Controls

MenuBar {
    id: root
    background: Rectangle {
        color: "transparent"
    }
    delegate: MenuBarItem {
        id: menuBarItem
        implicitHeight: 26
        contentItem: Text {
            text: menuBarItem.text
            color: menuBarItem.highlighted ? "#eaffea" : "#c0c0c0"
            font.pixelSize: 12
            font.bold: menuBarItem.highlighted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: menuBarItem.highlighted ? "#1fffffff" : "transparent"
        }
    }
}
