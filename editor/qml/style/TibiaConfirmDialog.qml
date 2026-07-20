import QtQuick
import QtQuick.Controls

TibiaDialog {
    id: root
    property string message: ""

    width: 340

    contentItem: Column {
        spacing: 12

        Text {
            width: root.width - 24
            text: root.message
            color: "#c0c0c0"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            TibiaButton {
                text: "Yes"
                width: 90
                onClicked: {
                    root.accepted();
                    root.close();
                }
            }
            TibiaButton {
                text: "Cancel"
                width: 90
                onClicked: root.close()
            }
        }
    }
}
