import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property var notes: []
    property int selectedIndex: -1
    property int targetX: 0
    property int targetY: 0
    property int targetZ: 0

    title: "Notes"
    width: 420

    function refresh() {
        notes = Backend.otbmReader.notesList();
        selectedIndex = -1;
        noteText.text = "";
    }

    function openAt(x, y, z, text) {
        targetX = x;
        targetY = y;
        targetZ = z;
        selectedIndex = -1;
        noteText.text = text || Backend.otbmReader.noteText(x, y, z);
        open();
        Qt.callLater(function() { noteText.forceActiveFocus(); });
    }

    function selectNote(index) {
        if (index < 0 || index >= notes.length) return;
        const note = notes[index];
        selectedIndex = index;
        targetX = note.x;
        targetY = note.y;
        targetZ = note.z;
        noteText.text = note.text;
    }

    function saveNote() {
        if (Backend.otbmReader.setNote(targetX, targetY, targetZ, noteText.text)) {
            refresh();
            close();
        }
    }

    function deleteNote() {
        if (Backend.otbmReader.setNote(targetX, targetY, targetZ, "")) {
            refresh();
            close();
        }
    }

    onOpened: {
        if (selectedIndex < 0 && noteText.text.length === 0)
            refresh();
    }

    contentItem: Column {
        spacing: 8

        DmePanel {
            width: parent.width
            height: 170

            ListView {
                id: noteList
                anchors.fill: parent
                anchors.margins: 2
                anchors.rightMargin: 14
                clip: true
                model: dialog.notes
                delegate: Rectangle {
                    id: noteRow
                    required property var modelData
                    required property int index
                    width: noteList.width
                    height: 34
                    color: noteList.currentIndex === index ? "#505050"
                                                             : (rowMouse.containsMouse ? "#383838" : "transparent")

                    Text {
                        anchors { left: parent.left; leftMargin: 7; top: parent.top; topMargin: 4 }
                        width: parent.width - 14
                        text: noteRow.modelData.text
                        color: "#c0c0c0"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors { left: parent.left; leftMargin: 7; bottom: parent.bottom; bottomMargin: 3 }
                        text: noteRow.modelData.x + ", " + noteRow.modelData.y + ", " + noteRow.modelData.z
                        color: "#777"
                        font.pixelSize: 9
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            noteList.currentIndex = noteRow.index;
                            dialog.selectNote(noteRow.index);
                        }
                        onDoubleClicked: dialog.mapCtrl.centerOnPosition(
                            noteRow.modelData.x, noteRow.modelData.y, noteRow.modelData.z)
                    }
                }
            }
            DmeScrollBar {
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                anchors.margins: 2
                flickable: noteList
            }
        }

        Text {
            text: "Position: " + dialog.targetX + ", " + dialog.targetY + ", " + dialog.targetZ
            color: "#999"
            font.pixelSize: 11
        }
        TextArea {
            id: noteText
            width: parent.width
            height: 90
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            color: dialog.grayTheme ? "#eeeeee" : "#c0c0c0"
            placeholderText: "Note text"
            background: Rectangle {
                color: dialog.grayTheme ? "#151515" : "#202020"
                border.width: 1
                border.color: noteText.activeFocus ? "#4a90e2" : "#444"
            }
        }
        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton { text: "Save"; width: 82; variant: "primary"; onClicked: dialog.saveNote() }
            DmeButton { text: "Delete"; width: 82; variant: "danger"; enabled: Backend.otbmReader.noteText(dialog.targetX, dialog.targetY, dialog.targetZ) !== ""; onClicked: dialog.deleteNote() }
            DmeButton { text: "Close"; width: 82; onClicked: dialog.close() }
        }
    }
}
