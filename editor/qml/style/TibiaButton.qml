import QtQuick

// Przycisk w stylu classic Tibia UI (zrodlo tekstury: TibiaClient data/images/ui/buttons.png,
// wartosci 1:1 z data/styles/10-buttons.otui -> Button: image-border=1, stany wykrojone
// offline do button_normal.png / button_active.png, patrz editor/ui/).
Item {
    id: root
    signal clicked()
    property alias text: label.text
    // "enabled" NIE deklarujemy - Item juz je ma wbudowane (redeklaracja kolidowalaby
    // z wbudowanym sygnalem enabledChanged, jak przy "text"/"textChanged" w TibiaTextField).
    property bool checked: false   // np. przelaczniki "Show shade" - traktowane jak wcisniety

    implicitWidth: Math.max(60, label.implicitWidth + 16)
    implicitHeight: 22
    opacity: enabled ? 1.0 : 0.5

    readonly property bool active: checked || mouseArea.pressed

    BorderImage {
        anchors.fill: parent
        source: root.active ? (uiTheme.tex + "button_active.png") : (uiTheme.tex + "button_normal.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }

    Text {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.active ? 1 : 0
        color: root.enabled ? "#c0c0c0" : "#777"
        font.bold: true
        font.pixelSize: 12
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
