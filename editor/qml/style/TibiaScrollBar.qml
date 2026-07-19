import QtQuick

// Wlasny pionowy scrollbar w stylu classic Tibia UI (zastepuje natywny ScrollBar).
// Zrodlo: scrollbar.png (atlas 36x74), wartosci z 10-scrollbars.otui:
//   track: image-border=1, kciuk: image-border=6, strzalki 12x12 (idle/hover).
// Stany wykrojone offline (patrz editor/ui/scrollbar_*.png) - BorderImage nie
// wspiera wycinania podregionu z atlasu.
Item {
    id: root
    property var flickable
    property bool dragging: false
    // contentHeight bywa chwilowo 0 (np. przy przelaczeniu tilesetu model sie
    // czysci i wypelnia od nowa w kolejnej klatce) - bez > 0 dzielenie przez
    // 0 w thumb.height dawalo Infinity i sie "pierdolilo" (rozjezdzajacy sie,
    // rozciagniety kciuk/track wystajacy poza panel).
    visible: root.flickable && root.flickable.contentHeight > 0
             && root.flickable.contentHeight > root.flickable.height
    // Zabezpieczenie: gdyby mimo to cos policzylo sie do NaN/Infinity/ujemnej,
    // nic nie wyciecze poza obreb wlasnego paska (wczesniej brak clip
    // pozwalal takiemu artefaktowi renderowac sie az poza panel palety).
    clip: true
    width: 12

    // GridView/ListView maja originY, ktore po zmianie rozmiaru komorek (Icon Size)
    // przestaje byc zerem - KAZDE odwolanie do contentY musi byc wzgledem niego.
    // Bez tego thumb pokazywal gore mimo przewiniecia, a strzalki wypychaly widok
    // poza zawartosc (clamp do zlego zakresu).
    readonly property real minY: root.flickable ? root.flickable.originY : 0
    readonly property real maxY: root.flickable
        ? root.flickable.originY + root.flickable.contentHeight - root.flickable.height
        : 0

    BorderImage {
        id: track
        anchors { top: upArrow.bottom; bottom: downArrow.top; left: parent.left; right: parent.right }
        source: (uiTheme.tex + "scrollbar_track.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }

    Image {
        id: upArrow
        anchors.top: parent.top
        width: 12; height: 12
        smooth: false
        source: upArea.pressed ? (uiTheme.tex + "scrollbar_arrow_up_hover.png") : (uiTheme.tex + "scrollbar_arrow_up.png")
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
        width: 12; height: 12
        smooth: false
        source: downArea.pressed ? (uiTheme.tex + "scrollbar_arrow_down_hover.png") : (uiTheme.tex + "scrollbar_arrow_down.png")
        MouseArea {
            id: downArea
            anchors.fill: parent
            onClicked: if (root.flickable)
                root.flickable.contentY = Math.min(root.maxY, root.flickable.contentY + 32)
        }
    }

    readonly property real scrollPos:
        root.flickable && root.flickable.contentHeight > root.flickable.height
        ? Math.max(0, Math.min(1,
              (root.flickable.contentY - root.minY)
              / (root.flickable.contentHeight - root.flickable.height)))
        : 0

    BorderImage {
        id: thumb
        width: 12
        // Math.max(24, ...) samo nie wystarczy - gdy contentHeight == 0 dzielenie
        // daje Infinity, a Math.max(24, Infinity) to dalej Infinity (kciuk
        // rozciagniety w nieskonczonosc). Explicit guard na contentHeight > 0.
        height: (root.flickable && root.flickable.contentHeight > 0)
            ? Math.min(track.height, Math.max(24,
                  track.height * (root.flickable.height / root.flickable.contentHeight)))
            : 20
        source: (uiTheme.tex + "scrollbar_thumb.png")
        smooth: false
        border { left: 6; right: 6; top: 6; bottom: 6 }

        Binding {
            target: thumb; property: "y"; when: !root.dragging
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
                if (!root.flickable) return
                const ratio = (thumb.y - upArrow.height) / (track.height - thumb.height)
                root.flickable.contentY = root.minY
                    + ratio * (root.flickable.contentHeight - root.flickable.height)
            }
        }
    }
}
