import Tibia 1.0
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
    padding: 1
    overlap: 0

    background: Item {
        implicitWidth: 150
        Image {
            anchors.fill: parent
            source: (Backend.uiTheme.tex + "texture.png")
            fillMode: Image.Tile
            smooth: false
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: "#6e6e6e"
        }
    }

    delegate: MenuItem {
        id: menuItem
        implicitHeight: 24
        padding: 0
        spacing: 0

        contentItem: Item {
            implicitWidth: itemText.implicitWidth + 10 + (menuItem.subMenu !== null ? 22 : 10)
            implicitHeight: 24
            Text {
                id: itemText
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: menuItem.text
                color: !menuItem.enabled ? "#777" : (menuItem.highlighted ? "#eaffea" : "#dcdcdc")
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
        }
        indicator: Item {}
        arrow: Text {
            visible: menuItem.subMenu !== null
            text: ">"
            color: menuItem.highlighted ? "#eaffea" : "#999"
            font.pixelSize: 10
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }
        background: Rectangle {

            color: menuItem.highlighted ? "#807a7d82" : "transparent"
            border.width: menuItem.highlighted ? 1 : 0
            border.color: "#9a9a9a"
        }
    }
}
