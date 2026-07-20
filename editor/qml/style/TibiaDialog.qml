import QtQuick
import QtQuick.Controls

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
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
            }
            text: root.title
            color: "#c0c0c0"
            font.bold: true
            font.pixelSize: 13
        }
    }
}
