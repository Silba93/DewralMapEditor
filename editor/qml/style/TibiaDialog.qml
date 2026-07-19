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
        // Tytul WYSRODKOWANY - tak wygladaja okna w kliencie Tibii (naglowek
        // wycentrowany nad trescia), a przy okazji rownowazy okna z szeroka trescia.
        Text {
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
            }
            text: root.title
            color: "#c0c0c0"
            font.bold: true
            font.pixelSize: 13
        }
        // BEZ separatora pod tytulem: popupwindow.png ma juz wlasny bevel na granicy
        // naglowka (border-top=27), wiec dokladana kreska dawala PODWOJNA linie, na
        // dodatek urwana 8px przed krawedzia - to wygladalo jak uszkodzona ramka.
    }
}
