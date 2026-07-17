import QtQuick

// Ciemny przycisk/pole paska narzedzi - PLASKIE ciemne tlo (#2b2b2b) + szara ramka,
// czyli styl jaki przyciski mialy przed przejsciem na teksture panel_side.png.
// Uzywany tam, gdzie liczy sie czytelnosc odczytu (np. numer pietra i steppery).
//
// readOnly = pole (bez hovera/klikania), inaczej przycisk.
Item {
    id: root
    property string label: ""
    property bool readOnly: false
    property bool active: false
    property color activeBg: "#2f6f4f"
    property color activeBorder: "#7fdc8f"
    property color textColor: "#eaffea"
    signal clicked()

    implicitWidth: labelText.implicitWidth + 18
    implicitHeight: 28

    Rectangle {
        anchors.fill: parent
        radius: 3
        color: root.active ? root.activeBg
                           : (!root.readOnly && ma.pressed ? "#222"
                              : (!root.readOnly && ma.containsMouse ? "#3a3a3a" : "#2b2b2b"))
        border.width: 1
        border.color: root.active ? root.activeBorder : "#555"
    }

    Text {
        id: labelText
        anchors.centerIn: parent
        text: root.label
        color: root.textColor
        font.pixelSize: 12
        font.bold: true
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        enabled: !root.readOnly
        hoverEnabled: !root.readOnly
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
