import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var model: []
    property int currentIndex: -1
    readonly property string currentText: (currentIndex >= 0 && currentIndex < model.length) ? model[currentIndex] : ""
    signal activated(int index)

    implicitWidth: 140
    implicitHeight: 23

    readonly property bool open: popup.visible

    Rectangle {
        anchors.fill: parent
        color: "#2b2b2b"
        border.width: 1
        border.color: root.open || mouseArea.containsMouse ? "#4a90e2" : "#555"
    }

    Text {
        anchors {
            left: parent.left
            leftMargin: 6
            right: arrow.left
            verticalCenter: parent.verticalCenter
        }
        text: root.currentText
        color: "#e8e8e8"
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    Canvas {
        id: arrow
        width: 10
        height: 10
        anchors {
            right: parent.right
            rightMargin: 6
            verticalCenter: parent.verticalCenter
        }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.fillStyle = "#c0c0c0";
            ctx.beginPath();
            ctx.moveTo(0, 3);
            ctx.lineTo(10, 3);
            ctx.lineTo(5, 9);
            ctx.closePath();
            ctx.fill();
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: popup.visible = !popup.visible
    }

    Popup {
        id: popup
        y: root.height
        width: root.width
        height: Math.min(200, listView.contentHeight + 2)
        padding: 1
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

        modal: true
        dim: false

        background: Rectangle {
            color: "#2b2b2b"
            border.width: 1
            border.color: "#555"
        }

        contentItem: ListView {
            id: listView
            implicitHeight: contentHeight
            model: root.model
            clip: true
            delegate: Rectangle {
                width: listView.width
                height: 22
                color: entryArea.containsMouse ? "#20ffffff" : "transparent"
                Text {
                    anchors {
                        left: parent.left
                        leftMargin: 6
                        verticalCenter: parent.verticalCenter
                    }
                    text: modelData
                    color: "#e8e8e8"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: entryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentIndex = index;
                        root.activated(index);
                        popup.visible = false;
                    }
                }
            }
        }
    }
}
