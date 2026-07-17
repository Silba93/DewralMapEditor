import QtQuick
import QtQuick.Controls

// Baza dla WSZYSTKICH okien dialogowych w edytorze: tlo + naglowek + stopka w stylu
// classic UI. Goly Dialog bierze je ze stylu Basic (plaskie ciemne prostokaty z
// systemowymi przyciskami), co odstawalo od reszty UI.
//
// Celowo NIE uzywamy standardButtons: DialogButtonBox tworzy stockowe Buttony, a
// TibiaButton nie jest AbstractButtonem, wiec nie da sie go tam wstrzyknac jako
// delegata. Zamiast tego kazde okno sklada wlasny rzad TibiaButton i wola
// accept()/reject() - tak samo, tylko z kontrola nad wygladem.
Dialog {
    id: root

    modal: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    padding: 12

    background: TibiaDialogBackground {}

    header: Item {
        visible: root.title.length > 0
        implicitHeight: visible ? 28 : 0
        Text {
            anchors {
                left: parent.left
                leftMargin: 12
                verticalCenter: parent.verticalCenter
            }
            text: root.title
            color: "#c0c0c0"
            font.bold: true
            font.pixelSize: 13
        }
        // Cienka linia pod tytulem - oddziela naglowek od tresci (jak w oknach Tibii).
        TibiaSeparator {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            anchors { leftMargin: 8; rightMargin: 8 }
        }
    }
}
