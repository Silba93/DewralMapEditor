import QtQuick

// SpinBox w stylu classic Tibia UI. Zrodlo: textedit.png (pole, border=1 z
// 10-textedits.otui) + spinbox_up.png/spinbox_down.png (strzalki, border=1,
// stany idle/hover/pressed z 20-spinboxes.otui, wykrojone offline).
Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    signal valueModified()

    implicitWidth: 96
    implicitHeight: 22

    function clamp(v) { return Math.max(root.from, Math.min(root.to, v)) }
    function setValue(v) {
        const c = clamp(v)
        if (c !== root.value) { root.value = c; root.valueModified() }
    }

    BorderImage {
        anchors.fill: parent
        anchors.rightMargin: 12
        source: (uiTheme.tex + "textedit.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 14
        verticalAlignment: TextInput.AlignVCenter
        color: "#c0c0c0"
        font.pixelSize: 12
        readOnly: !root.editable
        selectByMouse: true
        text: root.value
        validator: IntValidator { bottom: root.from; top: root.to }
        // Commit na KAZDA zmiane tekstu, nie dopiero na Enter/utrate focusu. TibiaButton to
        // goly MouseArea - klikniecie go nie zabiera focusu polu, wiec samo editingFinished
        // nie odpalalo sie i przycisk czytal STARA wartosc (wpisany server-id nie docieral
        // do C++). Podczas pisania Binding nizej jest nieaktywny (activeFocus), wiec clamp
        // nie przepisuje tekstu pod palcami.
        onTextEdited: root.setValue(parseInt(text || "0", 10))
        onEditingFinished: root.setValue(parseInt(text || "0", 10))
    }
    // Wartosc startowa moze byc ponizej from (domyslne 0 przy from=100) - znormalizuj,
    // zeby pole nie pokazywalo id spoza zakresu.
    Component.onCompleted: root.value = clamp(root.value)
    Binding { target: input; property: "text"; value: String(root.value); when: !input.activeFocus }

    Column {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 12
        Image {
            width: 10; height: 11
            source: upArea.pressed ? (uiTheme.tex + "spinbox_up_pressed.png")
                    : (upArea.containsMouse ? (uiTheme.tex + "spinbox_up_hover.png") : (uiTheme.tex + "spinbox_up_idle.png"))
            smooth: false
            MouseArea { id: upArea; anchors.fill: parent; hoverEnabled: true
                onClicked: root.setValue(root.value + root.stepSize) }
        }
        Image {
            width: 10; height: 11
            source: downArea.pressed ? (uiTheme.tex + "spinbox_down_pressed.png")
                    : (downArea.containsMouse ? (uiTheme.tex + "spinbox_down_hover.png") : (uiTheme.tex + "spinbox_down_idle.png"))
            smooth: false
            MouseArea { id: downArea; anchors.fill: parent; hoverEnabled: true
                onClicked: root.setValue(root.value - root.stepSize) }
        }
    }
}
