import Tibia 1.0
import QtQuick

Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    signal valueModified

    implicitWidth: 96
    implicitHeight: 22

    function clamp(v) {
        return Math.max(root.from, Math.min(root.to, v));
    }
    function setValue(v) {
        const c = clamp(v);
        if (c !== root.value) {
            root.value = c;
            root.valueModified();
        }
    }

    BorderImage {
        anchors.fill: parent
        anchors.rightMargin: 12
        source: (Backend.uiTheme.tex + "textedit.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
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
        validator: IntValidator {
            bottom: root.from
            top: root.to
        }

        onTextEdited: root.setValue(parseInt(text || "0", 10))
        onEditingFinished: root.setValue(parseInt(text || "0", 10))
    }

    Component.onCompleted: root.value = clamp(root.value)
    Binding {
        target: input
        property: "text"
        value: String(root.value)
        when: !input.activeFocus
    }

    Column {
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        width: 12
        Image {
            width: 10
            height: 11
            source: upArea.pressed ? (Backend.uiTheme.tex + "spinbox_up_pressed.png") : (upArea.containsMouse ? (Backend.uiTheme.tex + "spinbox_up_hover.png") : (Backend.uiTheme.tex + "spinbox_up_idle.png"))
            smooth: false
            MouseArea {
                id: upArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.setValue(root.value + root.stepSize)
            }
        }
        Image {
            width: 10
            height: 11
            source: downArea.pressed ? (Backend.uiTheme.tex + "spinbox_down_pressed.png") : (downArea.containsMouse ? (Backend.uiTheme.tex + "spinbox_down_hover.png") : (Backend.uiTheme.tex + "spinbox_down_idle.png"))
            smooth: false
            MouseArea {
                id: downArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.setValue(root.value - root.stepSize)
            }
        }
    }
}
