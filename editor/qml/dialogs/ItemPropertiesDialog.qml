import QtQuick
import QtQuick.Controls
import "../style"

TibiaDialog {
    id: propsDialog
    required property var ctx
    property var mapCtrl: null

    title: "Item Properties"
    width: 340

    readonly property bool isTeleport: ctx.teleport === true
    readonly property bool isWritable: ctx.writable === true
    readonly property bool hasCreature: ctx.creatureName !== undefined && ctx.creatureName !== ""
    readonly property bool hasSpawn: ctx.spawnRadius !== undefined && ctx.spawnRadius > 0

    onOpened: resetFields()

    function resetFields() {
        countField.value = ctx.count > 0 ? ctx.count : 1;
        aidField.value = ctx.actionId > 0 ? ctx.actionId : 0;
        uidField.value = ctx.uniqueId > 0 ? ctx.uniqueId : 0;
        textField.text = ctx.text !== undefined ? ctx.text : "";
        teleX.value = ctx.teleportX > 0 ? ctx.teleportX : 0;
        teleY.value = ctx.teleportY > 0 ? ctx.teleportY : 0;
        teleZ.value = ctx.teleportZ > 0 ? ctx.teleportZ : 0;
        spawntimeField.value = ctx.creatureSpawntime > 0 ? ctx.creatureSpawntime : 60;
        radiusField.value = ctx.spawnRadius > 0 ? ctx.spawnRadius : 1;
    }

    function applyAndClose() {
        if (mapCtrl && ctx.hasItem === true) {
            var p = {
                "actionId": aidField.value,
                "uniqueId": uidField.value,
                "count": countField.value
            };
            if (isWritable)
                p["text"] = textField.text;
            if (isTeleport) {
                if (teleX.value === 0 && teleY.value === 0 && teleZ.value === 0)
                    p["teleportClear"] = true;
                else {
                    p["teleportX"] = teleX.value;
                    p["teleportY"] = teleY.value;
                    p["teleportZ"] = teleZ.value;
                }
            }
            mapCtrl.applyContextItemProperties(p);
        }

        if (mapCtrl && hasCreature)
            mapCtrl.setContextCreatureSpawntime(spawntimeField.value);
        if (mapCtrl && hasSpawn)
            mapCtrl.setContextSpawnRadius(radiusField.value);
        propsDialog.close();
    }

    contentItem: Item {
        implicitWidth: 316
        implicitHeight: body.implicitHeight + 8

        Column {
            id: body
            x: 4
            y: 4
            width: parent.width - 8
            spacing: 6

            Row {
                spacing: 8
                Text {
                    text: "ID " + propsDialog.ctx.serverId
                    color: "#c0c0c0"
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    text: propsDialog.ctx.name && propsDialog.ctx.name.length ? propsDialog.ctx.name : "(unnamed)"
                    color: "#999"
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                text: "Client Id " + propsDialog.ctx.clientId + "   -   " + propsDialog.ctx.x + ", " + propsDialog.ctx.y + ", " + propsDialog.ctx.z
                color: "#777"
                font.pixelSize: 10
            }

            Item {
                width: 1
                height: 4
            }

            Text {
                visible: propsDialog.ctx.hasItem === true
                text: "Count"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.ctx.hasItem === true
                TibiaSpinBox {
                    id: countField
                    width: 100
                    from: 1
                    to: 100
                    enabled: propsDialog.ctx.stackable === true
                    editable: propsDialog.ctx.stackable === true
                    opacity: enabled ? 1.0 : 0.45
                }
            }

            Text {
                visible: propsDialog.ctx.hasItem === true
                text: "Action ID / Unique ID"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                spacing: 6
                visible: propsDialog.ctx.hasItem === true
                TibiaSpinBox {
                    id: aidField
                    width: 145
                    from: 0
                    to: 65535
                }
                TibiaSpinBox {
                    id: uidField
                    width: 145
                    from: 0
                    to: 65535
                }
            }

            Text {
                visible: propsDialog.isTeleport
                text: "Destination"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                spacing: 6
                visible: propsDialog.isTeleport
                TibiaSpinBox {
                    id: teleX
                    width: 100
                    from: 0
                    to: 65535
                }
                TibiaSpinBox {
                    id: teleY
                    width: 100
                    from: 0
                    to: 65535
                }
                TibiaSpinBox {
                    id: teleZ
                    width: 84
                    from: 0
                    to: 15
                }
            }

            Text {
                visible: propsDialog.isWritable
                text: "Text"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.isWritable
                TibiaTextField {
                    id: textField
                    width: 296
                }
            }

            Text {
                visible: propsDialog.hasCreature
                text: "Spawntime (" + (propsDialog.ctx.creatureName || "") + ")"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.hasCreature
                TibiaSpinBox {
                    id: spawntimeField
                    width: 100
                    from: 1
                    to: 86400
                }
            }
            Text {
                visible: propsDialog.hasSpawn
                text: "Spawn radius"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.hasSpawn
                TibiaSpinBox {
                    id: radiusField
                    width: 100
                    from: 1
                    to: 15
                }
            }

            Item {
                width: 1
                height: 4
            }

            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                TibiaButton {
                    text: "OK"
                    width: 90
                    onClicked: propsDialog.applyAndClose()
                }
                TibiaButton {
                    text: "Cancel"
                    width: 90
                    onClicked: propsDialog.close()
                }
            }
        }
    }
}
