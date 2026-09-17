import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: gotoPosDialog

    required property var mapCtrl

    title: "Go To Position"

    function pastePosition() {
        // Accepts every style written by the map context menu "Copy Position
        // As" entries and by "Copy Position".
        var position = Backend.fileTools.positionFromText(Backend.fileTools.clipboardText());
        if (position.valid !== true)
            return false;
        xField2.value = position.x;
        yField2.value = position.y;
        zField2.value = position.z;
        return true;
    }

    function go() {
        mapCtrl.centerOnTile(xField2.value, yField2.value, zField2.value);
        gotoPosDialog.close();
    }

    onOpened: xField2.focusEditor()

    contentItem: Column {
        spacing: 8

        Keys.onReturnPressed: gotoPosDialog.go()
        Keys.onEnterPressed: gotoPosDialog.go()

        Row {
            spacing: 6
            DmeSpinBox {
                id: xField2
                width: 78
                from: 0
                to: 65535
                value: Math.round(gotoPosDialog.mapCtrl.glOriginX())
                nextTabItem: yField2
                previousTabItem: zField2
                pasteHandler: function() { return gotoPosDialog.pastePosition(); }
            }
            DmeSpinBox {
                id: yField2
                width: 78
                from: 0
                to: 65535
                value: Math.round(gotoPosDialog.mapCtrl.glOriginY())
                nextTabItem: zField2
                previousTabItem: xField2
                pasteHandler: function() { return gotoPosDialog.pastePosition(); }
            }
            DmeSpinBox {
                id: zField2
                width: 62
                from: 0
                to: 15
                value: gotoPosDialog.mapCtrl.floor
                nextTabItem: xField2
                previousTabItem: yField2
                pasteHandler: function() { return gotoPosDialog.pastePosition(); }
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                text: "Paste"
                width: 78
                onClicked: {
                    if (gotoPosDialog.pastePosition())
                        xField2.focusEditor();
                }
            }
            DmeButton {
                text: "OK"
                width: 78
                onClicked: gotoPosDialog.go()
            }
            DmeButton {
                text: "Cancel"
                width: 78
                onClicked: gotoPosDialog.close()
            }
        }
    }
}
