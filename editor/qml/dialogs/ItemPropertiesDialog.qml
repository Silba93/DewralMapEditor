import QtQuick
import QtQuick.Controls
import "../style"

// Wlasciwosci itemu (PPM na mapie > Properties).
TibiaDialog {
    id: propsDialog
    // Dane kliknietego kafelka/itemu: { name, serverId, clientId, x, y, z, ... }.
    required property var ctx
    // Kontroler mapy - potrzebny do zapisu count. Bez niego dialog jest tylko do odczytu.
    property var mapCtrl: null

    title: "Item properties"

    contentItem: Column {
        spacing: 4
        Text { text: "Name: " + (propsDialog.ctx.name && propsDialog.ctx.name.length ? propsDialog.ctx.name : "(unnamed)"); color: "#c0c0c0"; font.pixelSize: 13; font.bold: true }
        Text { text: "Server Id: " + propsDialog.ctx.serverId; color: "#999"; font.pixelSize: 12 }
        Text { text: "Client Id: " + propsDialog.ctx.clientId; color: "#999"; font.pixelSize: 12 }
        Text { text: "Position: " + propsDialog.ctx.x + ", " + propsDialog.ctx.y + ", " + propsDialog.ctx.z; color: "#999"; font.pixelSize: 12 }
        // Count tylko dla stackowalnych - dla reszty OTBM trzyma 1 i wartosc nic nie znaczy.
        // Zmiana od razu przebudowuje sprite (count wybiera wariant sterty) i idzie na undo.
        Row {
            spacing: 6
            visible: propsDialog.ctx.stackable === true
            Text {
                text: "Count"
                color: "#ccc"; font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                id: countField
                width: 80
                from: 1
                to: 100      // limit sterty w Tibii; wyzej i tak nie ma wariantu sprite'a
                value: propsDialog.ctx.count > 0 ? propsDialog.ctx.count : 1
                onValueModified: if (propsDialog.mapCtrl) propsDialog.mapCtrl.setContextItemCount(value)
            }
        }
        Text {
            visible: propsDialog.ctx.actionId > 0
            text: "Action Id: " + propsDialog.ctx.actionId
            color: "#999"; font.pixelSize: 12
        }
        Text {
            visible: propsDialog.ctx.uniqueId > 0
            text: "Unique Id: " + propsDialog.ctx.uniqueId
            color: "#999"; font.pixelSize: 12
        }

        // Potwor na kaflu: nazwa + edytowalny spawntime (jak RME).
        Text {
            visible: propsDialog.ctx.creatureName !== undefined && propsDialog.ctx.creatureName !== ""
            text: "Creature: " + propsDialog.ctx.creatureName
            color: "#c0c0c0"; font.pixelSize: 12; font.bold: true
        }
        Row {
            spacing: 6
            visible: propsDialog.ctx.creatureName !== undefined && propsDialog.ctx.creatureName !== ""
            Text {
                text: "Spawntime"
                color: "#999"; font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                width: 80; from: 1; to: 86400
                value: propsDialog.ctx.creatureSpawntime > 0 ? propsDialog.ctx.creatureSpawntime : 60
                onValueModified: if (propsDialog.mapCtrl) propsDialog.mapCtrl.setContextCreatureSpawntime(value)
            }
        }
        // Centrum spawnu: promien edytowalny (jak spawntime).
        Row {
            spacing: 6
            visible: propsDialog.ctx.spawnRadius !== undefined && propsDialog.ctx.spawnRadius > 0
            Text {
                text: "Spawn radius"
                color: "#999"; font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaSpinBox {
                width: 80; from: 1; to: 15
                value: propsDialog.ctx.spawnRadius > 0 ? propsDialog.ctx.spawnRadius : 1
                onValueModified: if (propsDialog.mapCtrl) propsDialog.mapCtrl.setContextSpawnRadius(value)
            }
        }

        TibiaButton {
            text: "Zamknij"
            width: 90
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: propsDialog.close()
        }
    }
}
