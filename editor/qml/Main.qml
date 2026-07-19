import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Controls
import QtCore
import Tibia 1.0
import "dialogs"
import "style"

// -----------------------------------------------------------------------------
// Demo QML - minimalne UI do testowania readera.
//
// Model: otbReader (items.otb + Tibia.dat + Tibia.spr)
// Kolejnosc wczytywania: najpierw Tibia.dat, potem items.otb, potem Tibia.spr.
// DatReader musi byc wczytany przed OtbReader - OtbReader odpytuje DatReader
// o dane sprite przez itemByClientId().
//
// Role dostepne z otbReader: itemId, serverId, clientId, itemName, itemGroup,
// spriteIds, itemWidth, itemHeight, layers, isRenderable.
// -----------------------------------------------------------------------------

Window {
    id: root
    visible: started          // glowne okno edytora - dopiero po wczytaniu mapy
    width: 1000
    height: 680
    title: "Dewral Map Editor  -  " + (otbmReader.filePath !== "" ? fileTools.fileName(otbmReader.filePath) : "(no map)")
           + (otbmReader.dirty ? "  *" : "")
    // Bezramkowe okno (jak StartupWindow) - ramka wypelnia CALE okno (bez marginesu
    // jak w dialogu startowym, zeby nie tracic przestrzeni roboczej). Wlasny pasek
    // tytulu ponizej: przeciaganie + minimize + close.
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    TibiaDialogBackground {
        anchors.fill: parent
        // Wyzsza wersja tekstury (naglowek 45px zamiast klasycznych 27) - bevel u gory
        // i separator u dolu pozostaja ostre, doklejony tylko szum w srodku naglowka.
        frameSource: (uiTheme.tex + "popupwindow_tall.png")
        topBorder: 45
    }

    Item {
        id: titleBar
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 6; rightMargin: 6 }
        // Wysokosc = image-border-top popupwindow_tall.png (45), zeby dol paska
        // pokrywal sie z linia separatora zaszyta w teksturze naglowka (ponizej
        // zaczyna sie menu bar na tresci).
        height: 45

        Text {
            id: titleText
            // Tytul na SRODKU paska (menu po lewej, przyciski okna po prawej). Offset
            // w gore, bo w paśmie szumu NAD separatorem tekstury (separator w
            // popupwindow_tall.png ~34px, pasek 45px).
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -5
            text: root.title
            color: "#c0c0c0"
            font.bold: true
            font.pixelSize: 15
            elide: Text.ElideMiddle
            // Ograniczamy symetrycznie, zeby przy dlugiej nazwie mapy tytul nie wszedl
            // na menu (lewa) ani na przyciski okna (prawa) - zostaje wysrodkowany.
            width: Math.max(0, Math.min(implicitWidth,
                                        parent.width - 2 * (menuBar.width + 24)))
        }

        Row {
            id: winButtons
            anchors { right: parent.right; rightMargin: 6; verticalCenter: titleText.verticalCenter }
            spacing: 10

            Text {
                text: "—"
                color: minArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13; font.bold: true
                MouseArea {
                    id: minArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.showMinimized()
                }
            }
            Text {
                // Kwadrat = maksymalizuj, dwa nachodzace kwadraty = przywroc (jak w
                // standardowych paskach tytulu Windows).
                text: root.visibility === Window.Maximized ? "❐" : "□"
                color: maxArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13; font.bold: true
                MouseArea {
                    id: maxArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
                }
            }
            Text {
                text: "✕"
                color: closeArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13; font.bold: true
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 70   // nie lap kliku na minimize/maximize/close
            onPressed: root.startSystemMove()
            // Podwojny klik na pasku = maksymalizuj/przywroc (jak natywny pasek tytulu).
            onDoubleClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
        }
    }

    // -------------------------------------------------------------------------
    // Stan startowego loadera
    // -------------------------------------------------------------------------
    property bool started: false

    // Licznik FPS - realne klatki okna (renderer OpenGL renderuje ciagle).
    property int frameCount: 0
    property int fps: 0
    onFrameSwapped: frameCount++
    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: { root.fps = root.frameCount; root.frameCount = 0 }
    }

    // Sciezka aktywnej mapy zyje w dokumencie (otbmReader.filePath) - karty map.
    property var recentMaps: []

    // Trwale preferencje (foldery klientow per wersja, ostatnie mapy, dialog)
    Settings {
        id: prefs
        property string clientFolder: ""       // legacy (migrowane do clientPathsJson)
        property string clientPathsJson: "{}"  // { "772": "C:/...", "1098": "D:/..." }
        // Profile CUSTOM (osobne pozycje na liscie wersji, nie aliasy!):
        // [{ "name": "Midhem", "base": 1098 }] - kazdy ma wlasny folder klienta
        // (klucz "Midhem" w clientPathsJson) i wlasny katalog data/Midhem/.
        property string customProfilesJson: "[]"
        // Pamiec "mapa -> profil": { "C:/maps/midhem.otbm": "Midhem" }. Mapa niesie
        // tylko NUMER wersji, wiec bez tego kazde otwarcie wracalo na profil
        // numeryczny (i np. palety zapisywaly sie do data/1098 zamiast data/Midhem).
        property string mapProfilesJson: "{}"
        property string recentMapsJson: "[]"
        property string customPalettesJson: "{}" // { "Moja paleta": [serverId...] }
        property bool showStartup: true
        property int glMaxFps: 0          // limit FPS renderera OpenGL (0 = bez limitu)
        property int paletteWidth: 210    // szerokosc panelu palety (resize za uchwyt)
        property bool paletteCollapsed: false  // panel palety schowany (wiecej miejsca na mape)
        property int iconSize: 66         // rozmiar kafelka palety w px (Small/Medium/Large)
    }

    // Rozmiar kafelka palety (px) - z prefs, wspolny dla wszystkich palet (item/creature).
    readonly property int iconSizePx: prefs.iconSize

    // --- Wlasne palety (jak modul tilesetow w map-forge) ---
    property var customPalettes: ({})
    readonly property var customPaletteNames: Object.keys(customPalettes).sort()

    function loadCustomPalettes() {
        try { customPalettes = JSON.parse(prefs.customPalettesJson) || ({}) }
        catch (e) { customPalettes = ({}) }
    }
    function saveCustomPalettes() { prefs.customPalettesJson = JSON.stringify(customPalettes) }
    function addCustomPalette(name) {
        if (!name || customPalettes[name] !== undefined) return false
        var cp = JSON.parse(JSON.stringify(customPalettes))
        cp[name] = []
        customPalettes = cp; saveCustomPalettes()
        return true
    }
    function deleteCustomPalette(name) {
        var cp = JSON.parse(JSON.stringify(customPalettes))
        delete cp[name]
        customPalettes = cp; saveCustomPalettes()
    }
    function addItemToPalette(name, sid) {
        if (customPalettes[name] === undefined) return
        var cp = JSON.parse(JSON.stringify(customPalettes))
        if (cp[name].indexOf(sid) < 0) cp[name].push(sid)
        customPalettes = cp; saveCustomPalettes()
    }
    function removeItemFromPalette(name, sid) {
        if (customPalettes[name] === undefined) return
        var cp = JSON.parse(JSON.stringify(customPalettes))
        var i = cp[name].indexOf(sid)
        if (i >= 0) cp[name].splice(i, 1)
        customPalettes = cp; saveCustomPalettes()
    }

    // Wlasne dopiski do palet RME (Terrain/Doodad/Item/RAW) trzyma teraz TilesetStore
    // (C++), zapisywane do tilesets.json W FOLDERZE KLIENTA (obok Tibia.dat) - patrz
    // demo/tilesetstore.h. Podruzuje z klientem/mapa, nie z ustawieniami aplikacji.

    // --- Profile klienta (jak RME: kazdy ma swoj folder dat/spr/otb) ---
    // Tozsamosc profilu to KLUCZ (string): "1098" = wersja bazowa, "Midhem" =
    // profil custom (osobna pozycja listy, z baza w customProfiles). clientPaths
    // mapuje klucz -> folder klienta.
    property var clientPaths: ({})
    property var customProfiles: []   // [{name:"Midhem", base:1098}]
    property var mapProfiles: ({})    // sciezka mapy -> klucz profilu (patrz prefs)
    readonly property var knownVersions: [772, 780, 792, 800, 810, 820, 840, 850, 854,
                                          860, 870, 910, 946, 954, 960, 986, 1010,
                                          1030, 1041, 1077, 1098]
    property int loadedClientVersion: 0        // wersja BAZOWA zaladowanego klienta (numer)
    property string loadedClientKey: ""        // klucz zaladowanego profilu ("1098"/"Midhem")
    property string loadedClientFolder: ""
    property string pendingMapPath: ""         // mapa czekajaca na wskazanie folderu
    property string pendingKey: ""             // profil czekajacy na wskazanie folderu

    function versionLabel(v) {
        if (v >= 10100) return "10.100"
        return Math.floor(v / 100) + "." + ("0" + (v % 100)).slice(-2)
    }

    // --- Klucze profili ---
    // Wersja bazowa profilu: "1098" -> 1098; "Midhem" -> base z customProfiles.
    function profileVer(key) {
        var n = Number(key)
        if (!isNaN(n) && n > 0) return n
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name === key) return customProfiles[i].base
        return 0
    }
    // Etykieta: "10.98" dla bazowych, "Midhem  (10.98)" dla customow.
    function profileLabel(key) {
        var n = Number(key)
        if (!isNaN(n) && n > 0) return versionLabel(n)
        return key + "  (" + versionLabel(profileVer(key)) + ")"
    }
    // Wszystkie pozycje listy profili: wersje bazowe + customy na koncu.
    function allProfileKeys() {
        return knownVersions.map(function(v) { return String(v) })
                            .concat(customProfiles.map(function(p) { return p.name }))
    }
    function isCustomKey(key) { return isNaN(Number(key)) || Number(key) <= 0 }

    // Nowy profil custom oparty o wersje bazowa. false = zla/zajeta nazwa.
    function addCustomProfile(name, base) {
        name = (name || "").trim()
        // Nazwa nie moze byc pusta, liczba (kolizja z bazowymi) ani duplikatem.
        if (name === "" || !isNaN(Number(name))) return false
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name.toLowerCase() === name.toLowerCase()) return false
        var cp = customProfiles.slice()
        cp.push({ name: name, base: base })
        customProfiles = cp
        prefs.customProfilesJson = JSON.stringify(customProfiles)
        return true
    }
    function removeCustomProfile(name) {
        customProfiles = customProfiles.filter(function(p) { return p.name !== name })
        prefs.customProfilesJson = JSON.stringify(customProfiles)
        var paths = JSON.parse(JSON.stringify(clientPaths))
        delete paths[name]
        clientPaths = paths
        saveClientPaths()
    }

    function loadClientPaths() {
        try { clientPaths = JSON.parse(prefs.clientPathsJson) || ({}) }
        catch (e) { clientPaths = ({}) }
        try { customProfiles = JSON.parse(prefs.customProfilesJson) || [] }
        catch (e) { customProfiles = [] }
        try { mapProfiles = JSON.parse(prefs.mapProfilesJson) || ({}) }
        catch (e) { mapProfiles = ({}) }
        // Migracja starego pojedynczego folderu (7.72) do nowego modelu.
        if (Object.keys(clientPaths).length === 0 && prefs.clientFolder !== "") {
            var cp = ({}); cp["772"] = prefs.clientFolder
            clientPaths = cp
            saveClientPaths()
        }
    }
    function saveClientPaths() { prefs.clientPathsJson = JSON.stringify(clientPaths) }
    function setVersionFolder(ver, folder) {
        var cp = JSON.parse(JSON.stringify(clientPaths))
        cp[String(ver)] = folder
        clientPaths = cp          // nowy obiekt -> bindingi sie odswieza
        saveClientPaths()
    }
    // Dane profilu (brushes/tilesety/creatures/items.xml). Profil CUSTOM (np.
    // "Midhem") ma WLASNY katalog data/<Nazwa>/ - per plik, z fallbackiem do bazy
    // data/<wersja>/:
    //  - brushes/creatures/items.xml: tylko odczyt -> gdy w profilu brak pliku,
    //    bierzemy bazowy (mozna nadpisac np. same creatures).
    //  - tilesets: ZAWSZE katalog profilu - edycje palet zapisuja sie do tego
    //    samego pliku, wiec wlasne palety Midhema laduja w data/Midhem/
    //    tilesets.json i nie nadpisuja bazy wersji. Punkt startowy: skopiuj
    //    data/<wersja>/tilesets.json do data/<Nazwa>/.
    function loadProfileData(key) {
        var ver = profileVer(key)
        if (!isCustomKey(key)) {
            tilesetStore.loadForVersion(ver)
            brushStore.loadForVersion(ver)
            creatureStore.loadForVersion(ver)
            itemsXml.loadForVersion(ver)
            return
        }
        tilesetStore.loadForDir(key)
        if (!brushStore.loadForDir(key))    brushStore.loadForVersion(ver)
        if (!creatureStore.loadForDir(key)) creatureStore.loadForVersion(ver)
        if (!itemsXml.loadForDir(key))      itemsXml.loadForVersion(ver)
    }

    // Pliki klienta w folderze (preferujac standardowe nazwy).
    function clientFiles(folder) {
        if (!folder) return { dat: "", spr: "", otb: "" }
        return { dat: fileTools.findByExt(folder, "dat", "Tibia.dat"),
                 spr: fileTools.findByExt(folder, "spr", "Tibia.spr"),
                 otb: fileTools.findByExt(folder, "otb", "items.otb") }
    }

    function loadRecent() {
        try { recentMaps = JSON.parse(prefs.recentMapsJson) || [] }
        catch (e) { recentMaps = [] }
    }

    function addRecent(path) {
        var list = recentMaps.slice()
        var i = list.indexOf(path)
        if (i >= 0) list.splice(i, 1)
        list.unshift(path)
        if (list.length > 12) list = list.slice(0, 12)
        recentMaps = list
        prefs.recentMapsJson = JSON.stringify(list)
    }

    // "dirty" zyje w dokumencie (otbmReader.dirty) - kazda karta ma wlasna flage.
    property string savedToast: ""  // krotki komunikat po zapisie

    // Laduje dat/spr/otb dla wersji klienta wykrytej z mapy READERA, o ile jeszcze
    // nie zaladowane. Wydzielone z loadEverything, bo przelaczenie KARTY na mape z
    // innej wersji tez musi to umiec. false = folder wersji nieskonfigurowany.
    function ensureClientLoaded(reader) {
        var ver = reader.suggestedClientVersion()
        if (ver <= 0) ver = 772
        // Pamiec per mapa MA PIERWSZENSTWO: raz wybrany profil (np. "Midhem")
        // trzyma sie tej mapy, mimo ze naglowek mowi tylko "1098".
        var remembered = reader.filePath !== "" ? (mapProfiles[reader.filePath] || "") : ""
        var key = remembered !== "" && (clientPaths[remembered] || "") !== ""
                  ? remembered : resolveKeyForVersion(ver)
        return ensureClientVersion(key)
    }

    // Zapamietaj profil dla mapy (trwale, w Settings).
    function rememberMapProfile(mapPath, key) {
        if (mapPath === "" || key === "") return
        if (mapProfiles[mapPath] === key) return
        var mp = JSON.parse(JSON.stringify(mapProfiles))
        mp[mapPath] = key
        mapProfiles = mp
        prefs.mapProfilesJson = JSON.stringify(mapProfiles)
    }

    // Reczne przelaczenie profilu dla BIEZACEJ mapy (menu Map > Profil klienta).
    function switchMapProfile(key) {
        if (!ensureClientVersion(key)) return
        if (otbmReader.filePath !== "") rememberMapProfile(otbmReader.filePath, key)
    }

    // Skonfigurowane profile (z folderem) - do przelacznika w menu Map.
    function configuredProfileKeys() {
        var keys = Object.keys(clientPaths).filter(function(k) {
            return (clientPaths[k] || "") !== ""
        })
        keys.sort(function(a, b) {
            var na = Number(a), nb = Number(b)
            var ca = isNaN(na), cb = isNaN(nb)
            if (ca !== cb) return ca ? 1 : -1
            return ca ? a.localeCompare(b) : na - nb
        })
        return keys
    }

    // Mapa mowi tylko NUMER wersji - wybierz profil: bazowy numeryczny, jesli ma
    // folder; inaczej pierwszy custom o tej bazie z folderem; inaczej numeryczny
    // (nieskonfigurowany -> flow zapyta o folder).
    function resolveKeyForVersion(ver) {
        if ((clientPaths[String(ver)] || "") !== "") return String(ver)
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].base === ver
                && (clientPaths[customProfiles[i].name] || "") !== "")
                return customProfiles[i].name
        return String(ver)
    }

    // Jak wyzej, ale dla JAWNEGO KLUCZA profilu ("1098" / "Midhem") - File > New
    // i ekran startowy wybieraja profil wprost.
    function ensureClientVersion(key) {
        key = String(key)
        var ver = profileVer(key)
        var folder = clientPaths[key] || ""
        var files = clientFiles(folder)
        if (ver <= 0 || folder === "" || !files.dat || !files.spr || !files.otb) return false

        if (loadedClientKey !== key || loadedClientFolder !== folder) {
            // .otfi (jak RME) nadpisuje autodetekcje formatu .dat/.spr wg wersji -
            // potrzebne dla niestandardowych klientow (np. OTClient enableFeature(...)).
            var hasOtfi = otfiReader.loadFromFolder(folder)
            var datFile = hasOtfi ? (folder + "/" + otfiReader.metadataFile) : files.dat
            var sprFile = hasOtfi ? (folder + "/" + otfiReader.spritesFile) : files.spr

            datReader.clientVersion = ver
            datReader.setOtfiOverrides(hasOtfi, otfiReader.extended, otfiReader.frameDurations, otfiReader.frameGroups)
            datReader.loadFile(datFile, 0)

            var extendedSpr = hasOtfi ? otfiReader.extended : (ver >= 960)  // 9.60+: naglowek .spr u32
            var alphaSpr = hasOtfi ? otfiReader.transparency : false
            sprReader.loadFile(sprFile, 0, extendedSpr, alphaSpr)

            otbReader.loadFile(files.otb)
            loadedClientVersion = ver
            loadedClientKey = key
            loadedClientFolder = folder
            loadProfileData(key)   // data/<NazwaProfilu>/ lub data/<ver>/ (patrz funkcja)
            // Atlas mogl powstac na ZLYCH danych klienta (flow "mapa najpierw" dla
            // wykrycia wersji, albo karta z innej wersji) - przebuduj od zera.
            mapView.rebuildAtlas()
        }
        return true
    }

    // Nowy flow (jak RME): MAPA najpierw -> naglowek OTBM mowi jaka wersja klienta
    // -> ladujemy dat/spr/otb z folderu skonfigurowanego dla TEJ wersji. Gdy brak
    // konfiguracji, pytamy o folder i wracamy do ladowania.
    // File > New: nowa pusta mapa w nowej karcie. Wersja OTBM i wersje items.otb
    // ustawia OtbmReader::newMap; filePath zostaje pusty, wiec Ctrl+S robi Save As.
    function createNewMap(key, w, h) {
        if (!ensureClientVersion(key)) {
            // Brak folderu klienta tego profilu - popros i wroc tu po wyborze.
            pendingNewMap = { key: key, w: w, h: h }
            pendingKey = String(key)
            if (started) versionFolderDialogMain.open()
            else startupScreen.openVersionFolderDialog()
            return
        }
        if (otbmReader.loaded || otbmReader.filePath !== "") docMgr.newDocument()
        otbmReader.newMap(w, h, profileVer(key), otbReader.majorVersion, otbReader.minorVersion)
        mapView.centerOnTile(Math.floor(w / 2), Math.floor(h / 2), 7)  // srodek, parter
        started = true
    }
    property var pendingNewMap: null   // {key,w,h} czekajace na wskazanie folderu

    function loadEverything(mapPath) {
        if (mapPath === "") return false

        // Karty map: mapa otwarta w innej karcie -> przelacz na nia (jak RME).
        // Ta sama karta (powrot z wyboru folderu wersji) = przeladowanie w miejscu.
        var existing = docMgr.indexOfPath(mapPath)
        if (existing >= 0 && existing !== docMgr.currentIndex) {
            docMgr.currentIndex = existing
            ensureClientLoaded(docMgr.current)
            started = true
            return true
        }
        // Biezaca karta trzyma juz INNA zaladowana mape -> otworz nowa karte.
        if (otbmReader.loaded && otbmReader.filePath !== mapPath) docMgr.newDocument()

        // mapView.loadMap (NIE otbmReader.loadFile bezposrednio!) - trzyma lock na
        // caly czas wczytywania, inaczej watek roboczy MapView moze w tym momencie
        // czytac kafelki ktore reset+reload wlasnie kasuje/realokuje (use-after-free,
        // zalezny od timingu - "ta sama mapa czasem wczytuje sie czasem nie").
        if (!mapView.loadMap(mapPath)) return false

        if (!ensureClientLoaded(otbmReader)) {
            pendingMapPath = mapPath
            pendingKey = resolveKeyForVersion(otbmReader.suggestedClientVersion() > 0
                                              ? otbmReader.suggestedClientVersion() : 772)
            if (started) versionFolderDialogMain.open()
            else startupScreen.openVersionFolderDialog()
            return false
        }
        // Utrwal skojarzenie mapa->profil (pierwsze otwarcie zapisuje wynik
        // resolvera; reczna zmiana w menu Map nadpisze go pozniej).
        rememberMapProfile(mapPath, loadedClientKey)
        addRecent(mapPath)
        started = true
        return true
    }

    // Wspolna obsluga wyboru folderu dla wersji (dialogi startup/main).
    function onVersionFolderPicked(folderUrl) {
        var f = fileTools.toLocalFile(folderUrl)
        if (!f) return
        setVersionFolder(pendingKey, f)
        var mp = pendingMapPath
        pendingMapPath = ""
        if (mp !== "") loadEverything(mp)
        else if (pendingNewMap) {   // File > New czekalo na folder tego profilu
            var nm = pendingNewMap
            pendingNewMap = null
            createNewMap(nm.key, nm.w, nm.h)
        }
    }

    function saveMap() {
        if (otbmReader.filePath === "") { saveDialog.open(); return }
        // Naglowek pod aktualnego klienta (OTBM version + wersje items.otb), jak RME.
        otbmReader.applyClientVersions(loadedClientVersion, otbReader.majorVersion, otbReader.minorVersion)
        if (otbmReader.saveFile(otbmReader.filePath)) {   // saveFile sam czysci dirty
            rememberMapProfile(otbmReader.filePath, loadedClientKey)
            savedToast = "Saved: " + fileTools.fileName(otbmReader.filePath)
            savedToastTimer.restart()
        }
    }

    // Karty map: zamkniecie karty (z pytaniem przy niezapisanych zmianach).
    function closeTab(i) {
        var t = docMgr.tabs[i]
        if (t && t.dirty) {
            closeTabConfirm.tabIndex = i
            closeTabConfirm.message = "Mapa \"" + t.title + "\" ma niezapisane zmiany.\nZamknac bez zapisywania?"
            closeTabConfirm.open()
        } else {
            doCloseTab(i)
        }
    }
    function doCloseTab(i) {
        // true = nie zostal zaden zaladowany dokument -> ekran startowy.
        if (docMgr.closeDocument(i)) started = false
        else ensureClientLoaded(docMgr.current)   // karta obok moze byc z innej wersji
    }

    // Przelaczenie karty (klik/zamkniecie): mapa w karcie moze byc z innej wersji
    // klienta - dociagnij dat/spr/otb. mapView przelacza sie sam (otbm: docMgr.current).
    Connections {
        target: docMgr
        function onCurrentChanged() {
            if (docMgr.current && docMgr.current.loaded) ensureClientLoaded(docMgr.current)
        }
    }

    // Zmiana pedzla -> przewin palete do tego itemu.
    Timer { id: savedToastTimer; interval: 1800; onTriggered: root.savedToast = "" }

    Component.onCompleted: {
        loadRecent()
        loadClientPaths()
        loadCustomPalettes()
        // Auto-start gdy uzytkownik wylaczyl dialog i mamy ostatnia mape.
        if (!prefs.showStartup && recentMaps.length > 0)
            loadEverything(recentMaps[0])
    }

    // -------------------------------------------------------------------------
    // Pasek menu (File)
    // -------------------------------------------------------------------------
    // Menu siedzi W PASKU TYTULU (po lewej), tytul po prawej - brak osobnego paska.
    // Zadeklarowany PO titleBar, wiec jest nad nim w z-order i lapie kliki zanim
    // dosiegnie ich MouseArea przeciagania okna.
    TibiaMenuBar {
        id: menuBar
        anchors.verticalCenter: titleBar.verticalCenter
        anchors.verticalCenterOffset: -4
        anchors.left: titleBar.left
        anchors.leftMargin: 4

        TibiaMenu {
            title: "File"
            Action { text: "New…";     shortcut: "Ctrl+N"; onTriggered: newMapDialog.open() }
            Action { text: "Open…";    shortcut: "Ctrl+O"; onTriggered: startupScreen.openMapDialog() }
            Action { text: "Save";          shortcut: "Ctrl+S"; enabled: otbmReader.loaded; onTriggered: root.saveMap() }
            // RME: "Ctrl+Alt+S". Zostawiamy tez Ctrl+Shift+S (powszechna konwencja) -
            // patrz dodatkowy Shortcut{} ponizej menu, oba wywoluja to samo.
            Action { text: "Save As…"; shortcut: "Ctrl+Shift+S"; enabled: otbmReader.loaded; onTriggered: saveDialog.open() }
            MenuSeparator {}
            // RME: Ctrl+Q = zamknij MAPE (biezaca karte, nie aplikacje). "Exit"
            // zostaje bez skrotu (Alt+F4 dziala z poziomu systemu).
            Action { text: "Close map"; shortcut: "Ctrl+Q"; onTriggered: root.closeTab(docMgr.currentIndex) }
            Action { text: "Exit"; onTriggered: Qt.quit() }
        }

        TibiaMenu {
            title: "Edit"
            Action {
                text: "Undo"; shortcut: "Ctrl+Z"
                enabled: otbmReader.undoCount > 0
                onTriggered: mapView.undo()
            }
            Action {
                // RME uzywa Ctrl+Shift+Z; Ctrl+Y zostaje jako alias (patrz Shortcut nizej).
                text: "Redo"; shortcut: "Ctrl+Shift+Z"
                enabled: otbmReader.redoCount > 0
                onTriggered: mapView.redo()
            }
            MenuSeparator {}

            Action {
                text: "Find Item…"; shortcut: "Ctrl+F"
                enabled: otbmReader.loaded
                onTriggered: { selItemDialog.mode = "find"; selItemDialog.scope = "map"; selItemDialog.open() }
            }
            Action {
                text: "Replace Items…"; shortcut: "Ctrl+Shift+F"
                enabled: otbmReader.loaded
                onTriggered: { selItemDialog.mode = "replace"; selItemDialog.scope = "map"; selItemDialog.open() }
            }
            MenuSeparator {}

            TibiaMenu {
                title: "Border Options"
                Action {
                    text: "Border Automagic"; shortcut: "A"
                    checkable: true; checked: mapView.automagic
                    onTriggered: mapView.automagic = !mapView.automagic
                }
                MenuSeparator {}
                Action {
                    text: "Borderize Selection"; shortcut: "Ctrl+B"
                    enabled: mapView.selectionCount > 0
                    onTriggered: mapView.borderizeSelection()
                }
                Action {
                    text: "Borderize Map"
                    enabled: otbmReader.loaded
                    onTriggered: borderizeMapConfirm.open()
                }
                Action {
                    text: "Randomize Selection"
                    enabled: mapView.selectionCount > 0
                    onTriggered: mapView.randomizeSelection()
                }
                Action {
                    text: "Randomize Map"
                    enabled: otbmReader.loaded
                    onTriggered: randomizeMapConfirm.open()
                }
            }

            TibiaMenu {
                title: "Other Options"
                Action {
                    text: "Remove Items by ID…"
                    enabled: otbmReader.loaded
                    onTriggered: { selItemDialog.mode = "remove"; selItemDialog.scope = "map"; selItemDialog.open() }
                }
            }
            MenuSeparator {}

            Action {
                text: "Go to Previous Position"; shortcut: "P"
                enabled: mapView.hasPreviousPosition()
                onTriggered: mapView.goToPreviousPosition()
            }
            Action {
                text: "Go to Position…"; shortcut: "Ctrl+G"
                enabled: otbmReader.loaded
                onTriggered: gotoPosDialog.open()
            }
            MenuSeparator {}

            Action {
                text: "Cut"; shortcut: "Ctrl+X"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.cutSelection()
            }
            Action {
                text: "Copy"; shortcut: "Ctrl+C"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.copySelection()
            }
            Action {
                text: "Paste"; shortcut: "Ctrl+V"
                enabled: mapView.hasClipboard
                onTriggered: mapView.startPasting()
            }
        }

        // Menu "Map" wzorowane na RME (Edit Towns / Properties / Statistics).
        TibiaMenu {
            title: "Map"
            Action { text: "Edit Towns"; shortcut: "Ctrl+T"; enabled: otbmReader.loaded; onTriggered: townsDialog.open() }
            // Profil klienta BIEZACEJ mapy (np. czyste 10.98 vs "Midhem"). Wybor
            // jest zapamietywany per mapa - kolejne otwarcia wracaja na ten profil,
            // a palety/brushe zapisuja sie do data/<profil>/.
            TibiaMenu {
                id: mapProfileMenu
                title: "Profil klienta"
                Instantiator {
                    model: root.configuredProfileKeys()
                    delegate: TibiaMenuItem {
                        required property string modelData
                        text: root.profileLabel(modelData)
                        checkable: true
                        checked: root.loadedClientKey === modelData
                        onTriggered: root.switchMapProfile(modelData)
                    }
                    onObjectAdded: (index, object) => mapProfileMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => mapProfileMenu.removeItem(object)
                }
            }
            Action { text: "Edit Items"; enabled: false }        // jak w RME: wyszarzone
            Action { text: "Edit Monsters"; enabled: false }
            MenuSeparator {}
            Action { text: "Go To Position…"; shortcut: "Ctrl+G"; enabled: otbmReader.loaded; onTriggered: gotoPosDialog.open() }
            MenuSeparator {}
            Action { text: "Cleanup…"; enabled: false }          // TODO (destrukcyjne)
            Action { text: "Properties…"; shortcut: "Ctrl+P"; enabled: otbmReader.loaded; onTriggered: mapPropsDialog.open() }
            Action { text: "Statistics"; shortcut: "F8"; enabled: otbmReader.loaded; onTriggered: statsDialog.open() }
        }

        // Menu "Select" - operacje na zaznaczeniu (jak RME). Wszystko wymaga zaznaczenia.
        TibiaMenu {
            title: "Select"
            Action {
                text: "Replace Items on Selection…"
                enabled: mapView.selectionCount > 0
                onTriggered: { selItemDialog.mode = "replace"; selItemDialog.scope = "selection"; selItemDialog.open() }
            }
            Action {
                text: "Find Item on Selection…"
                enabled: mapView.selectionCount > 0
                onTriggered: { selItemDialog.mode = "find"; selItemDialog.scope = "selection"; selItemDialog.open() }
            }
            Action {
                text: "Remove Item on Selection…"
                enabled: mapView.selectionCount > 0
                onTriggered: { selItemDialog.mode = "remove"; selItemDialog.scope = "selection"; selItemDialog.open() }
            }
            MenuSeparator {}

            // Selection Mode (jak RME): ktore pietra zaznacza box-select + kompensacja
            // ukosnej projekcji pieter. Radio przez checked na wykluczajacych sie opcjach.
            TibiaMenu {
                title: "Selection Mode"
                Action {
                    text: "Compensate Selection"
                    checkable: true; checked: mapView.compensatedSelect
                    onTriggered: mapView.compensatedSelect = !mapView.compensatedSelect
                }
                MenuSeparator {}
                Action {
                    text: "Current Floor"
                    checkable: true; checked: mapView.selectionFloors === 0
                    onTriggered: mapView.selectionFloors = 0
                }
                Action {
                    text: "Lower Floors"
                    checkable: true; checked: mapView.selectionFloors === 1
                    onTriggered: mapView.selectionFloors = 1
                }
                Action {
                    text: "Visible Floors"
                    checkable: true; checked: mapView.selectionFloors === 2
                    onTriggered: mapView.selectionFloors = 2
                }
            }
            MenuSeparator {}
            Action {
                text: "Borderize Selection"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.borderizeSelection()
            }
            Action {
                text: "Randomize Selection"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.randomizeSelection()
            }
            MenuSeparator {}
            Action {
                text: "Clear Selection"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.clearSelection()
            }
        }

        // Menu "View" - zoom jak w RME (Ctrl+ / Ctrl- / Ctrl+0) + przelaczniki widoku
        // i ustawienia rzadko zmieniane (limit FPS, historia undo).
        TibiaMenu {
            title: "Tools"
            // Wizualny edytor ground/wall brushy (DnD z palety) - zapis do
            // data/<profil>/brushes.json. Wymaga zaladowanego klienta (ikony).
            Action {
                text: "Brush Editor…"
                enabled: otbReader.loaded
                onTriggered: brushEditorDialog.open()
            }
        }

        TibiaMenu {
            title: "View"
            Action { text: "Zoom In";     shortcut: "Ctrl++"; enabled: otbmReader.loaded; onTriggered: mapView.zoomSteps(1) }
            Action { text: "Zoom Out";    shortcut: "Ctrl+-"; enabled: otbmReader.loaded; onTriggered: mapView.zoomSteps(-1) }
            Action { text: "Zoom Normal"; shortcut: "Ctrl+0"; enabled: otbmReader.loaded; onTriggered: mapView.tileSize = 32 }
            MenuSeparator {}
            Action {
                text: "Show shade"; shortcut: "Q"
                checkable: true; checked: mapView.showShade
                onTriggered: mapView.showShade = !mapView.showShade
            }
            Action {
                text: "Show lower floors"; shortcut: "Ctrl+W"
                checkable: true; checked: mapView.showLowerFloors
                onTriggered: mapView.showLowerFloors = !mapView.showLowerFloors
            }
            Action {
                text: "Efekt przy stawianiu"
                checkable: true; checked: mapView.placeEffect
                onTriggered: mapView.placeEffect = !mapView.placeEffect
            }
            MenuSeparator {}
            // Przelaczniki warstw (RME menu "Show"). Skroty jak RME: Shift+G/F/S/
            // Ctrl+H/E. Domyslnie wszystko widoczne, siatka wylaczona.
            Action {
                text: "Show grid"; shortcut: "Shift+G"
                checkable: true; checked: mapView.showGrid
                onTriggered: mapView.showGrid = !mapView.showGrid
            }
            // UWAGA: F/S/E celowo BEZ shortcut - globalny skrot jednoliterowy odpalal
            // sie tez podczas PISANIA w szukajce palety (wpisanie "stone" wylaczalo
            // spawny i strefy!). Klawisze obsluguje MapView::keyPressEvent - dzialaja
            // tylko z focusem na mapie (hint w nawiasie).
            Action {
                text: "Show creatures  (F)"
                checkable: true; checked: mapView.showCreatures
                onTriggered: mapView.showCreatures = !mapView.showCreatures
            }
            Action {
                text: "Show spawns  (S)"
                checkable: true; checked: mapView.showSpawns
                onTriggered: mapView.showSpawns = !mapView.showSpawns
            }
            Action {
                text: "Show houses"; shortcut: "Ctrl+H"
                checkable: true; checked: mapView.showHouses
                onTriggered: mapView.showHouses = !mapView.showHouses
            }
            Action {
                text: "Show special (strefy)  (E)"
                checkable: true; checked: mapView.showZones
                onTriggered: mapView.showZones = !mapView.showZones
            }
            Action {
                text: "Always show zones"
                checkable: true; checked: mapView.showZonesAlways
                onTriggered: mapView.showZonesAlways = !mapView.showZonesAlways
            }
            MenuSeparator {}
            // Rozmiar kafelkow palety (jak RME Icon Size). Radio przez checked.
            TibiaMenu {
                title: "Icon Size"
                Action {
                    text: "Small"
                    checkable: true; checked: prefs.iconSize === 50
                    onTriggered: prefs.iconSize = 50
                }
                Action {
                    text: "Medium"
                    checkable: true; checked: prefs.iconSize === 66
                    onTriggered: prefs.iconSize = 66
                }
                Action {
                    text: "Large"
                    checkable: true; checked: prefs.iconSize === 88
                    onTriggered: prefs.iconSize = 88
                }
            }
            MenuSeparator {}
            Action { text: "Motyw UI…"; onTriggered: themeDialog.open() }
            MenuSeparator {}

            // Limit FPS renderera OpenGL (0 = bez limitu). Zapisywany w Settings.
            // Instantiator (nie Repeater!) - pozycje menu nie maja wizualnego rodzica.
            TibiaMenu {
                id: fpsMenu
                title: "Limit FPS"
                Instantiator {
                    model: [0, 30, 60, 120, 144, 240]
                    delegate: TibiaMenuItem {
                        required property int modelData
                        text: modelData === 0 ? "Bez limitu" : (modelData + " FPS")
                        checkable: true
                        checked: mapGl.maxFps === modelData
                        onTriggered: { mapGl.maxFps = modelData; prefs.glMaxFps = modelData }
                    }
                    onObjectAdded: (index, object) => fpsMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => fpsMenu.removeItem(object)
                }
            }
            // Limit historii cofniec (Undo).
            TibiaMenu {
                id: undoMenu
                title: "Undo max"
                Instantiator {
                    model: [100, 500, 1000, 5000]
                    delegate: TibiaMenuItem {
                        required property int modelData
                        text: modelData + " krokow"
                        onTriggered: otbmReader.setUndoLimit(modelData)
                    }
                    onObjectAdded: (index, object) => undoMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => undoMenu.removeItem(object)
                }
            }
        }
    }

    // Alias skrotu dla "Save As" - RME uzywa Ctrl+Alt+S, zostawiamy oba dzialajace.
    Shortcut {
        sequence: "Ctrl+Alt+S"
        enabled: otbmReader.loaded
        onActivated: saveDialog.open()
    }
    // Alias skrotu Zoom In dla klawiatur, gdzie "+" wymaga Ctrl+Shift+= (US layout).
    Shortcut {
        sequence: "Ctrl+="
        enabled: otbmReader.loaded
        onActivated: mapView.zoomSteps(1)
    }
    // Alias Redo: menu ma Ctrl+Shift+Z (jak RME), ale Ctrl+Y to powszechna konwencja.
    Shortcut {
        sequence: "Ctrl+Y"
        enabled: otbmReader.redoCount > 0
        onActivated: mapView.redo()
    }

    // Potwierdzenia dla operacji na CALEJ mapie (jak RME - to duza, hurtowa zmiana;
    // u nas cofalna Ctrl+Z, ale i tak lepiej zapytac).
    TibiaConfirmDialog {
        id: borderizeMapConfirm
        title: "Borderize Map"
        message: "Przeliczyc auto-bordery na CALYM biezacym pietrze?"
        onAccepted: mapView.borderizeMap()
    }
    TibiaConfirmDialog {
        id: randomizeMapConfirm
        title: "Randomize Map"
        message: "Wylosowac na nowo warianty gruntu na CALYM biezacym pietrze?"
        onAccepted: mapView.randomizeMap()
    }

    // -------------------------------------------------------------------------
    // Lewa paleta - wszystkie itemy z items.otb (GridView)
    // -------------------------------------------------------------------------
    // Lewa paleta itemow/brushy - patrz PalettePanel.qml.
    PalettePanel {
        id: palette
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.bottomMargin: 6
        width: prefs.paletteCollapsed ? 0
               : Math.max(160, Math.min(prefs.paletteWidth, root.width - 300))
        visible: !prefs.paletteCollapsed
        app: root
        mapCtrl: mapView
    }

    // Uchwyt do zmiany szerokosci panelu palety (przeciagniecie = resize, jak w RME).
    // GridView w PalettePanel reflowuje kolumny samoczynnie (cellWidth stale, width
    // dynamiczne), wiec nie trzeba nic robic po stronie panelu poza zmiana szerokosci.
    // Ukryty gdy panel schowany - nie ma czego rozciagac.
    Item {
        id: paletteSplitter
        anchors.top: palette.top
        anchors.bottom: palette.bottom
        anchors.left: palette.right
        width: 6
        z: 10
        visible: !prefs.paletteCollapsed

        Rectangle {
            anchors.centerIn: parent
            width: 1; height: parent.height
            color: splitterArea.containsMouse || splitterArea.pressed ? "#4a90e2" : "transparent"
        }

        MouseArea {
            id: splitterArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            property real startX: 0
            property int startWidth: 0
            // mouse.x jest lokalny dla splittera, ktory sam sie przesuwa (anchors.left:
            // palette.right) w reakcji na zmiane szerokosci - kazda klatka drag liczylaby
            // sie wiec od innego punktu odniesienia (efekt "ciecia"). Mapujemy na
            // contentItem okna, ktory stoi w miejscu (root to Window - nie ma mapToItem).
            onPressed: (mouse) => {
                startX = mapToItem(root.contentItem, mouse.x, 0).x
                startWidth = palette.width
            }
            onPositionChanged: (mouse) => {
                if (pressed)
                    prefs.paletteWidth = startWidth
                        + (mapToItem(root.contentItem, mouse.x, 0).x - startX)
            }
        }
    }

    // Zwijacz panelu palety - plywajacy przycisk NAD mapa (nie wewnatrz panelu,
    // panel ma calkiem znikac po zwinieciu - width 0). Prosty glyph (trojkat
    // Unicode) na plaskim tle, jak przyciski okna (—/□/✕ w titleBar) - bez
    // atlasu/rotacji tekstury, ktore dawaly artefakty. Wysrodkowany w pionie
    // wzgledem palety (dziala tez przy width 0 - Item ma wciaz wysokosc/y).
    // Rozwiniety: przy prawej krawedzi panelu, strzalka w lewo (klik = zwin).
    // Schowany: przy lewej krawedzi ekranu, strzalka w prawo (klik = rozwin).
    Item {
        id: paletteToggle
        // Szerokosc = dokladnie luka miedzy panelem a mapa (mapPanel.left =
        // palette.right + splitter(6) + margin(4) = +10), wiec przycisk wypelnia
        // ja rowno, bez wystawania na zaden panel.
        width: 10; height: 40
        anchors.verticalCenter: palette.verticalCenter
        // Wypelnia luke miedzy paleta a mapa. Schowany: panel znika, przycisk
        // siedzi przy lewej krawedzi okna, zeby dalo sie rozwinac.
        x: prefs.paletteCollapsed ? 2 : (palette.x + palette.width)
        z: 20

        Rectangle {
            anchors.fill: parent
            color: toggleArea.containsMouse ? "#3a3a3a" : "#242424"
            border.color: "#4a4a4a"
            border.width: 1
        }
        Text {
            anchors.centerIn: parent
            text: prefs.paletteCollapsed ? "▶" : "◀"
            color: "#ccc"
            font.pixelSize: 10
        }
        MouseArea {
            id: toggleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: prefs.paletteCollapsed = !prefs.paletteCollapsed
        }
    }

    // -------------------------------------------------------------------------
    // TOPBAR - pietro, widok, strefy (flagi kafla jak RME), gumka, tryb, pedzel.
    // Ustawienia rzadko zmieniane (Limit FPS / Undo max / toggle'e) siedza w menu View.
    // -------------------------------------------------------------------------
    Item {
        id: toolBar
        anchors.top: titleBar.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        height: otbmReader.loaded ? 40 : 0
        visible: otbmReader.loaded

        TibiaPanel { anchors.fill: parent }

        // Wspolny przycisk paska (toggle albo zwykly) - tlo z classic-UI panel_side.png.
        component TbBtn: Item {
            id: btn
            property string label: ""
            property string tip: ""               // podpowiedz na hover (strefy bez napisow)
            property color dot: "transparent"     // opcjonalna probka koloru (strefy)
            property string iconSource: ""        // opcjonalna ikona (sprite itemu)
            property int iconSize: 26             // rozmiar ikony (px)
            property bool active: false
            property color activeBg: "#2f6f4f"
            property color activeBorder: "#7fdc8f"
            signal clicked()
            width: btnRow.implicitWidth + 24
            // Pelna wysokosc paska (Row jest rozciagniety top-bottom) - przyciski
            // wypelniaja panel i przylegaja do siebie (spacing -1 = wspolna krawedz).
            height: parent ? parent.height : 24

            // DOKLADNIE te same tekstury co karty map (tab_normal/checked/hover,
            // border 2) - tabsq_* rozciagane na wysokosc paska wygladaly za grubo.
            BorderImage {
                anchors.fill: parent
                source: uiTheme.tex + (btn.active ? "tab_checked.png"
                        : (bma.containsMouse ? "tab_hover.png" : "tab_normal.png"))
                smooth: false
                border { left: 2; right: 2; top: 2; bottom: 2 }
            }
            // Kolorowy tint stanu aktywnego NA teksturze (hover ma juz wlasny stan
            // tekstury tabsq_hover). UWAGA: QML uzywa #AARRGGBB - stad Qt.rgba().
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1   // nie zamalowuj krawedzi segmentu
                color: btn.active
                       ? Qt.rgba(btn.activeBg.r, btn.activeBg.g, btn.activeBg.b, 0.45)
                       : "transparent"
            }
            Row {
                id: btnRow
                anchors.centerIn: parent
                spacing: 5
                Rectangle {
                    // Alpha > 0, NIE porownanie ze stringiem: color !== "transparent"
                    // to w QML zawsze true (rozne typy), wiec pusta kratka rysowala
                    // swoj obrys na KAZDYM przycisku (szary kwadracik przy ikonach).
                    visible: btn.dot.a > 0
                    width: 14; height: 14; radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: btn.dot
                    border { width: 1; color: "#20000000" }   // delikatny obrys kratki
                }
                Image {
                    visible: btn.iconSource !== ""
                    width: btn.iconSize; height: btn.iconSize
                    anchors.verticalCenter: parent.verticalCenter
                    // Ikony w zasobach sa w docelowym rozmiarze - rysujemy 1:1, ostro.
                    smooth: false
                    cache: false
                    fillMode: Image.PreserveAspectFit
                    source: btn.iconSource
                }
                Text {
                    visible: btn.label !== ""
                    text: btn.label
                    color: btn.active ? "#eaffea" : "#c0c0c0"
                    font.pixelSize: 12
                    font.bold: btn.active
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                id: bma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: btn.clicked()
            }
            ToolTip.visible: btn.tip !== "" && bma.containsMouse
            ToolTip.delay: 500
            ToolTip.text: btn.tip
        }

        // --- LEWA: strefy, gumka, tryb/pedzel/zaznaczenie ---
        // Rozciagniety na wysokosc paska (1px na bevel panelu) - TbBtn bierze
        // height: parent.height, wiec segmenty wypelniaja pasek i stykaja sie
        // krawedziami (spacing -1 = jedna wspolna linia miedzy przyciskami).
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            spacing: -1

            // --- Strefy (flagi kafla OTBM, 1:1 z RME tile.h) ---
            Repeater {
                model: [
                    { label: "PZ",        flag: 1,  col: "#7fdc8f" },
                    { label: "No PvP",    flag: 4,  col: "#dc8fd0" },
                    { label: "No Logout", flag: 8,  col: "#dcd07f" },
                    { label: "PvP",       flag: 16, col: "#dca57f" }
                ]
                delegate: TbBtn {
                    required property var modelData
                    // Bez napisu - sama kolorowa kratka; nazwa strefy w tooltipie.
                    tip: modelData.label
                    dot: modelData.col
                    active: mapView.activeZone === modelData.flag
                    activeBg: "#3a5a4a"
                    activeBorder: modelData.col
                    onClicked: mapView.activeZone = active ? 0 : modelData.flag
                }
            }

            // --- Gumka (itemy + strefy) ---
            TbBtn {
                label: "Erase"
                active: mapView.eraseMode
                activeBg: "#5a3030"
                activeBorder: "#dc8f8f"
                onClicked: mapView.eraseMode = !mapView.eraseMode
            }

            Rectangle { width: 1; height: 18; color: "#555"; anchors.verticalCenter: parent.verticalCenter }

            // --- Tryb / aktywny pedzel / zaznaczenie ---
            TbBtn {
                label: mapView.selectionMode ? "▣ Selection (Space)" : "✏ Draw (Space)"
                active: true
                activeBg: mapView.selectionMode ? "#2f3a4a" : "#22432f"
                activeBorder: mapView.selectionMode ? "#6aa0dc" : "#7fdc8f"
                onClicked: mapView.toggleSelectionMode()
            }
            TbBtn {
                visible: mapView.brushServerId > 0
                label: otbReader.nameForServerId(mapView.brushServerId) + "  ✕"
                active: true
                onClicked: mapView.brushServerId = 0
            }
            TbBtn {
                visible: mapView.selectionCount > 0
                label: "Clear sel (" + mapView.selectionCount + ")"
                onClicked: mapView.clearSelection()
            }
            Text {
                visible: mapView.pasting || mapView.eraseMode || mapView.activeZone !== 0
                text: mapView.pasting
                      ? "WKLEJANIE — LPM zatwierdza, Esc/PPM anuluje"
                      : (mapView.eraseMode
                         ? (mapView.activeZone !== 0 ? "ERASE: strefa" : "ERASE: itemy")
                         : "Ctrl+LPM = kasuj")
                color: mapView.pasting ? "#6aa0dc" : (mapView.eraseMode ? "#dc8f8f" : "#888")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // --- PRAWA: zarzadzanie pietrami ---
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            // Przelacznik-pochodnia: item 2059 (zapalona) gdy aktywny, 2058 (zgaszona)
            // gdy nie. Sam stan (mapView.torchOn) - funkcja do podpiecia pozniej.
            TbBtn {
                // KWADRAT z sama ikona (bez paddingu tekstowego TbBtn). Jawne wymiary:
                // prawy Row nie jest rozciagniety top-bottom (height: parent.height
                // z TbBtn daloby petle - Row liczy wysokosc z dzieci).
                width: 38
                height: 38
                iconSize: 32
                anchors.verticalCenter: parent.verticalCenter
                active: mapView.torchOn
                tip: "Oswietlenie (podglad nocy)"
                // Statyczne PNG z zasobow (nie sprite'y z .dat/.spr): prostsze, nie
                // zalezy od wczytanego klienta i wygladaja identycznie w kazdej wersji.
                iconSource: mapView.torchOn ? "qrc:/ui/LightON.png" : "qrc:/ui/LightOFF.png"
                onClicked: mapView.torchOn = !mapView.torchOn
            }

            // Animacje itemow (jak RME "Show Animation"). Ikona conditions.png -
            // tymczasowa (najblizsza "ruchowi" z dostepnych; latwo podmienic).
            TbBtn {
                width: 38
                height: 38
                iconSize: 26
                anchors.verticalCenter: parent.verticalCenter
                active: mapView.showAnimations
                tip: "Animacje itemow"
                iconSource: "qrc:/ui/conditions.png"
                onClicked: mapView.showAnimations = !mapView.showAnimations
            }

            // Okno minimapy (jak RME "Minimap", M).
            TbBtn {
                width: 38
                height: 38
                iconSize: 26
                anchors.verticalCenter: parent.verticalCenter
                active: mapView.minimapOn
                tip: "Minimapa"
                iconSource: "qrc:/ui/compass.png"
                onClicked: mapView.minimapOn = !mapView.minimapOn
            }

            Rectangle { width: 1; height: 18; color: "#555"; anchors.verticalCenter: parent.verticalCenter }

            // verticalCenter na kazdym przycisku: wyzszy przycisk pochodni (38px)
            // podbija wysokosc Rowa, a Row dzieci uklada od gory - bez centrowania
            // Center/Floor "uciekaly" do gornej krawedzi.
            TibiaDarkButton {
                label: "Center"; onClicked: mapView.centerOnContent()
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle { width: 1; height: 18; color: "#555"; anchors.verticalCenter: parent.verticalCenter }

            Text {
                text: "Floor"; color: "#999"; font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaDarkButton {
                readOnly: true                 // odczyt, nie przycisk
                width: 26
                label: mapView.floor
                anchors.verticalCenter: parent.verticalCenter
            }
            // "+" = wyzej = MNIEJSZE z (jak skrot '+')
            TibiaDarkButton {
                width: 26; label: "−"; onClicked: mapView.floor = mapView.floor + 1
                anchors.verticalCenter: parent.verticalCenter
            }
            TibiaDarkButton {
                width: 26; label: "+"; onClicked: mapView.floor = mapView.floor - 1
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // -------------------------------------------------------------------------
    // Komunikaty o bledach (nad mapa)
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // Pasek KART MAP (jak RME): kazda otwarta mapa to karta; klik przelacza,
    // "x" zamyka (z pytaniem przy niezapisanych zmianach). Gwiazdka = dirty.
    // Karty siedza NAD panelem mapy (jak RME) - panel zaczyna sie pod nimi.
    // -------------------------------------------------------------------------
    Item {
        id: tabBar
        anchors.top: toolBar.bottom     // bez marginesu - karty przylegaja do topbaru
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        height: root.started ? 22 : 0
        visible: root.started

        Row {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            spacing: 2

            Repeater {
                model: docMgr.tabs
                delegate: Item {
                    required property var modelData
                    required property int index
                    readonly property bool active: index === docMgr.currentIndex
                    width: tabLabel.implicitWidth + 34
                    height: 20

                    BorderImage {
                        anchors.fill: parent
                        source: uiTheme.tex + (parent.active ? "tab_checked.png" : "tab_normal.png")
                        smooth: false
                        border { left: 2; right: 2; top: 2; bottom: 2 }
                    }
                    Text {
                        id: tabLabel
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        text: modelData.title + (modelData.dirty ? " *" : "")
                        color: parent.active ? "#eaffea" : "#c0c0c0"
                        font.pixelSize: 11
                        font.bold: parent.active
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: 18   // strefa "x" ma wlasna
                        onClicked: docMgr.currentIndex = index
                    }
                    Text {
                        id: tabClose
                        anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                        text: "×"
                        color: closeMa.containsMouse ? "#ff8f8f" : "#888"
                        font.pixelSize: 12; font.bold: true
                        MouseArea {
                            id: closeMa
                            anchors.fill: parent
                            anchors.margins: -4   // wieksza strefa klikniecia
                            hoverEnabled: true
                            onClicked: root.closeTab(index)
                        }
                    }
                }
            }
        }
    }

    // Potwierdzenie zamkniecia karty z niezapisanymi zmianami.
    TibiaConfirmDialog {
        id: closeTabConfirm
        property int tabIndex: -1
        onAccepted: root.doCloseTab(tabIndex)
    }

    // -------------------------------------------------------------------------
    // Panel pod obszarem mapy: ta sama tekstura co topbar (TibiaPanel). Zaczyna
    // sie POD kartami; mapa siedzi 3px w glab, wiec panel wystaje jako obramowka.
    // -------------------------------------------------------------------------
    TibiaPanel {
        id: mapPanel
        anchors.top: tabBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        anchors.bottomMargin: 6   // rowno z dolem panelu palety (jej bottomMargin=6)
        visible: root.started
    }

    Column {
        id: errorArea
        anchors.top: tabBar.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2
        topPadding: otbmReader.errorString ? 4 : 0

        Text { visible: sprReader.errorString.length > 0; text: "SPR: " + sprReader.errorString; color: "#ff6b6b"; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
        Text { visible: datReader.errorString.length > 0; text: "DAT: " + datReader.errorString; color: "#ff6b6b"; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
        Text { visible: otbReader.errorString.length > 0; text: "OTB: " + otbReader.errorString; color: "#ff6b6b"; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
        Text { visible: otbmReader.errorString.length > 0; text: "OTBM: " + otbmReader.errorString; color: "#ff6b6b"; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
    }

    // -------------------------------------------------------------------------
    // Podglad mapy OTBM - render sprite'ow biezacego pietra
    // -------------------------------------------------------------------------
    Item {
        id: mapArea
        anchors.top: errorArea.bottom
        anchors.bottom: mapPanel.bottom
        anchors.left: mapPanel.left
        anchors.right: mapPanel.right
        anchors.margins: 3        // rant panelu pod spodem = obramowka (jak tabBar)
        visible: otbmReader.loaded
        clip: true

        // Dane kafelka kliknitego PPM (do menu kontekstowego).
        property var ctx: ({ hasItem: false, serverId: 0, clientId: 0, name: "", groupName: "", x: 0, y: 0, z: 0,
                             creatureName: "", creatureSpawntime: 0, spawnRadius: 0,
                             actionId: 0, uniqueId: 0, text: "", writable: false,
                             teleport: false, hasTeleportDest: false,
                             teleportX: 0, teleportY: 0, teleportZ: 0 })

        MapView {
            id: mapView
            anchors.fill: parent
            focus: true            // odbieraj strzalki (przesuwanie mapy)
            // docMgr.current, NIE context property "otbmReader": zwykly binding na
            // Q_PROPERTY z NOTIFY aktualizuje sie synchronicznie przy zmianie karty,
            // wiec loadEverything moze wolac mapView.loadMap zaraz po newDocument().
            // Context property jest przepinane w main.cpp dla reszty QML (menu/dialogi).
            otbm: docMgr.current
            otb: otbReader
            dat: datReader
            spr: sprReader
            floor: 7
            // Wpina silnik ground brushy - MapView wykrywa wtedy, ze klikniety w palecie
            // item nalezy do ground brusha i maluje z auto-borderami zamiast pojedynczego itemu.
            Component.onCompleted: { setBrushStore(brushStore); setCreatureStore(creatureStore) }
            onContextMenuRequested: (x, y) => {
                mapArea.ctx = mapView.contextInfo()
                ctxMenu.popup(x, y)
            }
        }

        // Renderer mapy: OpenGL z instancingiem (QQuickFramebufferObject). Czyta
        // dane i stan widoku z MapView (ktory obsluguje mysz/klawiature pod spodem).
        MapGLView {
            id: mapGl
            anchors.fill: parent
            source: mapView
            Component.onCompleted: maxFps = prefs.glMaxFps   // wczytaj zapisany limit
        }


        // Menu kontekstowe (PPM na mapie)
        TibiaMenu {
            id: ctxMenu
            // Action (nie jawne MenuItem) - zeby przejsc przez delegate TibiaMenu
            // (stylowany tekst/tlo/padding). Jawne MenuItem omijaja delegate.
            Action { text: "Cut"; enabled: mapView.selectionCount > 0                       // skrot: menu Edit
                onTriggered: mapView.cutSelection() }
            Action { text: "Copy"; enabled: mapView.selectionCount > 0                      // skrot: menu Edit
                onTriggered: mapView.copySelection() }
            Action { text: "Copy Position"
                onTriggered: fileTools.setClipboard(mapArea.ctx.x + ", " + mapArea.ctx.y + ", " + mapArea.ctx.z) }
            Action { text: "Paste"; enabled: mapView.hasClipboard                            // skrot: menu Edit
                onTriggered: mapView.startPasting() }   // podglad pod kursorem, LPM zatwierdza
            Action { text: "Delete"; enabled: mapView.selectionCount > 0
                onTriggered: mapView.deleteSelectedTop() }
            MenuSeparator {}
            Action { text: "Copy Item Server Id"; enabled: mapArea.ctx.hasItem
                onTriggered: fileTools.setClipboard("" + mapArea.ctx.serverId) }
            Action { text: "Copy Item Client Id"; enabled: mapArea.ctx.hasItem
                onTriggered: fileTools.setClipboard("" + mapArea.ctx.clientId) }
            Action { text: "Copy Item Name"; enabled: mapArea.ctx.hasItem
                onTriggered: fileTools.setClipboard(mapArea.ctx.name) }
            MenuSeparator {}
            // Select Brush (jak RME): jesli item nalezy do pedzla (ground/wall/doodad),
            // aktywuj TEN pedzel. useGroundBrush sam wykrywa typ. Wyszarzone dla itemow
            // spoza brushy (jest wtedy Select RAW).
            // TibiaMenuItem (nie Action): pozycje nieadekwatne do kliknietego itemu maja
            // ZNIKAC, a nie wisiec wyszarzone. Action w Menu da sie tylko wylaczyc,
            // wiec te dwie pozycje wstawiamy recznie. height=0 przy visible=false -
            // inaczej ukryta pozycja dalej zajmowalaby wiersz w menu.
            TibiaMenuItem {
                text: "Select Brush"
                visible: mapArea.ctx.hasItem
                         && mapView.brushForServerId(mapArea.ctx.serverId) !== ""
                height: visible ? implicitHeight : 0
                onTriggered: mapView.useGroundBrush(mapArea.ctx.serverId)
            }
            Action { text: "Select RAW"; enabled: mapArea.ctx.hasItem
                onTriggered: mapView.brushServerId = mapArea.ctx.serverId }
            // "Goto Destination" jak w RME (MapCanvas::OnGotoDestination): skacze na
            // cel teleportu, zeby jednym klikiem sprawdzic, dokad prowadzi.
            // centerOnPosition zapamietuje poprzedni srodek, wiec P wraca skad przyszlismy.
            // Widoczne TYLKO dla realnego teleportu z ustawionym celem.
            TibiaMenuItem {
                text: "Go To Destination"
                visible: mapArea.ctx.teleport === true && mapArea.ctx.hasTeleportDest === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.centerOnPosition(mapArea.ctx.teleportX,
                                                      mapArea.ctx.teleportY,
                                                      mapArea.ctx.teleportZ)
            }
            Action { text: "Properties"
                // Takze kafel z potworem/spawnem bez itemow (potwor na pustym kaflu).
                enabled: mapArea.ctx.hasItem || mapArea.ctx.creatureName !== ""
                         || mapArea.ctx.spawnRadius > 0
                onTriggered: propsDialog.open() }
        }

        // Okno minimapy (przycisk z kompasem w topbarze / klawisz M). Plywajacy
        // panel w prawym gornym rogu obszaru mapy - 1 px = 1 kafel (kolko = zoom),
        // klik/przeciaganie centruje glowny widok, biala ramka = widoczny obszar.
        Item {
            visible: mapView.minimapOn
            width: 236
            height: 262
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10

            TibiaPanel { anchors.fill: parent }

            Text {
                id: minimapTitle
                anchors { left: parent.left; top: parent.top; leftMargin: 8; topMargin: 5 }
                text: "Minimap  -  floor " + mapView.floor
                color: "#ddd"; font.pixelSize: 12; font.bold: true
            }
            // Zamkniecie (to samo co ponowne klikniecie kompasu / M).
            Text {
                anchors { right: parent.right; top: parent.top; rightMargin: 8; topMargin: 4 }
                text: "x"; color: mmCloseMa.containsMouse ? "#fff" : "#999"
                font.pixelSize: 13; font.bold: true
                MouseArea {
                    id: mmCloseMa
                    anchors.fill: parent; anchors.margins: -4
                    hoverEnabled: true
                    onClicked: mapView.minimapOn = false
                }
            }

            MinimapView {
                source: mapView
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                anchors { top: minimapTitle.bottom; margins: 6; topMargin: 4 }
            }
        }

        // Licznik FPS (lewy gorny rog)
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 6
            width: fpsLabel.implicitWidth + 12
            height: 20
            radius: 4
            color: "#B0000000";
            Text {
                id: fpsLabel
                anchors.centerIn: parent
                text: "FPS: " + root.fps + "   OpenGL"
                color: root.fps >= 50 ? "#7fdc8f" : (root.fps >= 25 ? "#e0c46a" : "#e08a6a")
                font.pixelSize: 11; font.bold: true
            }
        }

        // Toast po zapisie
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 8
            visible: root.savedToast.length > 0
            width: toastLabel.implicitWidth + 20; height: 26; radius: 5
            color: "#E622432f";
            Text { id: toastLabel; anchors.centerIn: parent; text: root.savedToast; color: "#eaffea"; font.pixelSize: 12 }
        }


        // Pasek statusu - kafelek pod kursorem
        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 8
            visible: mapView.hoverText.length > 0
            width: hoverLabel.implicitWidth + 16
            height: 22
            radius: 4
            color: "#B0000000";
            Text {
                id: hoverLabel
                anchors.centerIn: parent
                text: mapView.hoverText
                color: "#ddd"; font.pixelSize: 11
            }
        }
    }

    // Wybor folderu klienta dla wersji wykrytej z mapy (po starcie, np. File->Open).
    FolderDialog {
        id: versionFolderDialogMain
        title: "Wskaz folder klienta " + root.profileLabel(root.pendingKey)
               + " (Tibia.dat / Tibia.spr / items.otb)"
        onAccepted: root.onVersionFolderPicked(selectedFolder)
    }

    // Zapis mapy "jako" (.otbm)
    FileDialog {
        id: saveDialog
        title: "Save map as .otbm"
        fileMode: FileDialog.SaveFile
        nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
        defaultSuffix: "otbm"
        onAccepted: {
            var p = fileTools.toLocalFile(selectedFile)
            otbmReader.applyClientVersions(loadedClientVersion, otbReader.majorVersion, otbReader.minorVersion)
            if (otbmReader.saveFile(p)) {   // saveFile ustawia filePath i czysci dirty
                root.addRecent(p)
                root.savedToast = "Saved: " + fileTools.fileName(p)
                savedToastTimer.restart()
            }
        }
    }

    // Wlasciwosci itemu (z menu kontekstowego)
    // Wlasciwosci itemu (PPM na mapie) - patrz dialogs/ItemPropertiesDialog.qml.
    ItemPropertiesDialog { id: propsDialog; ctx: mapArea.ctx; mapCtrl: mapView }
    // Tools > Brush Editor (wizualne skladanie ground/wall brushy).
    BrushEditorDialog { id: brushEditorDialog; mapCtrl: mapView }

    // --- Map > Go To Position (Ctrl+G): przeskok na X,Y,Z (jak w RME) ---
    GoToPositionDialog { id: gotoPosDialog; mapCtrl: mapView }

    // --- Map > Statistics (F8): statystyki mapy (jak w RME) ---
    MapStatsDialog { id: statsDialog }

    // --- Map > Properties (Ctrl+P): wlasciwosci mapy (jak w RME) ---
    MapPropertiesDialog { id: mapPropsDialog; app: root }

    // Find / Remove / Replace Items on Selection (menu "Select", jak RME).
    SelectionItemDialog { id: selItemDialog; mapCtrl: mapView }
    ThemeDialog { id: themeDialog }
    NewMapDialog { id: newMapDialog; app: root }

    // --- Map > Edit Towns: lista + Add/Remove + edycja nazwy/pozycji swiatyni (jak RME) ---
    TownsDialog { id: townsDialog; app: root; mapCtrl: mapView }

    // -------------------------------------------------------------------------
    // Ekran startowy (loader) - OSOBNE okno, wybor mapy + folder klienta
    // -------------------------------------------------------------------------
    StartupWindow {
        id: startupScreen
        app: root
        settings: prefs
    }
}
