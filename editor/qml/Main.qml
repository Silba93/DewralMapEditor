pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import Tibia 1.0
import "dialogs"
import "style"
import "controllers"
import "components"

Window {
    id: root
    visible: app.started
    width: 1000
    height: 680
    title: "Dewral Map Editor  -  " + (Backend.otbmReader.filePath !== "" ? Backend.fileTools.fileName(Backend.otbmReader.filePath) : "(no map)") + (Backend.otbmReader.dirty ? "  *" : "")

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    onClosing: function (close) {
        if (app.appCloseAllowed) {
            close.accepted = true;
            return;
        }
        close.accepted = false;
        app.requestAppClose();
    }

    TibiaDialogBackground {
        anchors.fill: parent

        frameSource: (Backend.uiTheme.tex + "popupwindow_tall.png")
        topBorder: 45
    }

    Item {
        id: titleBar
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: 6
            rightMargin: 6
        }

        height: 45

        Text {
            id: titleText

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -5
            text: root.title
            color: "#c0c0c0"
            font.bold: true
            font.pixelSize: 15
            elide: Text.ElideMiddle

            width: Math.max(0, Math.min(implicitWidth, parent.width - 2 * (menuBar.width + 24)))
        }

        Row {
            id: winButtons
            anchors {
                right: parent.right
                rightMargin: 6
                verticalCenter: titleText.verticalCenter
            }
            spacing: 10

            Text {
                text: "_"
                color: minArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13
                font.bold: true
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

                text: root.visibility === Window.Maximized ? "[]" : "[ ]"
                color: maxArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13
                font.bold: true
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
                text: "X"
                color: closeArea.containsMouse ? "#eaffea" : "#999"
                font.pixelSize: 13
                font.bold: true
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.requestAppClose()
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 70
            onPressed: root.startSystemMove()

            onDoubleClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
        }
    }

    property int frameCount: 0
    property int fps: 0
    onFrameSwapped: frameCount++
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            root.fps = root.frameCount;
            root.frameCount = 0;
        }
    }

    AppSettings {
        id: prefs
    }

    AppController {
        id: app
        settings: prefs
        mapView: workspace.mapView
        appWindow: root
        startupWindow: startupScreen
        versionFolderDialog: versionFolderDialogMain
        saveDialog: saveDialog
        closeTabConfirm: closeTabConfirm
        appCloseConfirm: appCloseConfirm
    }

    Component.onCompleted: app.initialize()

    MainMenuBar {
        id: menuBar
        appController: app
        mapView: workspace.mapView
        mapGl: workspace.mapGl
        settings: prefs
        titleBarItem: titleBar
        startupWindow: startupScreen
        saveDialog: saveDialog
        newMapDialog: newMapDialog
        selectionItemDialog: selItemDialog
        goToDialog: gotoPosDialog
        townsDialog: townsDialog
        mapPropertiesDialog: mapPropsDialog
        statsDialog: statsDialog
        brushEditorDialog: brushEditorDialog
        themeDialog: themeDialog
        borderizeConfirm: borderizeMapConfirm
        randomizeConfirm: randomizeMapConfirm
    }

    Shortcut {
        sequence: "Ctrl+Alt+S"
        enabled: Backend.otbmReader.loaded
        onActivated: saveDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+="
        enabled: Backend.otbmReader.loaded
        onActivated: workspace.mapView.zoomSteps(1)
    }

    Shortcut {
        sequence: "Ctrl+Y"
        enabled: Backend.otbmReader.redoCount > 0
        onActivated: workspace.mapView.redo()
    }

    TibiaConfirmDialog {
        id: borderizeMapConfirm
        title: "Borderize Map"
        message: "Recalculate auto-borders on the entire current floor?"
        onAccepted: workspace.mapView.borderizeMap()
    }
    TibiaConfirmDialog {
        id: randomizeMapConfirm
        title: "Randomize Map"
        message: "Randomize ground variants on the entire current floor?"
        onAccepted: workspace.mapView.randomizeMap()
    }

    PalettePanel {
        id: palette
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.bottomMargin: 6
        width: prefs.paletteCollapsed ? 0 : Math.max(160, Math.min(prefs.paletteWidth, root.width - 300))
        visible: !prefs.paletteCollapsed
        app: app
        mapCtrl: workspace.mapView
    }

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
            width: 1
            height: parent.height
            color: splitterArea.containsMouse || splitterArea.pressed ? "#4a90e2" : "transparent"
        }

        MouseArea {
            id: splitterArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            property real startX: 0
            property int startWidth: 0

            onPressed: mouse => {
                startX = mapToItem(root.contentItem, mouse.x, 0).x;
                startWidth = palette.width;
            }
            onPositionChanged: mouse => {
                if (pressed)
                    prefs.paletteWidth = startWidth + (mapToItem(root.contentItem, mouse.x, 0).x - startX);
            }
        }
    }

    Item {
        id: paletteToggle

        width: 10
        height: 40
        anchors.verticalCenter: palette.verticalCenter

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
            text: prefs.paletteCollapsed ? ">" : "<"
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

    EditorToolBar {
        id: toolBar
        anchors.top: titleBar.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        mapView: workspace.mapView
    }

    DocumentTabs {
        id: tabBar
        anchors.top: toolBar.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        height: app.started ? 22 : 0
        visible: app.started
        app: app
    }

    TibiaConfirmDialog {
        id: closeTabConfirm
        property int tabIndex: -1
        onAccepted: app.doCloseTab(tabIndex)
    }

    TibiaDialog {
        id: appCloseConfirm
        title: "Unsaved maps"
        property string message: ""
        width: 460

        contentItem: Column {
            spacing: 12
            Text {
                width: appCloseConfirm.width - 24
                text: appCloseConfirm.message
                color: "#c0c0c0"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                TibiaButton {
                    text: "Save all"
                    width: 130
                    onClicked: {
                        appCloseConfirm.close();
                        app.beginSaveAllAndClose();
                    }
                }
                TibiaButton {
                    text: "Discard all"
                    width: 110
                    onClicked: {
                        appCloseConfirm.close();
                        app.finishAppClose();
                    }
                }
                TibiaButton {
                    text: "Cancel"
                    width: 90
                    onClicked: appCloseConfirm.close()
                }
            }
        }
    }

    MapWorkspace {
        id: workspace
        anchors.top: tabBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8
        anchors.bottomMargin: 6
        visible: app.started
        app: app
        settings: prefs
        propertiesDialog: propsDialog
        fps: root.fps
    }

    FolderDialog {
        id: versionFolderDialogMain
        title: "Select client folder for " + app.profileLabel(app.pendingKey) + " (Tibia.dat / Tibia.spr / items.otb)"
        onAccepted: app.onVersionFolderPicked(selectedFolder)
    }

    FileDialog {
        id: saveDialog
        title: "Save map as .otbm"
        fileMode: FileDialog.SaveFile
        nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
        defaultSuffix: "otbm"
        onAccepted: app.handleSaveAsAccepted(selectedFile)
        onRejected: app.handleSaveAsRejected()
    }

    QtObject {
        id: propsDialog
        function open() {
            propsDialogLoader.active = true;
            propsDialogLoader.item["open"]();
        }
    }
    Loader {
        id: propsDialogLoader
        active: false
        sourceComponent: ItemPropertiesDialog {
            ctx: workspace.context
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => propsDialogLoader.active = false)
        }
    }

    QtObject {
        id: brushEditorDialog
        function open() {
            brushEditorLoader.active = true;
            brushEditorLoader.item["open"]();
        }
    }
    Loader {
        id: brushEditorLoader
        active: false
        sourceComponent: BrushEditorDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => brushEditorLoader.active = false)
        }
    }

    QtObject {
        id: gotoPosDialog
        function open() {
            gotoPosLoader.active = true;
            gotoPosLoader.item["open"]();
        }
    }
    Loader {
        id: gotoPosLoader
        active: false
        sourceComponent: GoToPositionDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => gotoPosLoader.active = false)
        }
    }

    QtObject {
        id: statsDialog
        function open() {
            statsLoader.active = true;
            statsLoader.item["open"]();
        }
    }
    Loader {
        id: statsLoader
        active: false
        sourceComponent: MapStatsDialog {
            onClosed: Qt.callLater(() => statsLoader.active = false)
        }
    }

    QtObject {
        id: mapPropsDialog
        function open() {
            mapPropsLoader.active = true;
            mapPropsLoader.item["open"]();
        }
    }
    Loader {
        id: mapPropsLoader
        active: false
        sourceComponent: MapPropertiesDialog {
            app: app
            onClosed: Qt.callLater(() => mapPropsLoader.active = false)
        }
    }

    QtObject {
        id: selItemDialog
        property string mode: "find"
        property string scope: "map"
        function open() {
            selectionItemLoader.active = true;
            selectionItemLoader.item["mode"] = mode;
            selectionItemLoader.item["scope"] = scope;
            selectionItemLoader.item["open"]();
        }
    }
    Loader {
        id: selectionItemLoader
        active: false
        sourceComponent: SelectionItemDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => selectionItemLoader.active = false)
        }
    }

    QtObject {
        id: themeDialog
        function open() {
            themeLoader.active = true;
            themeLoader.item["open"]();
        }
    }
    Loader {
        id: themeLoader
        active: false
        sourceComponent: ThemeDialog {
            onClosed: Qt.callLater(() => themeLoader.active = false)
        }
    }

    QtObject {
        id: newMapDialog
        function open() {
            newMapLoader.active = true;
            newMapLoader.item["open"]();
        }
    }
    Loader {
        id: newMapLoader
        active: false
        sourceComponent: NewMapDialog {
            app: app
            onClosed: Qt.callLater(() => newMapLoader.active = false)
        }
    }

    QtObject {
        id: townsDialog
        function open() {
            townsLoader.active = true;
            townsLoader.item["open"]();
        }
    }
    Loader {
        id: townsLoader
        active: false
        sourceComponent: TownsDialog {
            app: app
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => townsLoader.active = false)
        }
    }

    QtObject {
        id: startupScreen
        function ensureWindow() {
            startupLoader.active = true;
            return startupLoader.item;
        }
        function openMapDialog() {
            ensureWindow().openMapDialog();
        }
        function openVersionFolderDialog() {
            ensureWindow().openVersionFolderDialog();
        }
    }
    Loader {
        id: startupLoader
        active: !app.started
        sourceComponent: StartupWindow {
            app: app
            settings: prefs
        }
    }
}
