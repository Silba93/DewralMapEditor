import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

// Tools > Brush Editor - wizualne skladanie GROUND BRUSHY (grunt + 12 kafli
// bordera) i WALL BRUSHY (17 slotow wg maski polaczen N/W/E/S), przeciaganiem
// itemow z wbudowanej palety. Zapis od razu do data/<profil>/brushes.json
// (BrushStore.saveGroundBrush/saveWallBrush - patrz brushstore.h).
//
// DnD: jeden wspolny "duch" (dragGhost) + DropArea na slotach. Chwyc item w
// palecie po lewej i upusc na slot. PPM na slocie czysci go.
TibiaDialog {
    id: root
    property var mapCtrl: null

    title: "Brush Editor"

    property string tab: "ground"          // "ground" | "wall"
    property string curGround: ""          // edytowany ground brush ("" = nowy)
    property string curWall: ""
    // Bufory edycji (zapis dopiero na "Zapisz").
    // Bordery per CEL: { "": [13], "*": [13], "water": [13] }. Kluczem jest "to" -
    // "" = przeciw pustce, "*" = dowolny inny brush, nazwa = konkretny (np. woda).
    property var borderSets: ({ "": [0,0,0,0,0,0,0,0,0,0,0,0,0] })
    property string borderTarget: ""       // aktualnie edytowany cel (klucz borderSets)
    // Wygodny alias: zestaw 13 kafli aktualnego celu.
    readonly property var borderIds: borderSets[borderTarget] || [0,0,0,0,0,0,0,0,0,0,0,0,0]
    property var wallIds: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]

    // Ustaw kafel bordera w AKTUALNYM celu (kopia obiektu -> binding sie odswieza).
    function setBorderTile(bt, sid) {
        var sets = JSON.parse(JSON.stringify(borderSets))
        if (!sets[borderTarget]) sets[borderTarget] = [0,0,0,0,0,0,0,0,0,0,0,0,0]
        sets[borderTarget][bt] = sid
        borderSets = sets
    }
    // Cele dostepne w combo: pustka, dowolny, + inne ground brushe (nie ten edytowany).
    function borderTargetKeys() {
        var keys = ["", "*"]
        var names = brushStore.groundBrushNames()
        for (var i = 0; i < names.length; ++i)
            if (names[i] !== groundNameField.text.trim()) keys.push(names[i])
        return keys
    }
    function borderTargetLabel(key) {
        if (key === "") return "Pustka (brak sasiada)"
        if (key === "*") return "Dowolny inny brush"
        return key
    }

    function iconSrc(id) {
        if (id <= 0) return ""
        var row = otbReader.rowForServerId(id)
        if (row < 0) return ""
        var d = otbReader.detailsAt(row)
        return sprReader.itemImageSource(d.spriteIds, d.itemWidth, d.itemHeight, d.layers)
    }

    function loadGround(name) {
        curGround = name
        var d = brushStore.groundBrushEdit(name)
        zorderField.value = d.zorder
        gItems.clear()
        for (var i = 0; i < d.items.length; ++i)
            gItems.append({ sid: d.items[i].id, chance: d.items[i].chance })
        // Bordery per cel: zbuduj mape { to: [13] }. Zawsze zapewnij klucz "".
        var sets = { "": [0,0,0,0,0,0,0,0,0,0,0,0,0] }
        for (var b = 0; b < d.borders.length; ++b)
            sets[d.borders[b].to] = d.borders[b].tiles.slice()
        borderSets = sets
        borderTarget = ""
        groundNameField.text = name
        targetCombo.syncFromApp()
    }
    function newGround() {
        curGround = ""
        groundNameField.text = ""
        zorderField.value = 3500
        gItems.clear()
        borderSets = ({ "": [0,0,0,0,0,0,0,0,0,0,0,0,0] })
        borderTarget = ""
        targetCombo.syncFromApp()
    }
    function saveGround() {
        var name = groundNameField.text.trim()
        if (name === "" || gItems.count === 0) return
        var items = []
        for (var i = 0; i < gItems.count; ++i)
            items.push({ id: gItems.get(i).sid, chance: gItems.get(i).chance })
        // Kazdy cel z bufora -> osobny blok (pusty pomija sam BrushStore).
        var blocks = []
        for (var key in borderSets)
            blocks.push({ to: key, tiles: borderSets[key] })
        if (brushStore.saveGroundBrush(name, zorderField.value, items, blocks))
            loadGround(name)
    }

    function loadWall(name) {
        curWall = name
        wallIds = brushStore.wallBrushEdit(name)
        wallNameField.text = name
    }
    function newWall() {
        curWall = ""
        wallNameField.text = ""
        wallIds = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
    }
    function saveWall() {
        var name = wallNameField.text.trim()
        if (name === "") return
        if (brushStore.saveWallBrush(name, wallIds)) loadWall(name)
    }

    // Zmiana pliku brushy (edycja/zmiana profilu) -> odswiez comba.
    Connections {
        target: brushStore
        function onBrushesChanged() {
            groundCombo.model = brushStore.groundBrushNames()
            wallCombo.model = brushStore.wallBrushNames()
        }
    }
    onOpened: {
        groundCombo.model = brushStore.groundBrushNames()
        wallCombo.model = brushStore.wallBrushNames()
        newGround()
        newWall()
    }

    // Filtr wbudowanej palety (wszystkie itemy + szukajka).
    PaletteFilter {
        id: pf
        sourceModel: otbReader
        mode: "all"
    }

    contentItem: Item {
        id: body
        implicitWidth: 660
        implicitHeight: 480

        // ===== LEWO: paleta itemow (zrodlo przeciagania) =====
        Column {
            id: pickerCol
            width: 216
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            spacing: 6

            TibiaTextField {
                width: parent.width
                placeholderText: "Szukaj (nazwa lub id)..."
                onTextChanged: pf.searchText = text
            }

            TibiaPanel {
                width: parent.width
                height: pickerCol.height - 30

                GridView {
                    id: pickerGrid
                    anchors.fill: parent
                    anchors.margins: 3
                    clip: true
                    cellWidth: 42; cellHeight: 42
                    model: pf

                    delegate: Rectangle {
                        width: 40; height: 40
                        color: cellMa.containsMouse ? "#303030" : "#252525"
                        border.color: "#3a3a3a"; border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: 32; height: 32
                            fillMode: Image.PreserveAspectFit
                            smooth: false; cache: false
                            source: (typeof spriteIds !== "undefined" && spriteIds.length > 0)
                                    ? sprReader.itemImageSource(spriteIds,
                                          typeof itemWidth !== "undefined" ? itemWidth : 1,
                                          typeof itemHeight !== "undefined" ? itemHeight : 1,
                                          typeof layers !== "undefined" ? layers : 1)
                                    : ""
                        }
                        Text {
                            anchors { right: parent.right; bottom: parent.bottom; margins: 1 }
                            text: typeof serverId !== "undefined" ? serverId : ""
                            color: "#888"; font.pixelSize: 8
                        }

                        MouseArea {
                            id: cellMa
                            anchors.fill: parent
                            hoverEnabled: true
                            drag.target: dragGhost
                            onPressed: (mouse) => {
                                dragGhost.sid = (typeof serverId !== "undefined") ? serverId : 0
                                dragGhost.source = parent.children[0].source
                                var p = mapToItem(body, mouse.x, mouse.y)
                                dragGhost.x = p.x - 16
                                dragGhost.y = p.y - 16
                            }
                            drag.onActiveChanged: dragGhost.visible = drag.active
                            onReleased: {
                                if (dragGhost.visible) dragGhost.Drag.drop()
                                dragGhost.visible = false
                            }
                        }
                    }
                }
                TibiaScrollBar {
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    anchors.margins: 2
                    flickable: pickerGrid
                }
            }
        }

        // ===== PRAWO: edytor =====
        Column {
            anchors { left: pickerCol.right; right: parent.right; top: parent.top }
            anchors.leftMargin: 10
            spacing: 8

            // Zakladki Ground / Wall.
            Row {
                spacing: 6
                TibiaButton {
                    text: "Ground brush"; width: 110
                    opacity: root.tab === "ground" ? 1.0 : 0.55
                    onClicked: root.tab = "ground"
                }
                TibiaButton {
                    text: "Wall brush"; width: 110
                    opacity: root.tab === "wall" ? 1.0 : 0.55
                    onClicked: root.tab = "wall"
                }
            }

            // ---------- GROUND ----------
            Column {
                visible: root.tab === "ground"
                spacing: 6
                width: parent.width

                Row {
                    spacing: 6
                    TibiaComboBox {
                        id: groundCombo
                        width: 150; height: 23
                        onActivated: root.loadGround(model[currentIndex])
                    }
                    TibiaButton { text: "Nowy"; width: 60; onClicked: root.newGround() }
                    TibiaButton {
                        text: "Usun"; width: 60
                        enabled: root.curGround !== ""
                        onClicked: { brushStore.deleteGroundBrush(root.curGround); root.newGround() }
                    }
                }
                Row {
                    spacing: 6
                    Text { text: "Nazwa"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                    TibiaTextField { id: groundNameField; width: 150; height: 22 }
                    Text { text: "Z-order"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                    TibiaSpinBox { id: zorderField; width: 80; from: 0; to: 65535; value: 3500 }
                }

                // Grunty (warianty losowane wagami) - upusc item tutaj.
                Text { text: "Grunty (upusc item; waga = szansa)"; color: "#999"; font.pixelSize: 11 }
                Rectangle {
                    width: parent.width; height: 66
                    color: gDrop.containsDrag ? "#2f4f3f" : "#252525"
                    border.color: gDrop.containsDrag ? "#7fdc8f" : "#3a3a3a"; border.width: 1

                    DropArea {
                        id: gDrop
                        anchors.fill: parent
                        onDropped: (drop) => {
                            var sid = drop.source.sid
                            if (sid > 0) gItems.append({ sid: sid, chance: 10 })
                        }
                    }
                    ListView {
                        anchors.fill: parent
                        anchors.margins: 4
                        orientation: ListView.Horizontal
                        spacing: 4
                        clip: true
                        model: ListModel { id: gItems }
                        delegate: Column {
                            spacing: 1
                            Image {
                                width: 32; height: 32
                                smooth: false; cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(sid)
                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: gItems.remove(index)   // PPM = usun grunt
                                }
                            }
                            TibiaSpinBox {
                                width: 44; height: 18
                                from: 1; to: 1000
                                value: chance
                                onValueModified: gItems.setProperty(index, "chance", value)
                            }
                        }
                    }
                }

                // Bordery: PIERSCIEN wokol gruntu (jak na mapie) - krawedzie N/E/S/W
                // na osiach, ROGI (C*) na przekatnych blizej srodka, PRZEKATNE (D*)
                // na przekatnych na zewnatrz. Kazdy slot lezy tam, gdzie jego kafel
                // faktycznie wyladuje wzgledem gruntu.
                Text { text: "Bordery (upusc kafle; PPM czysci slot)"; color: "#999"; font.pixelSize: 11 }

                // Cel bordera: "wodny border" = ustaw cel na brush wody i uloz kafle.
                // Kazdy cel ma WLASNY zestaw 13 kafli. Przelaczanie nie kasuje pozostalych.
                Row {
                    spacing: 6
                    Text {
                        text: "Border do:"; color: "#999"; font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: targetCombo
                        width: 190; height: 23
                        property var keys: []
                        function syncFromApp() {
                            keys = root.borderTargetKeys()
                            model = keys.map(function(k) { return root.borderTargetLabel(k) })
                            var idx = keys.indexOf(root.borderTarget)
                            currentIndex = idx >= 0 ? idx : 0
                        }
                        onActivated: root.borderTarget = keys[currentIndex]
                    }
                }
                Item {
                    // Siatka logiczna 5x5, krok 50 px (kafel 44 + 6 przerwy).
                    width: 5 * 50 - 6
                    height: 5 * 50 - 6

                    // Srodek: podglad gruntu (pierwszy wariant) + podpis.
                    Rectangle {
                        x: 2 * 50; y: 2 * 50
                        width: 44; height: 44
                        color: "#1c1c1c"
                        border.color: "#3a3a3a"; border.width: 1
                        Image {
                            anchors.centerIn: parent
                            width: 32; height: 32
                            smooth: false; cache: false
                            fillMode: Image.PreserveAspectFit
                            source: gItems.count > 0 ? root.iconSrc(gItems.get(0).sid) : ""
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: gItems.count === 0
                            text: "Ground"
                            color: "#777"; font.pixelSize: 9
                        }
                    }

                    Repeater {
                        model: [
                            { bt: 9,  lab: "DNW", cx: 0, cy: 0 },
                            { bt: 1,  lab: "N",   cx: 2, cy: 0 },
                            { bt: 10, lab: "DNE", cx: 4, cy: 0 },
                            { bt: 5,  lab: "CNW", cx: 1, cy: 1 },
                            { bt: 6,  lab: "CNE", cx: 3, cy: 1 },
                            { bt: 4,  lab: "W",   cx: 0, cy: 2 },
                            { bt: 2,  lab: "E",   cx: 4, cy: 2 },
                            { bt: 7,  lab: "CSW", cx: 1, cy: 3 },
                            { bt: 8,  lab: "CSE", cx: 3, cy: 3 },
                            { bt: 12, lab: "DSW", cx: 0, cy: 4 },
                            { bt: 3,  lab: "S",   cx: 2, cy: 4 },
                            { bt: 11, lab: "DSE", cx: 4, cy: 4 }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            x: modelData.cx * 50
                            y: modelData.cy * 50
                            width: 44; height: 44
                            color: slotDrop.containsDrag ? "#2f4f3f" : "#252525"
                            border.color: slotDrop.containsDrag ? "#7fdc8f" : "#3a3a3a"
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                width: 32; height: 32
                                smooth: false; cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(root.borderIds[modelData.bt])
                            }
                            Text {
                                anchors { left: parent.left; top: parent.top; margins: 1 }
                                text: modelData.lab
                                color: "#777"; font.pixelSize: 8
                            }
                            DropArea {
                                id: slotDrop
                                anchors.fill: parent
                                onDropped: (drop) => root.setBorderTile(modelData.bt, drop.source.sid)
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: root.setBorderTile(modelData.bt, 0)
                            }
                        }
                    }
                }

                Row {
                    spacing: 6
                    TibiaButton { text: "Zapisz"; width: 90; onClicked: root.saveGround() }
                    TibiaButton {
                        text: "Testuj na mapie"; width: 120
                        enabled: gItems.count > 0 && root.curGround !== ""
                        onClicked: if (root.mapCtrl) root.mapCtrl.useGroundBrush(gItems.get(0).sid)
                    }
                }
            }

            // ---------- WALL ----------
            Column {
                visible: root.tab === "wall"
                spacing: 6
                width: parent.width

                Row {
                    spacing: 6
                    TibiaComboBox {
                        id: wallCombo
                        width: 150; height: 23
                        onActivated: root.loadWall(model[currentIndex])
                    }
                    TibiaButton { text: "Nowy"; width: 60; onClicked: root.newWall() }
                    TibiaButton {
                        text: "Usun"; width: 60
                        enabled: root.curWall !== ""
                        onClicked: { brushStore.deleteWallBrush(root.curWall); root.newWall() }
                    }
                }
                Row {
                    spacing: 6
                    Text { text: "Nazwa"; color: "#999"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                    TibiaTextField { id: wallNameField; width: 150; height: 22 }
                }

                // 16 slotow = maska polaczen (glif pokazuje, z ktorych stron slot
                // laczy sie ze sciana) + slot 16 (specjalny). PPM czysci.
                Text { text: "Sloty scian wg polaczen (upusc; PPM czysci)"; color: "#999"; font.pixelSize: 11 }
                Grid {
                    columns: 6
                    spacing: 3
                    Repeater {
                        // Glify box-drawing wg maski N=1 W=2 E=4 S=8.
                        model: ["•","╵","╴","┘","╶","└","─","┴",
                                "╷","│","┐","┤","┌","├","┬","┼","✦"]
                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            width: 44; height: 52
                            color: wDrop.containsDrag ? "#2f4f3f" : "#252525"
                            border.color: wDrop.containsDrag ? "#7fdc8f" : "#3a3a3a"
                            border.width: 1
                            Image {
                                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 2 }
                                width: 32; height: 32
                                smooth: false; cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(root.wallIds[index])
                            }
                            Text {
                                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 1 }
                                text: modelData
                                color: "#9a9a9a"; font.pixelSize: 12; font.bold: true
                            }
                            DropArea {
                                id: wDrop
                                anchors.fill: parent
                                onDropped: (drop) => {
                                    var w = root.wallIds.slice()
                                    w[index] = drop.source.sid
                                    root.wallIds = w
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    var w = root.wallIds.slice()
                                    w[index] = 0
                                    root.wallIds = w
                                }
                            }
                        }
                    }
                }

                TibiaButton { text: "Zapisz"; width: 90; onClicked: root.saveWall() }
            }

            TibiaButton {
                text: "Zamknij"; width: 90
                onClicked: root.close()
            }
        }

        // ===== Duch przeciagania (wspolny dla wszystkich itemow palety) =====
        Image {
            id: dragGhost
            width: 32; height: 32
            visible: false
            z: 1000
            smooth: false; cache: false
            fillMode: Image.PreserveAspectFit
            opacity: 0.85
            property int sid: 0
            Drag.active: visible
            Drag.source: dragGhost
            Drag.hotSpot.x: 16
            Drag.hotSpot.y: 16
        }
    }
}
