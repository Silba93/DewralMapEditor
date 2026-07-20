import Tibia 1.0
import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Controls
import "style"

Window {
    id: startupScreen

    required property var app

    required property var settings

    function openMapDialog() {
        startMapDialog.open();
    }
    function openVersionFolderDialog() {
        versionFolderDialogStartup.open();
    }

    transientParent: null
    visible: !app.started
    width: 800
    height: 520
    minimumWidth: 800
    maximumWidth: 800
    minimumHeight: 520
    maximumHeight: 520
    x: Screen.width / 2 - width / 2
    y: Screen.height / 2 - height / 2
    title: "Dewral Map Editor"

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    TibiaDialogBackground {
        id: card
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 760)
        height: Math.min(parent.height - 80, 440)

        Item {
            id: titleBar
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: 27

            Text {
                anchors.centerIn: parent
                text: startupScreen.title
                color: "#c0c0c0"
                font.bold: true
                font.pixelSize: 13
            }

            Text {
                anchors {
                    right: parent.right
                    rightMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                text: "X"
                color: closeArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13
                font.bold: true
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.quit()
                }
            }

            MouseArea {
                anchors.fill: parent
                anchors.rightMargin: 20
                onPressed: startupScreen.startSystemMove()
            }
        }

        Row {
            anchors.fill: parent
            anchors.topMargin: titleBar.height + 8
            anchors.margins: 8
            spacing: 8

            TibiaPanel {
                width: 280
                height: parent.height
                Column {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14

                    Column {
                        spacing: 4
                        Text {
                            text: "Dewral Map Editor"
                            color: "#f0f0f0"
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Rectangle {
                            width: 180
                            height: 2
                            color: "#4a90e2"
                        }
                    }
                    Text {
                        text: "Tibia 7.72 - 10.98+ | OpenGL"
                        color: "#888"
                        font.pixelSize: 12
                    }

                    Item {
                        width: 1
                        height: 8
                    }

                    Rectangle {
                        width: 232
                        height: 40
                        radius: 3
                        color: omAcc.pressed ? "#1f5f3f" : (omAcc.containsMouse ? "#36805a" : "#2f6f4f")
                        border.color: "#7fdc8f"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "Open map..."
                            color: "#eaffea"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        MouseArea {
                            id: omAcc
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: startMapDialog.open()
                        }
                    }

                    Text {
                        text: "Client versions"
                        color: "#ddd"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Row {
                        spacing: 6
                        TibiaComboBox {
                            id: verCombo
                            width: 150
                            height: 23

                            model: app.allProfileKeys().map(function (k) {
                                return app.profileLabel(k);
                            })
                            currentIndex: 0
                            readonly property string selKey: {
                                var keys = app.allProfileKeys();
                                return currentIndex >= 0 && currentIndex < keys.length ? keys[currentIndex] : "772";
                            }
                        }
                        TibiaButton {
                            width: 76
                            height: 23
                            text: "Folder..."
                            onClicked: {
                                app.pendingKey = verCombo.selKey;
                                app.pendingMapPath = "";
                                versionFolderDialogStartup.open();
                            }
                        }
                    }

                    Row {
                        spacing: 6
                        TibiaTextField {
                            id: newProfileField
                            width: 150
                            height: 23
                            placeholderText: "e.g. Midhem"
                        }
                        TibiaButton {
                            width: 76
                            height: 23
                            text: "+ Custom"
                            onClicked: {
                                var base = app.profileVer(verCombo.selKey);
                                var name = newProfileField.text.trim();
                                if (app.addCustomProfile(name, base)) {
                                    newProfileField.text = "";

                                    verCombo.currentIndex = app.allProfileKeys().indexOf(name);
                                }
                            }
                        }
                    }

                    TibiaButton {
                        visible: app.isCustomKey(verCombo.selKey)
                        width: 232
                        height: 21
                        text: "Remove profile " + verCombo.selKey
                        onClicked: {
                            app.removeCustomProfile(verCombo.selKey);
                            verCombo.currentIndex = 0;
                        }
                    }

                    Column {
                        spacing: 2
                        property string selFolder: app.clientPaths[verCombo.selKey] || ""
                        property var selFiles: app.clientFiles(selFolder)
                        Text {
                            width: 232
                            elide: Text.ElideMiddle
                            text: parent.selFolder !== "" ? parent.selFolder : "(folder not set for this version)"
                            color: "#999"
                            font.pixelSize: 10
                        }
                        Text {
                            font.pixelSize: 11
                            visible: parent.selFolder !== ""
                            property bool ok: parent.selFiles.dat && parent.selFiles.spr && parent.selFiles.otb
                            color: ok ? "#7fdc8f" : "#e08a6a"
                            text: ok ? "OK: dat / spr / otb found" : "Missing: " + (parent.selFiles.dat ? "" : "dat ") + (parent.selFiles.spr ? "" : "spr ") + (parent.selFiles.otb ? "" : "otb ") + "missing"
                        }

                        Text {
                            width: 232
                            wrapMode: Text.WordWrap
                            text: {
                                var keys = Object.keys(app.clientPaths).sort(function (a, b) {
                                    var na = Number(a), nb = Number(b);
                                    var ca = isNaN(na), cb = isNaN(nb);
                                    if (ca !== cb)
                                        return ca ? 1 : -1;
                                    return ca ? a.localeCompare(b) : na - nb;
                                });
                                if (keys.length === 0)
                                    return "No versions configured yet.";
                                return "Configured: " + keys.map(function (k) {
                                    return app.profileLabel(k);
                                }).join(", ");
                            }
                            color: "#7a9a7a"
                            font.pixelSize: 10
                        }
                    }
                }
            }

            TibiaPanel {
                width: parent.width - 288
                height: parent.height

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 8

                    Text {
                        text: "Recent maps"
                        color: "#ddd"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        visible: app.recentMaps.length === 0
                        text: "No recent maps yet.\nUse Open map... to load one."
                        color: "#777"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        id: recentList
                        width: parent.width
                        height: parent.height - 30
                        clip: true
                        model: app.recentMaps
                        spacing: 4

                        delegate: Item {
                            width: recentList.width
                            height: 44

                            Rectangle {
                                anchors.fill: parent
                                color: rma.pressed ? "#14ffffff" : (rma.containsMouse ? "#0affffff" : "transparent")
                                border.color: "#555"
                                border.width: 1
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 1
                                Text {
                                    text: Backend.fileTools.fileName(modelData)
                                    color: "#eee"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: modelData
                                    color: "#888"
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                    width: parent.width
                                }
                            }
                            MouseArea {
                                id: rma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!Backend.fileTools.exists(modelData))
                                        return;
                                    app.loadEverything(modelData, verCombo.selKey);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: versionFolderDialogStartup
        title: "Select client folder for " + app.profileLabel(app.pendingKey) + " (Tibia.dat / Tibia.spr / items.otb)"
        onAccepted: app.onVersionFolderPicked(selectedFolder)
    }

    FileDialog {
        id: startMapDialog
        title: "Open .otbm map"
        nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
        onAccepted: app.loadEverything(Backend.fileTools.toLocalFile(selectedFile), verCombo.selKey)
    }
}
