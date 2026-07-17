import QtQuick

// Checkbox w stylu classic Tibia UI - zastepuje wlasnorecznie rysowane Rectangle
// przelaczniki (np. "Show shade", "Show lower floors"). Zrodlo: checkbox_2.png
// (12x24, 2 stany po 12px), stany wykrojone offline -> checkbox_off.png/checkbox_on.png.
Item {
    id: root
    signal clicked()
    property bool checked: false
    property alias text: label.text

    implicitWidth: box.width + (label.text.length > 0 ? label.implicitWidth + 8 : 0)
    implicitHeight: Math.max(box.height, label.implicitHeight)

    Image {
        id: box
        width: 12; height: 12
        anchors.verticalCenter: parent.verticalCenter
        smooth: false
        source: root.checked ? (uiTheme.tex + "checkbox_on.png") : (uiTheme.tex + "checkbox_off.png")
    }

    Text {
        id: label
        anchors { left: box.right; leftMargin: 8; verticalCenter: parent.verticalCenter }
        color: "#c0c0c0"
        font.pixelSize: 12
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
