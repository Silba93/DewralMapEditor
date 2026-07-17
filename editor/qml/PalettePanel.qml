import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "style"

// Lewy panel palety: wybor kategorii RME (All/Terrain/Doodad/Item/RAW/My Palettes)
// + tilesetu, szukajka, siatka itemow, rozmiar/ksztalt pedzla, menu PPM
// (dodawanie/usuwanie z tilesetow i wlasnych palet) oraz dialog nowej nazwy.
Rectangle {
    id: palette
    // Okno glowne - API wlasnych palet (customPalettes/add/remove...) i stan aplikacji.
    required property var app
    // MapView (kontroler mapy) - aktywny pedzel, ksztalt/rozmiar.
    required property var mapCtrl

    width: 210
    color: "transparent"

    TibiaPanel {
        anchors.fill: parent
    }

        // Proxy palety: filtruje OtbReader po tilesecie/wlasnej palecie/szukajce.
        PaletteFilter {
            id: paletteFilter
            sourceModel: otbReader
        }

        Column {
            id: paletteCol
            anchors.fill: parent
            anchors.margins: 6
            spacing: 4

            // Wybor palety jak w RME (choicebook): kind (All/Terrain/Doodad/Item/
            // RAW/My Palettes) + Tileset (lista w obrebie wybranego kind).
            property var kinds: ["All Items", "Terrain Palette", "Doodad Palette", "Item Palette", "RAW Palette", "Creature Palette", "House Palette", "My Palettes"]
            property bool creatureMode: currentKind === "Creature Palette"
            property bool houseMode: currentKind === "House Palette"
            property string currentKind: kindCombo.currentText
            // Kategoria RME odpowiadajaca wybranemu kind ("" = All/My Palettes).
            property string currentCategory: {
                switch (currentKind) {
                case "Terrain Palette": return "terrain"
                case "Doodad Palette":  return "doodad"
                case "Item Palette":    return "item"
                case "RAW Palette":     return "raw"
                default: return ""
                }
            }
            // Lista drugiego poziomu: tilesety RME (TilesetStore juz zwraca baze +
            // wlasne dopiski scalone), albo wlasne palety.
            // "tilesetStore.revision" na poczatku: namesFor() jest Q_INVOKABLE (nie
            // Q_PROPERTY), wiec bez odczytania tej NOTIFY-jacej wlasciwosci QML nie
            // wie kiedy odswiezyc ten binding po addItem/newTileset/deleteTileset.
            property var subNames: {
                const _r = tilesetStore.revision   // wymuszona zaleznosc reaktywna
                if (currentCategory !== "") return tilesetStore.namesFor(currentCategory)
                if (currentKind === "My Palettes") return app.customPaletteNames
                return []
            }
            property bool showSub: currentKind !== "All Items" && !creatureMode && !houseMode
            property string currentSubName: (subCombo.currentIndex >= 0 && subCombo.currentIndex < subNames.length)
                                             ? subNames[subCombo.currentIndex] : ""
            // Nazwa wybranej WLASNEJ palety ("" gdy kind != My Palettes).
            property string currentCustomName: currentKind === "My Palettes" ? currentSubName : ""
            // Czy aktywny tileset da sie usunac: wlasna paleta, LUB tileset kategorii
            // ktory NIE istnieje w XML (czysto nasz, wtedy usuwalny w calosci).
            property bool canDeleteCurrentTileset: {
                const _r = tilesetStore.revision   // wymuszona zaleznosc reaktywna
                return currentSubName !== "" && (
                    currentKind === "My Palettes"
                    || (currentCategory !== "" && tilesetStore.isCustomOnly(currentCategory, currentSubName))
                )
            }

            // Jedno reaktywne zrodlo prawdy dla filtra palety - zamiast wielu osobnych
            // "onCurrentIndexChanged"/"onModelChanged" wywolan (kindCombo, subCombo),
            // ktore potrafily sie wyscigowo minac: gdy zmiana kategorii wypadala na TEN
            // SAM numeryczny indeks w subCombo co poprzednio, subCombo.currentIndex sie
            // nie zmienial wiec jego "currentIndexChanged" wcale nie odpalal, mimo ze
            // nazwa/model juz byly inne (nazwa aktualizowala sie w UI, ale stara lista
            // itemow zostawala). Tutaj currentIds zalezy od WSZYSTKICH skladowych na
            // raz i zawsze przelicza sie od zera - null = tryb "All Items".
            property var currentIds: {
                const _r = tilesetStore.revision   // wymuszona zaleznosc reaktywna
                if (currentKind === "All Items") return null
                if (currentSubName === "") return []
                if (currentKind === "My Palettes") return app.customPalettes[currentSubName] || []
                return tilesetStore.itemsFor(currentCategory, currentSubName)
            }
            onCurrentIdsChanged: {
                if (currentIds === null) { paletteFilter.mode = "all"; return }
                paletteFilter.setIds(currentIds)
                paletteFilter.mode = "ids"
            }

            // Przelacza na "My Palettes" i wybiera konkretna nazwe (po utworzeniu nowej).
            function selectCustomPalette(name) {
                kindCombo.currentIndex = kinds.indexOf("My Palettes")
                Qt.callLater(function() {
                    var idx = app.customPaletteNames.indexOf(name)
                    if (idx >= 0) subCombo.currentIndex = idx
                })
            }
            // Przelacza na palete danej kategorii RME i wybiera konkretny tileset.
            function selectCategoryTileset(category, name) {
                const kindName = { terrain: "Terrain Palette", doodad: "Doodad Palette",
                                    item: "Item Palette", raw: "RAW Palette" }[category]
                kindCombo.currentIndex = kinds.indexOf(kindName)
                Qt.callLater(function() {
                    var idx = paletteCol.subNames.indexOf(name)
                    if (idx >= 0) subCombo.currentIndex = idx
                })
            }

            // Wszystkie kontrolki naglowka w jednym Column - jego height (Column
            // pomija niewidoczne dzieci przy liczeniu rozmiaru) pozwala policzyc
            // dokladna wysokosc GridView bez magicznych stalych.
            Column {
                id: controlsColumn
                width: parent.width
                spacing: 4

                // Wybor kategorii (All/Terrain/Doodad/Item/RAW/My Palettes) - na pelna
                // szerokosc panelu, tak jak Tileset combobox ponizej. Zarzadzanie
                // tilesetami (nowy/usun) przeniesione do menu PPM na itemie palety
                // (palItemMenu -> "New tileset…" / "Remove from...") - patrz nizej.
                TibiaComboBox {
                    id: kindCombo
                    width: parent.width; height: 23
                    model: paletteCol.kinds
                    currentIndex: 0   // domyslnie "All Items" (jak native ComboBox wczesniej)
                }

                // Drugi poziom: Tileset (RME) / nazwa wlasnej palety.
                Text {
                    visible: paletteCol.showSub
                    text: paletteCol.currentKind === "My Palettes" ? "Palette" : "Tileset"
                    color: "#7fdc8f"; font.pixelSize: 10; font.bold: true
                }
                TibiaComboBox {
                    id: subCombo
                    visible: paletteCol.showSub
                    width: parent.width; height: 23
                    model: paletteCol.subNames
                    currentIndex: 0
                    onModelChanged: {
                        if (currentIndex >= model.length) currentIndex = 0
                    }
                }

                // Szukajka (nazwa lub poczatek server id) - jak PaletteSearch w map-forge.
                TibiaTextField {
                    id: palSearch
                    width: parent.width - 4; height: 22
                    placeholderText: "Search…"
                    onTextChanged: paletteFilter.searchText = text
                }

                Text {
                    text: (paletteCol.showSub && paletteCol.currentSubName !== ""
                           ? paletteCol.currentSubName : paletteCol.currentKind)
                          + "  (" + (paletteCol.creatureMode ? creatureStore.count
                                     : (paletteCol.houseMode ? houseCol.houses.length : grid.count)) + ")"
                    color: "#ddd"; font.pixelSize: 12; font.bold: true
                    elide: Text.ElideRight; width: parent.width
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
                // Zmiana rozmiaru komorek przelicza caly layout - stare contentY
                // wskazuje wtedy inny wiersz niz przed zmiana. Wracamy na gore.
                onCellWidthChanged: grid.positionViewAtBeginning()
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                model: otbReader.loaded ? paletteFilter : (datReader.loaded ? datReader : sprReader)

                delegate: Rectangle {
                    width: grid.cellWidth - 2
                    height: grid.cellHeight - 2
                    property bool isBrush: typeof serverId !== "undefined" && mapCtrl.brushServerId === serverId
                    color: isBrush ? "#2f6f4f" : (paletteCell.containsMouse ? "#303030" : "#252525")
                    border.color: isBrush ? "#7fdc8f" : "#3a3a3a"
                    border.width: isBrush ? 2 : 1

                    // Podglad doodada-compositu (stempel wielokaflowy) - caly stempel
                    // zlozony w jeden obrazek, skalowany do komorki. "" gdy to nie composite.
                    property string doodadPrev: (typeof serverId !== "undefined")
                                                ? mapCtrl.doodadPreviewSource(serverId) : ""

                    Image {
                        anchors.centerIn: parent
                        // Wspolna skala: 1 kafel ma ten sam rozmiar w kazdej palecie.
                        // Komorka miesci 2x2 kafle (64px zrodla), stad box/64.
                        // Stempel doodada ma dowolny rozmiar (cols x rows kafli), wiec
                        // bierzemy go z implicitWidth/Height obrazka - podstawianie 64
                        // rozciagalo stemple 1x1 i sciskalo wieksze niz 2x2.
                        readonly property int nativeW: parent.doodadPrev !== ""
                                ? implicitWidth
                                : (typeof itemWidth !== "undefined"  ? Math.min(itemWidth, 2)  : 1) * 32
                        readonly property int nativeH: parent.doodadPrev !== ""
                                ? implicitHeight
                                : (typeof itemHeight !== "undefined" ? Math.min(itemHeight, 2) : 1) * 32
                        readonly property real tileScale: (grid.cellWidth - 6) / 64
                        // Stempel wiekszy niz 2x2 nie moze wyjsc poza kratke - wtedy
                        // maleje dodatkowo, tak by zmiescic sie w komorce.
                        readonly property real fitScale: Math.min(tileScale,
                                (grid.cellWidth - 6) / Math.max(1, nativeW),
                                (grid.cellHeight - 6) / Math.max(1, nativeH))
                        width:  nativeW * fitScale
                        height: nativeH * fitScale
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: {
                            if (parent.doodadPrev !== "") return parent.doodadPrev
                            if (typeof spriteIds !== "undefined" && spriteIds.length > 0) {
                                return sprReader.itemImageSource(
                                    spriteIds,
                                    typeof itemWidth !== "undefined"  ? itemWidth  : 1,
                                    typeof itemHeight !== "undefined" ? itemHeight : 1,
                                    typeof layers !== "undefined"     ? layers     : 1
                                )
                            } else if (typeof spriteImageSource !== "undefined") {
                                return spriteImageSource
                            }
                            return ""
                        }
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.margins: 2
                        font.pixelSize: 9
                        color: "#777"
                        text: {
                            if (typeof serverId !== "undefined") return serverId
                            if (typeof itemId   !== "undefined") return itemId
                            if (typeof spriteId !== "undefined") return spriteId
                            return ""
                        }
                    }

                    MouseArea {
                        id: paletteCell
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse && typeof itemName !== "undefined" && itemName.length > 0
                        ToolTip.text: (typeof itemName !== "undefined" ? itemName : "")
                                      + (typeof serverId !== "undefined" ? "  (sid " + serverId + ")" : "")
                        // LPM wybiera pedzel; PPM otwiera menu palet (dodaj/usun).
                        onClicked: (mouse) => {
                            if (typeof serverId === "undefined") return
                            if (mouse.button === Qt.RightButton) {
                                palItemMenu.sid = serverId
                                palItemMenu.popup()
                            } else if (mapCtrl.brushServerId === serverId) {
                                mapCtrl.brushServerId = 0   // klik ponownie = odznacz
                            } else if (paletteCol.currentKind === "All Items"
                                       || paletteCol.currentKind === "RAW Palette") {
                                // RAW/All Items: surowy pojedynczy item (bez auto-borderow), jak w RME.
                                mapCtrl.brushServerId = serverId
                            } else {
                                // Terrain/Doodad/Item/My Palettes: tryb brusha (auto-bordery
                                // gdy id nalezy do ground brusha).
                                mapCtrl.useGroundBrush(serverId)
                            }
                        }
                    }
                }
            }

            // --- Paleta Creatures: potwory/NPC z creatures.xml + pedzel spawnu ---
            Column {
                anchors.fill: parent
                visible: paletteCol.creatureMode
                spacing: 4
                // Wyjscie z palety = rozbroj pedzle potwora/spawnu (jak House).
                onVisibleChanged: {
                    if (!visible) {
                        if (mapCtrl.creatureBrush !== "") mapCtrl.creatureBrush = ""
                        if (mapCtrl.spawnBrush) mapCtrl.spawnBrush = false
                    }
                }

                // Responsywnie: przycisk na cala szerokosc palety, pod nim wiersz
                // spawntime (label + spinbox wypelniajacy reszte) - nic nie wystaje
                // przy zwezaniu/rozszerzaniu panelu.
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
                        text: "Spawntime"; color: "#999"; font.pixelSize: 11
                        width: 80
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: spawntimeRow.width - stLabel.width - spawntimeRow.spacing
                        from: 1; to: 86400
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
                        text: "Spawn radius"; color: "#999"; font.pixelSize: 11
                        width: 80
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: spawnRadiusRow.width - srLabel.width - spawnRadiusRow.spacing
                        from: 1; to: 15
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
                        // Kafelek potwora: szerokosc jak reszta palet, wysokosc +
                        // miejsce na nazwe pod outfitem.
                        cellWidth: app.iconSizePx
                        cellHeight: app.iconSizePx + 14
                        onCellWidthChanged: creatureList.positionViewAtBeginning()
                        model: creatureStore
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
                                    // Outfit skalowany do kafelka (zostawiajac miejsce na nazwe).
                                    width: creatureList.cellWidth - 14
                                    height: creatureList.cellHeight - 18
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    smooth: false
                                    cache: false
                                    fillMode: Image.PreserveAspectFit
                                    source: {
                                        var p = datReader.outfitPreview(lookType)
                                        return (p.ids !== undefined && p.ids.length > 0)
                                            ? sprReader.itemImageSource(p.ids, p.width, p.height, 1)
                                            : ""
                                    }
                                }
                                Text {
                                    // Nazwa pod outfitem - przycieta do kafelka.
                                    text: name
                                    color: "#c0c0c0"; font.pixelSize: 10
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
                                // Klik = pedzel potwora; ponowny klik = odznacz.
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

            // --- Paleta House: lista domow + Add/Remove + Draw/Set exit (jak RME) ---
            Column {
                id: houseCol
                anchors.fill: parent
                visible: paletteCol.houseMode
                spacing: 4

                // Snapshot listy domow - odswiezany po kazdej zmianie mapy (dodanie/
                // usuniecie domu, malowanie kafli zmienia "size").
                property var allHouses: []
                property var towns: []
                property int selHouseId: -1
                // Lista domow FILTROWANA do wybranego miasta (jak RME house_choice) -
                // dom bez tego filtra latwo zgubic w liscie wszystkich.
                readonly property var houses: {
                    var tid = townCombo.currentTownId
                    if (tid <= 0) return []
                    return allHouses.filter(function(h) { return h.townId === tid })
                }
                function refresh() {
                    allHouses = otbmReader.housesList()
                    towns = otbmReader.townsList()
                }
                Connections {
                    target: otbmReader
                    function onMapChanged() { if (paletteCol.houseMode) houseCol.refresh() }
                    function onLoadedChanged() { houseCol.refresh(); houseCol.selHouseId = -1 }
                }

                // Wybor miasta (jak RME house_choice): NOWE domy dostaja jego id, a
                // lista ponizej pokazuje TYLKO domy tego miasta. Bez miasta TFS/RME
                // moze nie pozwolic wejsc/kupic domu (townid musi wskazywac cos realnego).
                Row {
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        text: "Town"; color: "#999"; font.pixelSize: 11
                        width: 40
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: townCombo
                        width: parent.width - 46
                        model: houseCol.towns.length > 0
                               ? houseCol.towns.map(function(t) { return t.name })
                               : ["No Town"]
                        currentIndex: houseCol.towns.length > 0 ? 0 : -1
                        readonly property int currentTownId:
                            (currentIndex >= 0 && currentIndex < houseCol.towns.length)
                            ? houseCol.towns[currentIndex].id : -1
                        // Klik uzytkownika zrywa binding currentIndex (jak w subCombo) -
                        // bez tego stary indeks przeciekalby do listy miast NOWEJ mapy.
                        onModelChanged: {
                            if (currentIndex < 0 || currentIndex >= model.length)
                                currentIndex = houseCol.towns.length > 0 ? 0 : -1
                        }
                        // currentIndex aktualizuje sie sam (jak w kindCombo/subCombo) -
                        // tu tylko czyscimy zaznaczenie domu z POPRZEDNIEGO miasta.
                        onActivated: houseCol.selHouseId = -1
                    }
                }
                onVisibleChanged: {
                    if (visible) {
                        refresh()
                    } else {
                        // Wyjscie z palety House = rozbroj pedzel domu i tryb wejscia
                        // (inaczej klik na mapie dalej malowalby niewidoczny juz dom).
                        if (mapCtrl.houseBrush > 0) mapCtrl.houseBrush = 0
                        mapCtrl.houseExitMode = false
                        selHouseId = -1
                    }
                }

                Row {
                    width: parent.width - 14
                    spacing: 6
                    TibiaButton {
                        text: "Add house"
                        width: (parent.width - 6) / 2
                        // Jak RME: bez miasta przycisk wylaczony - dom MUSI miec
                        // realne townid (0 nie istnieje, addTown numeruje od 1).
                        enabled: townCombo.currentTownId > 0
                        onClicked: {
                            houseCol.selHouseId = otbmReader.addHouse(townCombo.currentTownId)
                            houseCol.refresh()
                        }
                    }
                    TibiaButton {
                        text: "Remove"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        onClicked: {
                            if (mapCtrl.houseBrush === houseCol.selHouseId) mapCtrl.houseBrush = 0
                            otbmReader.removeHouse(houseCol.selHouseId)
                            houseCol.selHouseId = -1
                            houseCol.refresh()
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
                            mapCtrl.houseExitMode = false
                            mapCtrl.houseBrush = (mapCtrl.houseBrush === houseCol.selHouseId
                                                  && !mapCtrl.houseExitMode) ? 0 : houseCol.selHouseId
                        }
                    }
                    TibiaButton {
                        text: "Set exit"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        checked: mapCtrl.houseExitMode && mapCtrl.houseBrush === houseCol.selHouseId
                        onClicked: {
                            mapCtrl.houseBrush = houseCol.selHouseId
                            mapCtrl.houseExitMode = !mapCtrl.houseExitMode
                        }
                    }
                }

                // Edycja wybranego domu: nazwa + czynsz.
                TibiaTextField {
                    id: houseNameField
                    width: parent.width - 14
                    enabled: houseCol.selHouseId > 0
                    placeholderText: "House name"
                    onEditingFinished: {
                        if (houseCol.selHouseId > 0 && text !== "") {
                            otbmReader.setHouseName(houseCol.selHouseId, text)
                            houseCol.refresh()
                        }
                    }
                }
                Row {
                    id: rentRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: rentLabel
                        text: "Rent"; color: "#999"; font.pixelSize: 11
                        width: 50
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: rentRow.width - rentLabel.width - rentRow.spacing
                        from: 0; to: 100000000
                        enabled: houseCol.selHouseId > 0
                        value: {
                            for (var i = 0; i < houseCol.houses.length; ++i)
                                if (houseCol.houses[i].id === houseCol.selHouseId)
                                    return houseCol.houses[i].rent
                            return 0
                        }
                        onValueModified: {
                            if (houseCol.selHouseId > 0) otbmReader.setHouseRent(houseCol.selHouseId, value)
                        }
                    }
                }

                // Przypisanie WYBRANEGO domu do innego miasta (jak RME EditHouseDialog -
                // niezalezne od combo filtrujacego liste powyzej). currentIndex ustawiany
                // IMPERATYWNIE w onClicked delegatu (ten sam wzorzec co houseNameField.text) -
                // reaktywne wiazanie byloby i tak zerwane przez klik uzytkownika w combo.
                Row {
                    id: houseTownRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: houseTownLabel
                        text: "Town"; color: "#999"; font.pixelSize: 11
                        width: 50
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: houseTownCombo
                        width: houseTownRow.width - houseTownLabel.width - houseTownRow.spacing
                        enabled: houseCol.selHouseId > 0 && houseCol.towns.length > 0
                        model: houseCol.towns.length > 0
                               ? houseCol.towns.map(function(t) { return t.name })
                               : ["No Town"]
                        onActivated: {
                            if (houseCol.selHouseId > 0 && currentIndex >= 0
                                && currentIndex < houseCol.towns.length) {
                                otbmReader.setHouseTownId(houseCol.selHouseId,
                                                          houseCol.towns[currentIndex].id)
                                houseCol.refresh()
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
                                anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                                Text {
                                    text: modelData.name
                                    color: "#c0c0c0"; font.pixelSize: 12
                                    width: houseList.width - 12; elide: Text.ElideRight
                                }
                                Text {
                                    text: "id " + modelData.id + "   " + modelData.size + " sqm   rent " + modelData.rent
                                    color: "#888"; font.pixelSize: 10
                                }
                            }
                            MouseArea {
                                id: hma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    houseCol.selHouseId = modelData.id
                                    houseNameField.text = modelData.name
                                    // Synchronizuj combo "Town" (edycja domu) z FAKTYCZNYM
                                    // miastem tego domu - imperatywnie, jak houseNameField.text.
                                    for (var i = 0; i < houseCol.towns.length; ++i)
                                        if (houseCol.towns[i].id === modelData.townId) {
                                            houseTownCombo.currentIndex = i
                                            break
                                        }
                                    // Wybor domu od razu uzbraja pedzel (jak RME).
                                    mapCtrl.houseExitMode = false
                                    mapCtrl.houseBrush = modelData.id
                                }
                                onDoubleClicked: {
                                    // Dwuklik = skocz do wejscia domu (jesli ustawione).
                                    if (modelData.entryX > 0)
                                        mapCtrl.centerOnTile(modelData.entryX, modelData.entryY, modelData.entryZ)
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

            // --- Brush Size (jak w RME): ksztalt + promien pedzla ---
            Column {
                id: brushSizeBox
                width: parent.width
                spacing: 3

                Text { text: "Brush size"; color: "#ddd"; font.pixelSize: 11; font.bold: true }

                Flow {
                    width: parent.width
                    spacing: 3

                    // Przycisk pedzla w stylu classic Tibia: tlo panel_side.png (bevel 1px
                    // + kafelkowany szum), ikonka SZARA - jak w RME, bez niebieskiego.
                    component BrushBtn: Item {
                        id: bb
                        property bool active: false
                        property bool round: false     // ikonka: kolo czy kwadrat
                        property int iconSize: 14
                        property alias hovered: bbMa.containsMouse   // do ToolTipa z zewnatrz
                        signal clicked()
                        width: 26; height: 26

                        BorderImage {
                            anchors.fill: parent
                            source: (uiTheme.tex + "panel_side.png")
                            smooth: false
                            border { left: 1; right: 1; top: 1; bottom: 1 }
                            horizontalTileMode: BorderImage.Repeat
                            verticalTileMode: BorderImage.Repeat
                        }
                        // Stan: alfa NA POCZATKU (#AARRGGBB). Obrys tylko gdy aktywny -
                        // w stanie zwyklym ramke daje bevel tekstury.
                        Rectangle {
                            anchors.fill: parent
                            color: bb.active ? "#992f6f4f" : (bbMa.containsMouse ? "#28ffffff" : "transparent")
                            border.width: bb.active ? 1 : 0
                            border.color: "#7fdc8f"
                        }
                        // Ikonka: szary kwadrat/kolo (jasniejsza gdy aktywny).
                        Rectangle {
                            anchors.centerIn: parent
                            width: bb.iconSize; height: bb.iconSize
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

                    // Ksztalt: kwadrat / kolo (jak w RME)
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

                    Item { width: 10; height: 26 }   // odstep jak w RME

                    // Rozmiary: promienie RME 0,1,2,4,6,8,11 (rosnaca ikonka)
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
                            ToolTip.text: (modelData * 2 + 1) + "×" + (modelData * 2 + 1)
                        }
                    }
                }
            }
        }


        // Menu PPM na itemie palety: dodawanie/usuwanie z tilesetow RME (Terrain/
        // Doodad/Item/RAW) i z wlasnych palet.
        TibiaMenu {
            id: palItemMenu
            property int sid: 0

            // Podmenu dla jednej kategorii RME: lista tilesetow (baza+wlasne) + "New tileset…".
            component CategoryAddMenu: TibiaMenu {
                id: catMenu
                required property string category
                required property string label
                title: label
                Instantiator {
                    // tilesetStore.revision wymusza odswiezenie po addItem/newTileset gdzie indziej.
                    model: tilesetStore.revision, tilesetStore.namesFor(catMenu.category)
                    delegate: TibiaMenuItem {
                        text: modelData
                        onTriggered: tilesetStore.addItem(catMenu.category, modelData, palItemMenu.sid)
                    }
                    onObjectAdded: (index, object) => catMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => catMenu.removeItem(object)
                }
                MenuSeparator {
                    visible: (tilesetStore.revision, tilesetStore.namesFor(catMenu.category).length > 0)
                }
                TibiaMenuItem {
                    text: "New tileset…"
                    onTriggered: {
                        newPaletteField.text = ""
                        newPaletteDialog.pendingSid = palItemMenu.sid
                        newPaletteDialog.targetCategory = catMenu.category
                        newPaletteDialog.open()
                    }
                }
            }

            CategoryAddMenu { category: "terrain"; label: "Terrain Palette" }
            CategoryAddMenu { category: "doodad";  label: "Doodad Palette" }
            CategoryAddMenu { category: "item";    label: "Item Palette" }
            CategoryAddMenu { category: "raw";     label: "RAW Palette" }

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
                MenuSeparator { visible: app.customPaletteNames.length > 0 }
                TibiaMenuItem {
                    text: "New palette…"
                    onTriggered: {
                        newPaletteField.text = ""
                        newPaletteDialog.pendingSid = palItemMenu.sid
                        newPaletteDialog.targetCategory = ""
                        newPaletteDialog.open()
                    }
                }
            }

            MenuSeparator { visible: paletteCol.showSub && paletteCol.currentSubName !== "" }
            TibiaMenuItem {
                text: "Remove from \"" + (paletteCol.currentKind === "My Palettes"
                                          ? paletteCol.currentCustomName : paletteCol.currentSubName) + "\""
                visible: paletteCol.showSub && paletteCol.currentSubName !== ""
                height: visible ? implicitHeight : 0
                onTriggered: {
                    if (paletteCol.currentKind === "My Palettes")
                        app.removeItemFromPalette(paletteCol.currentCustomName, palItemMenu.sid)
                    else
                        tilesetStore.removeItem(paletteCol.currentCategory, paletteCol.currentSubName, palItemMenu.sid)
                }
            }
        }

        // Dialog nazwy nowej palety/tilesetu (opcjonalnie od razu dodaje item pendingSid).
        // targetCategory === "" -> wlasna paleta (My Palettes); inaczej nowy tileset
        // w tej kategorii RME (terrain/doodad/item/raw).
        TibiaDialog {
            id: newPaletteDialog
            property int pendingSid: 0
            property string targetCategory: ""
            title: targetCategory === "" ? "New palette" : "New tileset"

            function commit() {
                var name = newPaletteField.text.trim()
                if (name === "") return
                if (targetCategory === "") {
                    if (app.addCustomPalette(name) && pendingSid > 0)
                        app.addItemToPalette(name, pendingSid)
                    pendingSid = 0
                    paletteCol.selectCustomPalette(name)
                } else {
                    if (tilesetStore.newTileset(targetCategory, name) && pendingSid > 0)
                        tilesetStore.addItem(targetCategory, name, pendingSid)
                    pendingSid = 0
                    paletteCol.selectCategoryTileset(targetCategory, name)
                }
                newPaletteDialog.close()
            }

            onOpened: { newPaletteField.text = ""; newPaletteField.forceActiveFocus() }

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
                    TibiaButton { text: "OK"; width: 90; onClicked: newPaletteDialog.commit() }
                    TibiaButton { text: "Anuluj"; width: 90; onClicked: newPaletteDialog.close() }
                }
            }
        }

    // Zmiana pedzla -> przewin siatke palety do tego itemu.
    Connections {
        target: palette.mapCtrl
        function onBrushChanged() {
            if (palette.mapCtrl.brushServerId > 0) {
                // Wiersz w PROXY palety (aktywny tileset/paleta moze filtrowac).
                var row = paletteFilter.rowForServerId(palette.mapCtrl.brushServerId)
                if (row >= 0) grid.positionViewAtIndex(row, GridView.Center)
            }
        }
    }
}
