import QtQuick
import QtQuick.Controls

// Pasek menu (File/Edit/Map/View) w stylu classic Tibia UI. Tlo przezroczyste
// (siedzi juz na teksturowanym oknie - TibiaDialogBackground w Main.qml), tylko
// pozycje menu (delegate) dostaja klasyczne kolory + podswietlenie przy hover/open.
MenuBar {
    id: root
    background: Rectangle { color: "transparent" }
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
