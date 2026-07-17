import QtQuick

// Pole tekstowe w stylu classic Tibia UI. Zrodlo: textedit.png, border=1
// (10-textedits.otui -> TextEdit).
Item {
    id: root
    // "text" jako alias juz automatycznie generuje sygnal zmiany (onTextChanged
    // dziala od razu na instancji) - jawna deklaracja "signal textChanged()"
    // kolidowalaby z nim (Duplicate signal name).
    property alias text: input.text
    property alias placeholderText: placeholder.text
    signal accepted()
    // Enter LUB utrata focusu. Uwaga: TibiaButton to goly MouseArea i focusu NIE zabiera,
    // wiec klikniecie przycisku myszka tego NIE odpali - okno, ktore zapisuje tekst
    // dopiero tutaj, musi dodatkowo zatwierdzic go samo w handlerze przycisku.
    signal editingFinished()

    implicitWidth: 140
    implicitHeight: 22

    BorderImage {
        anchors.fill: parent
        source: (uiTheme.tex + "textedit.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }

    Text {
        id: placeholder
        anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
        color: "#777"
        font.pixelSize: 12
        visible: input.text.length === 0
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        verticalAlignment: TextInput.AlignVCenter
        color: "#c0c0c0"
        font.pixelSize: 12
        clip: true
        selectByMouse: true
        onAccepted: root.accepted()
        onEditingFinished: root.editingFinished()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: input.forceActiveFocus()
    }
}
