import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Item {
    id: workspace
    required property var app
    required property var settings
    required property var propertiesDialog
    required property int fps

    property alias mapView: mapView
    property alias mapGl: mapGl
    property alias context: mapArea.ctx

    TibiaPanel {
        anchors.fill: parent
    }

    Column {
        id: errorArea
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        spacing: 2
        topPadding: Backend.otbmReader.errorString ? 4 : 0

        Text {
            visible: Backend.sprReader.errorString.length > 0
            text: "SPR: " + Backend.sprReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.datReader.errorString.length > 0
            text: "DAT: " + Backend.datReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.otbReader.errorString.length > 0
            text: "OTB: " + Backend.otbReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.otbmReader.errorString.length > 0
            text: "OTBM: " + Backend.otbmReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }

    Item {
        id: mapArea
        anchors {
            top: errorArea.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 3
        }
        visible: Backend.otbmReader.loaded
        clip: true

        property var ctx: ({
                hasItem: false,
                serverId: 0,
                clientId: 0,
                name: "",
                groupName: "",
                x: 0,
                y: 0,
                z: 0,
                creatureName: "",
                creatureSpawntime: 0,
                spawnRadius: 0,
                actionId: 0,
                uniqueId: 0,
                text: "",
                writable: false,
                teleport: false,
                hasTeleportDest: false,
                teleportX: 0,
                teleportY: 0,
                teleportZ: 0
            })

        MapView {
            id: mapView
            anchors.fill: parent
            focus: true
            otbm: Backend.docMgr.current
            otb: Backend.otbReader
            dat: Backend.datReader
            spr: Backend.sprReader
            floor: 7
            Component.onCompleted: {
                setBrushStore(Backend.brushStore);
                setCreatureStore(Backend.creatureStore);
            }
            onContextMenuRequested: (x, y) => {
                mapArea.ctx = mapView.contextInfo();
                contextMenu.popup(x, y);
            }
        }

        MapGLView {
            id: mapGl
            anchors.fill: parent
            source: mapView
            Component.onCompleted: {
                if (!workspace.settings.glMaxFpsConfigured) {
                    workspace.settings.glMaxFps = 60;
                    workspace.settings.glMaxFpsConfigured = true;
                }
                maxFps = workspace.settings.glMaxFps;
            }
        }

        TibiaMenu {
            id: contextMenu
            Action {
                text: "Cut"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.cutSelection()
            }
            Action {
                text: "Copy"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.copySelection()
            }
            Action {
                text: "Copy Position"
                onTriggered: Backend.fileTools.setClipboard(mapArea.ctx.x + ", " + mapArea.ctx.y + ", " + mapArea.ctx.z)
            }
            Action {
                text: "Paste"
                enabled: mapView.hasClipboard
                onTriggered: mapView.startPasting()
            }
            Action {
                text: "Delete"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.deleteSelectedTop()
            }
            MenuSeparator {}
            Action {
                text: "Copy Item Server Id"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard("" + mapArea.ctx.serverId)
            }
            Action {
                text: "Copy Item Client Id"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard("" + mapArea.ctx.clientId)
            }
            Action {
                text: "Copy Item Name"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard(mapArea.ctx.name)
            }
            MenuSeparator {}
            TibiaMenuItem {
                text: "Select Brush"
                visible: mapArea.ctx.hasItem && mapView.brushForServerId(mapArea.ctx.serverId) !== ""
                height: visible ? implicitHeight : 0
                onTriggered: mapView.useGroundBrush(mapArea.ctx.serverId)
            }
            Action {
                text: "Select RAW"
                enabled: mapArea.ctx.hasItem
                onTriggered: mapView.brushServerId = mapArea.ctx.serverId
            }
            TibiaMenuItem {
                text: "Go To Destination"
                visible: mapArea.ctx.teleport === true && mapArea.ctx.hasTeleportDest === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.centerOnPosition(mapArea.ctx.teleportX, mapArea.ctx.teleportY, mapArea.ctx.teleportZ)
            }
            Action {
                text: "Properties"
                enabled: mapArea.ctx.hasItem || mapArea.ctx.creatureName !== "" || mapArea.ctx.spawnRadius > 0
                onTriggered: workspace.propertiesDialog.open()
            }
        }

        Item {
            visible: mapView.minimapOn
            width: 236
            height: 262
            anchors {
                right: parent.right
                top: parent.top
                margins: 10
            }

            TibiaPanel {
                anchors.fill: parent
            }
            Text {
                id: minimapTitle
                anchors {
                    left: parent.left
                    top: parent.top
                    leftMargin: 8
                    topMargin: 5
                }
                text: "Minimap  -  floor " + mapView.floor
                color: "#ddd"
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                anchors {
                    right: parent.right
                    top: parent.top
                    rightMargin: 8
                    topMargin: 4
                }
                text: "x"
                color: closeMinimapArea.containsMouse ? "#fff" : "#999"
                font.pixelSize: 13
                font.bold: true
                MouseArea {
                    id: closeMinimapArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    onClicked: mapView.minimapOn = false
                }
            }
            MinimapView {
                source: mapView
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    top: minimapTitle.bottom
                    margins: 6
                    topMargin: 4
                }
            }
        }

        Rectangle {
            anchors {
                left: parent.left
                top: parent.top
                margins: 6
            }
            width: fpsLabel.implicitWidth + 12
            height: 20
            radius: 4
            color: "#B0000000"
            Text {
                id: fpsLabel
                anchors.centerIn: parent
                text: "FPS: " + workspace.fps + "   OpenGL"
                color: workspace.fps >= 50 ? "#7fdc8f" : (workspace.fps >= 25 ? "#e0c46a" : "#e08a6a")
                font.pixelSize: 11
                font.bold: true
            }
        }

        Rectangle {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                topMargin: 8
            }
            visible: workspace.app.savedToast.length > 0
            width: toastLabel.implicitWidth + 20
            height: 26
            radius: 5
            color: "#E622432f"
            Text {
                id: toastLabel
                anchors.centerIn: parent
                text: workspace.app.savedToast
                color: "#eaffea"
                font.pixelSize: 12
            }
        }

        Rectangle {
            anchors {
                left: parent.left
                bottom: parent.bottom
                margins: 8
            }
            visible: mapView.hoverText.length > 0
            width: hoverLabel.implicitWidth + 16
            height: 22
            radius: 4
            color: "#B0000000"
            Text {
                id: hoverLabel
                anchors.centerIn: parent
                text: mapView.hoverText
                color: "#ddd"
                font.pixelSize: 11
            }
        }
    }
}
