import Tibia 1.0
import QtQuick

Item {
    id: root
    property var flickable
    property bool dragging: false
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    visible: root.flickable && root.flickable.contentHeight > 0 && root.flickable.contentHeight > root.flickable.height

    clip: true
    width: 12

    readonly property real minY: root.flickable ? root.flickable.originY : 0
    readonly property real maxY: root.flickable ? root.flickable.originY + root.flickable.contentHeight - root.flickable.height : 0

    Item {
        id: track
        anchors {
            top: upArrow.bottom
            bottom: downArrow.top
            left: parent.left
            right: parent.right
        }

        BorderImage {
            anchors.fill: parent
            visible: !root.githubUi
            source: (Backend.uiTheme.tex + "scrollbar_track.png")
            smooth: false
            border {
                left: 1
                right: 1
                top: 1
                bottom: 1
            }
        }

        Rectangle {
            anchors {
                fill: parent
                leftMargin: 4
                rightMargin: 4
            }
            visible: root.githubUi
            radius: 2
            color: "#161B22"
        }
    }

    Item {
        id: upArrow
        anchors.top: parent.top
        width: 12
        height: 12
        Image {
            anchors.fill: parent
            visible: !root.githubUi
            smooth: false
            source: upArea.pressed ? (Backend.uiTheme.tex + "scrollbar_arrow_up_hover.png") : (Backend.uiTheme.tex + "scrollbar_arrow_up.png")
        }
        Text {
            anchors.centerIn: parent
            visible: root.githubUi
            text: "\u2303"
            color: upArea.containsMouse ? "#C9D1D9" : "#6E7681"
            font.pixelSize: 9
        }
        MouseArea {
            id: upArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (root.flickable)
                root.flickable.contentY = Math.max(root.minY, root.flickable.contentY - 32)
        }
    }

    Item {
        id: downArrow
        anchors.bottom: parent.bottom
        width: 12
        height: 12
        Image {
            anchors.fill: parent
            visible: !root.githubUi
            smooth: false
            source: downArea.pressed ? (Backend.uiTheme.tex + "scrollbar_arrow_down_hover.png") : (Backend.uiTheme.tex + "scrollbar_arrow_down.png")
        }
        Text {
            anchors.centerIn: parent
            visible: root.githubUi
            text: "\u2304"
            color: downArea.containsMouse ? "#C9D1D9" : "#6E7681"
            font.pixelSize: 9
        }
        MouseArea {
            id: downArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (root.flickable)
                root.flickable.contentY = Math.min(root.maxY, root.flickable.contentY + 32)
        }
    }

    readonly property real scrollPos: root.flickable && root.flickable.contentHeight > root.flickable.height ? Math.max(0, Math.min(1, (root.flickable.contentY - root.minY) / (root.flickable.contentHeight - root.flickable.height))) : 0

    Item {
        id: thumb
        width: 12

        height: (root.flickable && root.flickable.contentHeight > 0) ? Math.min(track.height, Math.max(24, track.height * (root.flickable.height / root.flickable.contentHeight))) : 20
        BorderImage {
            anchors.fill: parent
            visible: !root.githubUi
            source: (Backend.uiTheme.tex + "scrollbar_thumb.png")
            smooth: false
            border {
                left: 6
                right: 6
                top: 6
                bottom: 6
            }
        }

        Rectangle {
            anchors {
                fill: parent
                leftMargin: 3
                rightMargin: 3
            }
            visible: root.githubUi
            radius: 3
            color: thumbArea.containsMouse || root.dragging ? "#8B949E" : "#57606A"
        }

        Binding {
            target: thumb
            property: "y"
            when: !root.dragging
            value: upArrow.height + root.scrollPos * (track.height - thumb.height)
        }

        MouseArea {
            id: thumbArea
            anchors.fill: parent
            hoverEnabled: true
            drag.target: thumb
            drag.axis: Drag.YAxis
            drag.minimumY: upArrow.height
            drag.maximumY: upArrow.height + track.height - thumb.height
            onPressed: root.dragging = true
            onReleased: root.dragging = false
            onPositionChanged: {
                if (!root.flickable)
                    return;
                const ratio = (thumb.y - upArrow.height) / (track.height - thumb.height);
                root.flickable.contentY = root.minY + ratio * (root.flickable.contentHeight - root.flickable.height);
            }
        }
    }
}
