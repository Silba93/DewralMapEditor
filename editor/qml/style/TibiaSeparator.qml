import QtQuick

// Separator (linia) w stylu classic Tibia UI. Zrodlo: separator_horizontal.png /
// separator_vertical.png, border=1 (10-separators.otui).
//
// Opakowane w Item, bo implicitWidth/implicitHeight sa READ-ONLY na BorderImage
// (QQuickImplicitSizeItem wyprowadza je z rozmiaru obrazka). Item ma je zapisywalne.
Item {
    id: root
    property bool vertical: false

    implicitWidth: vertical ? 2 : 100
    implicitHeight: vertical ? 100 : 2

    BorderImage {
        anchors.fill: parent
        source: root.vertical ? (uiTheme.tex + "separator_vertical.png")
                              : (uiTheme.tex + "separator_horizontal.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }
}
