import QtQuick
import QtQuick.Controls
import "../style"

// Map > Go To Position (Ctrl+G): przeskok kamery na X,Y,Z (jak w RME).
TibiaDialog {
    id: gotoPosDialog
    // MapView - biezaca pozycja (wartosci startowe pol) + centerOnTile.
    required property var mapCtrl

    title: "Go To Position"

    function go() {
        mapCtrl.centerOnTile(xField2.value, yField2.value, zField2.value)
        gotoPosDialog.close()
    }

    onOpened: xField2.forceActiveFocus()

    contentItem: Column {
        spacing: 8
        // Enter w ktorymkolwiek polu = od razu skacz (bez klikania OK).
        Keys.onReturnPressed: gotoPosDialog.go()
        Keys.onEnterPressed: gotoPosDialog.go()

        Row {
            spacing: 6
            TibiaSpinBox {
                id: xField2; width: 78; from: 0; to: 65535
                value: Math.round(gotoPosDialog.mapCtrl.glOriginX())
            }
            TibiaSpinBox {
                id: yField2; width: 78; from: 0; to: 65535
                value: Math.round(gotoPosDialog.mapCtrl.glOriginY())
            }
            TibiaSpinBox {
                id: zField2; width: 62; from: 0; to: 15
                value: gotoPosDialog.mapCtrl.floor
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            TibiaButton { text: "OK"; width: 90; onClicked: gotoPosDialog.go() }
            TibiaButton { text: "Anuluj"; width: 90; onClicked: gotoPosDialog.close() }
        }
    }
}
