import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "style"

Rectangle {
    id: palette

    required property var app

    required property var mapCtrl

    width: 210
    color: "transparent"

    TibiaPanel {
        anchors.fill: parent
    }

    PaletteFilter {
        id: paletteFilter
        sourceModel: Backend.otbReader
    }

    Column {
        id: paletteCol
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        property var kinds: ["All Items", "Terrain Palette", "Doodad Palette", "Item Palette", "RAW Palette", "Creature Palette", "House Palette", "My Palettes"]
        property bool creatureMode: currentKind === "Creature Palette"
        property bool houseMode: currentKind === "House Palette"
        property string currentKind: kindCombo.currentText

        property string currentCategory: {
            switch (currentKind) {
            case "Terrain Palette":
                return "terrain";
            case "Doodad Palette":
                return "doodad";
            case "Item Palette":
                return "item";
            case "RAW Palette":
                return "raw";
            default:
                return "";
            }
        }

        property var subNames: {
            const _r = Backend.tilesetStore.revision;
            if (currentCategory !== "")
                return Backend.tilesetStore.namesFor(currentCategory);
            if (currentKind === "My Palettes")
                return app.customPaletteNames;
            return [];
        }
        property bool showSub: currentKind !== "All Items" && !creatureMode && !houseMode
        property string currentSubName: (subCombo.currentIndex >= 0 && subCombo.currentIndex < subNames.length) ? subNames[subCombo.currentIndex] : ""

        property string currentCustomName: currentKind === "My Palettes" ? currentSubName : ""

        property bool canDeleteCurrentTileset: {
            const _r = Backend.tilesetStore.revision;
            return currentSubName !== "" && (currentKind === "My Palettes" || (currentCategory !== "" && Backend.tilesetStore.isCustomOnly(currentCategory, currentSubName)));
        }

        property var currentIds: {
            const _r = Backend.tilesetStore.revision;
            if (currentKind === "All Items")
                return null;
            if (currentSubName === "")
                return [];
            if (currentKind === "My Palettes")
                return app.customPalettes[currentSubName] || [];
            return Backend.tilesetStore.itemsFor(currentCategory, currentSubName);
        }
        onCurrentIdsChanged: {
            if (currentIds === null) {
                paletteFilter.mode = "all";
                return;
            }
            paletteFilter.setIds(currentIds);
            paletteFilter.mode = "ids";
        }

        function selectCustomPalette(name) {
            kindCombo.currentIndex = kinds.indexOf("My Palettes");
            Qt.callLater(function () {
                var idx = app.customPaletteNames.indexOf(name);
                if (idx >= 0)
                    subCombo.currentIndex = idx;
            });
        }

        function selectCategoryTileset(category, name) {
            const kindName = {
                terrain: "Terrain Palette",
                doodad: "Doodad Palette",
                item: "Item Palette",
                raw: "RAW Palette"
            }[category];
            kindCombo.currentIndex = kinds.indexOf(kindName);
            Qt.callLater(function () {
                var idx = paletteCol.subNames.indexOf(name);
                if (idx >= 0)
                    subCombo.currentIndex = idx;
            });
        }

        Column {
            id: controlsColumn
            width: parent.width
            spacing: 4

            TibiaComboBox {
                id: kindCombo
                width: parent.width
                height: 23
                model: paletteCol.kinds
                currentIndex: 0
            }

            Text {
                visible: paletteCol.showSub
                text: paletteCol.currentKind === "My Palettes" ? "Palette" : "Tileset"
                color: "#7fdc8f"
                font.pixelSize: 10
                font.bold: true
            }
            TibiaComboBox {
                id: subCombo
                visible: paletteCol.showSub
                width: parent.width
                height: 23
                model: paletteCol.subNames
                currentIndex: 0
                onModelChanged: {
                    if (currentIndex >= model.length)
                        currentIndex = 0;
                }
            }

            TibiaTextField {
                id: palSearch
                width: parent.width - 4
                height: 22
                placeholderText: "Search..."
                onTextChanged: paletteFilter.searchText = text
            }

            Text {
                text: (paletteCol.showSub && paletteCol.currentSubName !== "" ? paletteCol.currentSubName : paletteCol.currentKind) + "  (" + (paletteCol.creatureMode ? Backend.creatureStore.count : (paletteCol.houseMode ? houseCol.houses.length : grid.count)) + ")"
                color: "#ddd"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                width: parent.width
            }
        }

        Item {
            width: parent.width
            height: parent.height - controlsColumn.height - brushSizeBox.height - paletteCol.spacing * 2

            GridView {
                id: grid
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - 14
                clip: true
                cellWidth: app.iconSizePx
                cellHeight: app.iconSizePx

                onCellWidthChanged: grid.positionViewAtBeginning()
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                model: Backend.otbReader.loaded ? paletteFilter : (Backend.datReader.loaded ? Backend.datReader : Backend.sprReader)

                delegate: Rectangle {
                    width: grid.cellWidth - 2
                    height: grid.cellHeight - 2
                    property bool isBrush: typeof serverId !== "undefined" && mapCtrl.brushServerId === serverId
                    color: isBrush ? "#2f6f4f" : (paletteCell.containsMouse ? "#303030" : "#252525")
                    border.color: isBrush ? "#7fdc8f" : "#3a3a3a"
                    border.width: isBrush ? 2 : 1

                    property string doodadPrev: (typeof serverId !== "undefined") ? mapCtrl.doodadPreviewSource(serverId) : ""

                    Image {
                        anchors.centerIn: parent

                        readonly property int nativeW: parent.doodadPrev !== "" ? implicitWidth : (typeof itemWidth !== "undefined" ? Math.min(itemWidth, 2) : 1) * 32
                        readonly property int nativeH: parent.doodadPrev !== "" ? implicitHeight : (typeof itemHeight !== "undefined" ? Math.min(itemHeight, 2) : 1) * 32
                        readonly property real tileScale: (grid.cellWidth - 6) / 64

                        readonly property real fitScale: Math.min(tileScale, (grid.cellWidth - 6) / Math.max(1, nativeW), (grid.cellHeight - 6) / Math.max(1, nativeH))
                        width: nativeW * fitScale
                        height: nativeH * fitScale
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: {
                            if (parent.doodadPrev !== "")
                                return parent.doodadPrev;
                            if (typeof spriteIds !== "undefined" && spriteIds.length > 0) {
                                return Backend.sprReader.itemImageSource(spriteIds, typeof itemWidth !== "undefined" ? itemWidth : 1, typeof itemHeight !== "undefined" ? itemHeight : 1, typeof layers !== "undefined" ? layers : 1);
                            } else if (typeof spriteImageSource !== "undefined") {
                                return spriteImageSource;
                            }
                            return "";
                        }
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.margins: 2
                        font.pixelSize: 9
                        color: "#777"
                        text: {
                            if (typeof serverId !== "undefined")
                                return serverId;
                            if (typeof itemId !== "undefined")
                                return itemId;
                            if (typeof spriteId !== "undefined")
                                return spriteId;
                            return "";
                        }
                    }

                    MouseArea {
                        id: paletteCell
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse && typeof itemName !== "undefined" && itemName.length > 0
                        ToolTip.text: (typeof itemName !== "undefined" ? itemName : "") + (typeof serverId !== "undefined" ? "  (sid " + serverId + ")" : "")

                        onClicked: mouse => {
                            if (typeof serverId === "undefined")
                                return;
                            if (mouse.button === Qt.RightButton) {
                                palItemMenu.sid = serverId;
                                palItemMenu.popup();
                            } else if (mapCtrl.brushServerId === serverId) {
                                mapCtrl.brushServerId = 0;
                            } else if (paletteCol.currentKind === "All Items" || paletteCol.currentKind === "RAW Palette") {
                                mapCtrl.brushServerId = serverId;
                            } else {
                                mapCtrl.useGroundBrush(serverId);
                            }
                        }
                    }
                }
            }

            Column {
                anchors.fill: parent
                visible: paletteCol.creatureMode
                spacing: 4

                onVisibleChanged: {
                    if (!visible) {
                        if (mapCtrl.creatureBrush !== "")
                            mapCtrl.creatureBrush = "";
                        if (mapCtrl.spawnBrush)
                            mapCtrl.spawnBrush = false;
                    }
                }

                TibiaButton {
                    text: "Spawn brush"
                    width: parent.width - 14
                    checked: mapCtrl.spawnBrush
                    onClicked: mapCtrl.spawnBrush = !mapCtrl.spawnBrush
                }
                Row {
                    id: spawntimeRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: stLabel
                        text: "Spawntime"
                        color: "#999"
                        font.pixelSize: 11
                        width: 80
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: spawntimeRow.width - stLabel.width - spawntimeRow.spacing
                        from: 1
                        to: 86400
                        value: mapCtrl.creatureSpawntime
                        onValueModified: mapCtrl.creatureSpawntime = value
                    }
                }
                Row {
                    id: spawnRadiusRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: srLabel
                        text: "Spawn radius"
                        color: "#999"
                        font.pixelSize: 11
                        width: 80
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: spawnRadiusRow.width - srLabel.width - spawnRadiusRow.spacing
                        from: 1
                        to: 15
                        value: mapCtrl.spawnBrushRadius
                        onValueModified: mapCtrl.spawnBrushRadius = value
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 26 - 22 - 22 - parent.spacing * 3

                    GridView {
                        id: creatureList
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width - 14
                        clip: true

                        cellWidth: app.iconSizePx
                        cellHeight: app.iconSizePx + 14
                        onCellWidthChanged: creatureList.positionViewAtBeginning()
                        model: Backend.creatureStore
                        delegate: Rectangle {
                            required property string name
                            required property bool isNpc
                            required property int lookType
                            width: creatureList.cellWidth - 2
                            height: creatureList.cellHeight - 2
                            property bool isBrush: mapCtrl.creatureBrush === name
                            color: isBrush ? "#2f6f4f" : (cma.containsMouse ? "#303030" : "#252525")
                            border.color: isBrush ? "#7fdc8f" : "#3a3a3a"
                            border.width: isBrush ? 2 : 1

                            Column {
                                anchors.centerIn: parent
                                spacing: 1
                                Image {

                                    width: creatureList.cellWidth - 14
                                    height: creatureList.cellHeight - 18
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    smooth: false
                                    cache: false
                                    fillMode: Image.PreserveAspectFit
                                    source: {
                                        var p = Backend.datReader.outfitPreview(lookType);
                                        return (p.ids !== undefined && p.ids.length > 0) ? Backend.sprReader.itemImageSource(p.ids, p.width, p.height, 1) : "";
                                    }
                                }
                                Text {

                                    text: name
                                    color: "#c0c0c0"
                                    font.pixelSize: 10
                                    width: creatureList.cellWidth - 8
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                            MouseArea {
                                id: cma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.delay: 500
                                ToolTip.text: name + (isNpc ? "  (NPC)" : "")

                                onClicked: mapCtrl.creatureBrush = isBrush ? "" : name
                            }
                        }
                    }
                    TibiaScrollBar {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        flickable: creatureList
                    }
                }
            }

            Column {
                id: houseCol
                anchors.fill: parent
                visible: paletteCol.houseMode
                spacing: 4

                property var allHouses: []
                property var towns: []
                property int selHouseId: -1

                readonly property var houses: {
                    var tid = townCombo.currentTownId;
                    if (tid <= 0)
                        return [];
                    return allHouses.filter(function (h) {
                        return h.townId === tid;
                    });
                }
                function refresh() {
                    allHouses = Backend.otbmReader.housesList();
                    towns = Backend.otbmReader.townsList();
                }
                Connections {
                    target: Backend.otbmReader
                    function onMapChanged() {
                        if (paletteCol.houseMode)
                            houseCol.refresh();
                    }
                    function onLoadedChanged() {
                        houseCol.refresh();
                        houseCol.selHouseId = -1;
                    }
                }

                Row {
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        text: "Town"
                        color: "#999"
                        font.pixelSize: 11
                        width: 40
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: townCombo
                        width: parent.width - 46
                        model: houseCol.towns.length > 0 ? houseCol.towns.map(function (t) {
                            return t.name;
                        }) : ["No Town"]
                        currentIndex: houseCol.towns.length > 0 ? 0 : -1
                        readonly property int currentTownId: (currentIndex >= 0 && currentIndex < houseCol.towns.length) ? houseCol.towns[currentIndex].id : -1

                        onModelChanged: {
                            if (currentIndex < 0 || currentIndex >= model.length)
                                currentIndex = houseCol.towns.length > 0 ? 0 : -1;
                        }

                        onActivated: houseCol.selHouseId = -1
                    }
                }
                onVisibleChanged: {
                    if (visible) {
                        refresh();
                    } else {
                        if (mapCtrl.houseBrush > 0)
                            mapCtrl.houseBrush = 0;
                        mapCtrl.houseExitMode = false;
                        selHouseId = -1;
                    }
                }

                Row {
                    width: parent.width - 14
                    spacing: 6
                    TibiaButton {
                        text: "Add house"
                        width: (parent.width - 6) / 2

                        enabled: townCombo.currentTownId > 0
                        onClicked: {
                            houseCol.selHouseId = Backend.otbmReader.addHouse(townCombo.currentTownId);
                            houseCol.refresh();
                        }
                    }
                    TibiaButton {
                        text: "Remove"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        onClicked: {
                            if (mapCtrl.houseBrush === houseCol.selHouseId)
                                mapCtrl.houseBrush = 0;
                            Backend.otbmReader.removeHouse(houseCol.selHouseId);
                            houseCol.selHouseId = -1;
                            houseCol.refresh();
                        }
                    }
                }
                Row {
                    width: parent.width - 14
                    spacing: 6
                    TibiaButton {
                        text: "Draw"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        checked: mapCtrl.houseBrush === houseCol.selHouseId && !mapCtrl.houseExitMode
                        onClicked: {
                            mapCtrl.houseExitMode = false;
                            mapCtrl.houseBrush = (mapCtrl.houseBrush === houseCol.selHouseId && !mapCtrl.houseExitMode) ? 0 : houseCol.selHouseId;
                        }
                    }
                    TibiaButton {
                        text: "Set exit"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        checked: mapCtrl.houseExitMode && mapCtrl.houseBrush === houseCol.selHouseId
                        onClicked: {
                            mapCtrl.houseBrush = houseCol.selHouseId;
                            mapCtrl.houseExitMode = !mapCtrl.houseExitMode;
                        }
                    }
                }

                TibiaTextField {
                    id: houseNameField
                    width: parent.width - 14
                    enabled: houseCol.selHouseId > 0
                    placeholderText: "House name"
                    onEditingFinished: {
                        if (houseCol.selHouseId > 0 && text !== "") {
                            Backend.otbmReader.setHouseName(houseCol.selHouseId, text);
                            houseCol.refresh();
                        }
                    }
                }
                Row {
                    id: rentRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: rentLabel
                        text: "Rent"
                        color: "#999"
                        font.pixelSize: 11
                        width: 50
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: rentRow.width - rentLabel.width - rentRow.spacing
                        from: 0
                        to: 100000000
                        enabled: houseCol.selHouseId > 0
                        value: {
                            for (var i = 0; i < houseCol.houses.length; ++i)
                                if (houseCol.houses[i].id === houseCol.selHouseId)
                                    return houseCol.houses[i].rent;
                            return 0;
                        }
                        onValueModified: {
                            if (houseCol.selHouseId > 0)
                                Backend.otbmReader.setHouseRent(houseCol.selHouseId, value);
                        }
                    }
                }

                Row {
                    id: houseTownRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: houseTownLabel
                        text: "Town"
                        color: "#999"
                        font.pixelSize: 11
                        width: 50
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: houseTownCombo
                        width: houseTownRow.width - houseTownLabel.width - houseTownRow.spacing
                        enabled: houseCol.selHouseId > 0 && houseCol.towns.length > 0
                        model: houseCol.towns.length > 0 ? houseCol.towns.map(function (t) {
                            return t.name;
                        }) : ["No Town"]
                        onActivated: {
                            if (houseCol.selHouseId > 0 && currentIndex >= 0 && currentIndex < houseCol.towns.length) {
                                Backend.otbmReader.setHouseTownId(houseCol.selHouseId, houseCol.towns[currentIndex].id);
                                houseCol.refresh();
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 23 * 2 - 26 * 2 - 22 * 2 - parent.spacing * 6

                    ListView {
                        id: houseList
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width - 14
                        clip: true
                        model: houseCol.houses
                        delegate: Rectangle {
                            required property var modelData
                            width: houseList.width
                            height: 34
                            property bool isSel: houseCol.selHouseId === modelData.id
                            color: isSel ? "#2f6f4f" : (hma.containsMouse ? "#303030" : "#252525")
                            border.color: isSel ? "#7fdc8f" : "#3a3a3a"
                            border.width: 1

                            Column {
                                anchors {
                                    left: parent.left
                                    leftMargin: 6
                                    verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: modelData.name
                                    color: "#c0c0c0"
                                    font.pixelSize: 12
                                    width: houseList.width - 12
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: "id " + modelData.id + "   " + modelData.size + " sqm   rent " + modelData.rent
                                    color: "#888"
                                    font.pixelSize: 10
                                }
                            }
                            MouseArea {
                                id: hma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    houseCol.selHouseId = modelData.id;
                                    houseNameField.text = modelData.name;

                                    for (var i = 0; i < houseCol.towns.length; ++i)
                                        if (houseCol.towns[i].id === modelData.townId) {
                                            houseTownCombo.currentIndex = i;
                                            break;
                                        }

                                    mapCtrl.houseExitMode = false;
                                    mapCtrl.houseBrush = modelData.id;
                                }
                                onDoubleClicked: {
                                    if (modelData.entryX > 0)
                                        mapCtrl.centerOnTile(modelData.entryX, modelData.entryY, modelData.entryZ);
                                }
                            }
                        }
                    }
                    TibiaScrollBar {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        flickable: houseList
                    }
                }
            }

            TibiaScrollBar {
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                flickable: grid
            }
        }

        Column {
            id: brushSizeBox
            width: parent.width
            spacing: 3

            Text {
                text: "Brush size"
                color: "#ddd"
                font.pixelSize: 11
                font.bold: true
            }

            Flow {
                width: parent.width
                spacing: 3

                Repeater {
                    model: ["square", "circle"]
                    delegate: BrushBtn {
                        required property string modelData
                        active: mapCtrl.brushShape === modelData
                        round: modelData === "circle"
                        iconSize: 14
                        onClicked: mapCtrl.brushShape = modelData
                    }
                }

                Item {
                    width: 10
                    height: 26
                }

                Repeater {
                    model: [0, 1, 2, 4, 6, 8, 11]
                    delegate: BrushBtn {
                        required property int modelData
                        required property int index
                        active: mapCtrl.brushSize === modelData
                        round: mapCtrl.brushShape === "circle"
                        iconSize: 6 + index * 2
                        onClicked: mapCtrl.brushSize = modelData
                        ToolTip.visible: hovered
                        ToolTip.text: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                    }
                }
            }
        }
    }

    component BrushBtn: Item {
        id: bb
        property bool active: false
        property bool round: false
        property int iconSize: 14
        property alias hovered: bbMa.containsMouse
        signal clicked
        width: 26
        height: 26

        BorderImage {
            anchors.fill: parent
            source: (Backend.uiTheme.tex + "panel_side.png")
            smooth: false
            border {
                left: 1
                right: 1
                top: 1
                bottom: 1
            }
            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat
        }

        Rectangle {
            anchors.fill: parent
            color: bb.active ? "#992f6f4f" : (bbMa.containsMouse ? "#28ffffff" : "transparent")
            border.width: bb.active ? 1 : 0
            border.color: "#7fdc8f"
        }

        Rectangle {
            anchors.centerIn: parent
            width: bb.iconSize
            height: bb.iconSize
            radius: bb.round ? width / 2 : 0
            color: bb.active ? "#d8d8d8" : "#9a9a9a"
            border.color: bb.active ? "#ffffff" : "#c8c8c8"
            border.width: 1
        }
        MouseArea {
            id: bbMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: bb.clicked()
        }
    }

    TibiaMenu {
        id: palItemMenu
        property int sid: 0

        CategoryAddMenu {
            category: "terrain"
            label: "Terrain Palette"
        }
        CategoryAddMenu {
            category: "doodad"
            label: "Doodad Palette"
        }
        CategoryAddMenu {
            category: "item"
            label: "Item Palette"
        }
        CategoryAddMenu {
            category: "raw"
            label: "RAW Palette"
        }

        TibiaMenu {
            id: addToMenu
            title: "My Palettes"
            Instantiator {
                model: app.customPaletteNames
                delegate: TibiaMenuItem {
                    text: modelData
                    onTriggered: app.addItemToPalette(modelData, palItemMenu.sid)
                }
                onObjectAdded: (index, object) => addToMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addToMenu.removeItem(object)
            }
            MenuSeparator {
                visible: app.customPaletteNames.length > 0
            }
            TibiaMenuItem {
                text: "New palette..."
                onTriggered: {
                    newPaletteField.text = "";
                    newPaletteDialog.pendingSid = palItemMenu.sid;
                    newPaletteDialog.targetCategory = "";
                    newPaletteDialog.open();
                }
            }
        }

        MenuSeparator {
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
        }
        TibiaMenuItem {
            text: "Remove from \"" + (paletteCol.currentKind === "My Palettes" ? paletteCol.currentCustomName : paletteCol.currentSubName) + "\""
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
            height: visible ? implicitHeight : 0
            onTriggered: {
                if (paletteCol.currentKind === "My Palettes")
                    app.removeItemFromPalette(paletteCol.currentCustomName, palItemMenu.sid);
                else
                    Backend.tilesetStore.removeItem(paletteCol.currentCategory, paletteCol.currentSubName, palItemMenu.sid);
            }
        }
    }

    component CategoryAddMenu: TibiaMenu {
        id: catMenu
        required property string category
        required property string label
        readonly property int tilesetRevision: Backend.tilesetStore.revision
        title: label
        Instantiator {

            model: {
                catMenu.tilesetRevision;
                return Backend.tilesetStore.namesFor(catMenu.category);
            }
            delegate: TibiaMenuItem {
                text: modelData
                onTriggered: Backend.tilesetStore.addItem(catMenu.category, modelData, palItemMenu.sid)
            }
            onObjectAdded: (index, object) => catMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => catMenu.removeItem(object)
        }
        MenuSeparator {
            visible: {
                catMenu.tilesetRevision;
                return Backend.tilesetStore.namesFor(catMenu.category).length > 0;
            }
        }
        TibiaMenuItem {
            text: "New tileset..."
            onTriggered: {
                newPaletteField.text = "";
                newPaletteDialog.pendingSid = palItemMenu.sid;
                newPaletteDialog.targetCategory = catMenu.category;
                newPaletteDialog.open();
            }
        }
    }

    TibiaDialog {
        id: newPaletteDialog
        property int pendingSid: 0
        property string targetCategory: ""
        title: targetCategory === "" ? "New palette" : "New tileset"

        function commit() {
            var name = newPaletteField.text.trim();
            if (name === "")
                return;
            if (targetCategory === "") {
                if (app.addCustomPalette(name) && pendingSid > 0)
                    app.addItemToPalette(name, pendingSid);
                pendingSid = 0;
                paletteCol.selectCustomPalette(name);
            } else {
                if (Backend.tilesetStore.newTileset(targetCategory, name) && pendingSid > 0)
                    Backend.tilesetStore.addItem(targetCategory, name, pendingSid);
                pendingSid = 0;
                paletteCol.selectCategoryTileset(targetCategory, name);
            }
            newPaletteDialog.close();
        }

        onOpened: {
            newPaletteField.text = "";
            newPaletteField.forceActiveFocus();
        }

        contentItem: Column {
            spacing: 10
            TibiaTextField {
                id: newPaletteField
                width: 220
                placeholderText: newPaletteDialog.targetCategory === "" ? "Palette name" : "Tileset name"
                onAccepted: newPaletteDialog.commit()
            }
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                TibiaButton {
                    text: "OK"
                    width: 90
                    onClicked: newPaletteDialog.commit()
                }
                TibiaButton {
                    text: "Cancel"
                    width: 90
                    onClicked: newPaletteDialog.close()
                }
            }
        }
    }

    Connections {
        target: palette.mapCtrl
        function onBrushChanged() {
            if (palette.mapCtrl.brushServerId > 0) {
                var row = paletteFilter.rowForServerId(palette.mapCtrl.brushServerId);
                if (row >= 0)
                    grid.positionViewAtIndex(row, GridView.Center);
            }
        }
    }
}
