import Tibia 1.0
import QtQuick

Item {
    id: root
    property var flickable
    property bool dragging: false

    visible: root.flickable && root.flickable.contentHeight > 0 && root.flickable.contentHeight > root.flickable.height

    clip: true
    width: 12

    readonly property real minY: root.flickable ? root.flickable.originY : 0
    readonly property real maxY: root.flickable ? root.flickable.originY + root.flickable.contentHeight - root.flickable.height : 0

    BorderImage {
        id: track
        anchors {
            top: upArrow.bottom
            bottom: downArrow.top
            left: parent.left
            right: parent.right
        }
        source: (Backend.uiTheme.tex + "scrollbar_track.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
    }

    Image {
        id: upArrow
        anchors.top: parent.top
        width: 12
        height: 12
        smooth: false
        source: upArea.pressed ? (Backend.uiTheme.tex + "scrollbar_arrow_up_hover.png") : (Backend.uiTheme.tex + "scrollbar_arrow_up.png")
        MouseArea {
            id: upArea
            anchors.fill: parent
            onClicked: if (root.flickable)
                root.flickable.contentY = Math.max(root.minY, root.flickable.contentY - 32)
        }
    }

    Image {
        id: downArrow
        anchors.bottom: parent.bottom
        width: 12
        height: 12
        smooth: false
        source: downArea.pressed ? (Backend.uiTheme.tex + "scrollbar_arrow_down_hover.png") : (Backend.uiTheme.tex + "scrollbar_arrow_down.png")
        MouseArea {
            id: downArea
            anchors.fill: parent
            onClicked: if (root.flickable)
                root.flickable.contentY = Math.min(root.maxY, root.flickable.contentY + 32)
        }
    }

    readonly property real scrollPos: root.flickable && root.flickable.contentHeight > root.flickable.height ? Math.max(0, Math.min(1, (root.flickable.contentY - root.minY) / (root.flickable.contentHeight - root.flickable.height))) : 0

    BorderImage {
        id: thumb
        width: 12

        height: (root.flickable && root.flickable.contentHeight > 0) ? Math.min(track.height, Math.max(24, track.height * (root.flickable.height / root.flickable.contentHeight))) : 20
        source: (Backend.uiTheme.tex + "scrollbar_thumb.png")
        smooth: false
        border {
            left: 6
            right: 6
            top: 6
            bottom: 6
        }

        Binding {
            target: thumb
            property: "y"
            when: !root.dragging
            value: upArrow.height + root.scrollPos * (track.height - thumb.height)
        }

        MouseArea {
            anchors.fill: parent
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
