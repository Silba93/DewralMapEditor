import QtQuick
import QtQuick.Controls

// Pojedyncza pozycja menu w stylu classic Tibia UI. Uzywana dwojako:
//  - jako delegate w TibiaMenu (pozycje tworzone z Action),
//  - bezposrednio tam, gdzie MenuItem wstawiany jest recznie/Instantiatorem
//    (te omijaja delegate menu, wiec musza byc juz TibiaMenuItem).
// Tekst KOTWICZONY do lewej (nie polegamy na horizontalAlignment - styl Basic potrafi
// centrowac nadpisany contentItem). Szerokosc liczona z tekstu, bez lewej kolumny
// na "ptaszek" (indicator wyzerowany). padding/spacing=0 zeby nie bylo dziur.
MenuItem {
    id: control
    implicitHeight: 24
    padding: 0
    spacing: 0

    contentItem: Item {
        // Szerokosc = tekst + lewy margines 10 + prawy 10 (lub 22 gdy submenu, na strzalke).
        implicitWidth: itemText.implicitWidth + 10 + (control.subMenu !== null ? 22 : 10)
        implicitHeight: 24
        Text {
            id: itemText
            // Bez anchors.right - naturalna szerokosc, zeby menu samo sie rozszerzalo
            // do najdluzszej pozycji (z anchors.right tekst byl przycinany).
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
        text: "❯"   // ❯ - strzalka submenu
        color: control.highlighted ? "#eaffea" : "#999"
        font.pixelSize: 10
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
    }
    background: Rectangle {
        // QML uzywa #AARRGGBB (alfa na poczatku): szare pol-przezroczyste zaznaczenie
        // + 1px szara ramka (tekst czytelny). Patrz TibiaMenu.qml.
        color: control.highlighted ? "#807a7d82" : "transparent"
        border.width: control.highlighted ? 1 : 0
        border.color: "#9a9a9a"
    }
}
