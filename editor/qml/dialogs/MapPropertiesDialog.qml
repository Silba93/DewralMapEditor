import QtQuick
import QtQuick.Controls
import "../style"

// Map > Properties (Ctrl+P): wlasciwosci mapy (jak w RME).
TibiaDialog {
    id: mapPropsDialog
    // Okno glowne - wersja zaladowanego klienta (versionLabel/loadedClientVersion).
    required property var app

    title: "Map Properties"

    property var h: otbmReader.header()
    onAboutToShow: h = otbmReader.header()

    contentItem: Column {
        spacing: 10

        Grid {
            columns: 2; rowSpacing: 4; columnSpacing: 18
            Text { text: "Description:"; color: "#999"; font.pixelSize: 12 }
            Text {
                text: mapPropsDialog.h.description && mapPropsDialog.h.description.length
                      ? mapPropsDialog.h.description : "(none)"
                color: "#c0c0c0"; font.pixelSize: 12; width: 320; wrapMode: Text.WordWrap
            }
            Text { text: "OTBM version:"; color: "#999"; font.pixelSize: 12 }
            Text { text: "" + mapPropsDialog.h.otbmVersion; color: "#c0c0c0"; font.pixelSize: 12 }
            Text { text: "Client version:"; color: "#999"; font.pixelSize: 12 }
            Text {
                text: mapPropsDialog.app.loadedClientVersion > 0
                      ? mapPropsDialog.app.versionLabel(mapPropsDialog.app.loadedClientVersion) : "?"
                color: "#c0c0c0"; font.pixelSize: 12
            }
            Text { text: "Items (OTB):"; color: "#999"; font.pixelSize: 12 }
            Text {
                text: mapPropsDialog.h.otbItemsMajorVersion + "." + mapPropsDialog.h.otbItemsMinorVersion
                color: "#c0c0c0"; font.pixelSize: 12
            }
            Text { text: "Spawn file:"; color: "#999"; font.pixelSize: 12 }
            Text {
                text: mapPropsDialog.h.spawnFile && mapPropsDialog.h.spawnFile.length
                      ? mapPropsDialog.h.spawnFile : "(none)"
                color: "#c0c0c0"; font.pixelSize: 12
            }
            Text { text: "House file:"; color: "#999"; font.pixelSize: 12 }
            Text {
                text: mapPropsDialog.h.houseFile && mapPropsDialog.h.houseFile.length
                      ? mapPropsDialog.h.houseFile : "(none)"
                color: "#c0c0c0"; font.pixelSize: 12
            }
        }

        TibiaButton {
            text: "Zamknij"
            width: 90
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: mapPropsDialog.close()
        }
    }
}
