import QtQuick
import QtQuick.Controls
import "../style"

// Map > Statistics (F8): statystyki mapy (jak w RME).
TibiaDialog {
    id: statsDialog
    title: "Map Statistics"

    property var h: otbmReader.header()
    onAboutToShow: h = otbmReader.header()   // odswiez przy otwarciu

    contentItem: Column {
        spacing: 10

        Grid {
            columns: 2; rowSpacing: 4; columnSpacing: 18
            Text { text: "Dimensions:"; color: "#999"; font.pixelSize: 12 }
            Text { text: statsDialog.h.width + " x " + statsDialog.h.height; color: "#c0c0c0"; font.pixelSize: 12 }
            Text { text: "Tiles:"; color: "#999"; font.pixelSize: 12 }
            Text { text: "" + statsDialog.h.tileCount; color: "#c0c0c0"; font.pixelSize: 12 }
            Text { text: "Items:"; color: "#999"; font.pixelSize: 12 }
            Text { text: "" + statsDialog.h.itemCount; color: "#c0c0c0"; font.pixelSize: 12 }
            Text { text: "Towns:"; color: "#999"; font.pixelSize: 12 }
            Text { text: "" + statsDialog.h.townCount; color: "#c0c0c0"; font.pixelSize: 12 }
            Text { text: "Waypoints:"; color: "#999"; font.pixelSize: 12 }
            Text { text: "" + statsDialog.h.waypointCount; color: "#c0c0c0"; font.pixelSize: 12 }
        }

        TibiaButton {
            text: "Zamknij"
            width: 90
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: statsDialog.close()
        }
    }
}
