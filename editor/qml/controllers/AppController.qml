import QtQuick

Item {
    id: controller
    visible: false
    width: 0
    height: 0

    required property var settings
    required property var mapView
    required property var appWindow
    required property var startupWindow
    required property var versionFolderDialog
    required property var itemDataDialog
    required property var saveDialog
    required property var closeTabConfirm
    required property var appCloseConfirm

    property alias started: documents.started
    property alias recentMaps: documents.recentMaps
    property alias pendingMapPath: documents.pendingMapPath
    property alias pendingKey: documents.pendingKey
    property alias pendingItemSource: documents.pendingItemSource
    property alias savedToast: documents.savedToast
    property alias appCloseAllowed: documents.appCloseAllowed
    property alias appCloseSaveAsPending: documents.appCloseSaveAsPending
    property alias recoveringSession: documents.recoveringSession

    function showToast(message) {
        documents.showToast(message)
    }

    property alias clientPaths: profiles.clientPaths
    property alias itemDataPaths: profiles.itemDataPaths
    property alias customProfiles: profiles.customProfiles
    property alias mapProfiles: profiles.mapProfiles
    property alias loadedClientVersion: profiles.loadedClientVersion
    property alias loadedClientKey: profiles.loadedClientKey
    property alias loadedClientFolder: profiles.loadedClientFolder
    property alias loadedItemSource: profiles.loadedItemSource
    property alias loadedItemTomlPath: profiles.loadedItemTomlPath

    property alias customPalettes: palettes.customPalettes
    property alias customPaletteNames: palettes.customPaletteNames
    property alias iconSizePx: palettes.iconSizePx
    property alias recentBrushIds: palettes.recentBrushIds
    property alias favoriteBrushIds: palettes.favoriteBrushIds

    ClientProfileController {
        id: profiles
        settings: controller.settings
        mapView: controller.mapView
    }

    PaletteController {
        id: palettes
        settings: controller.settings
        profileKey: profiles.loadedClientKey
    }

    DocumentController {
        id: documents
        settings: controller.settings
        profiles: profiles
        palettes: palettes
        mapView: controller.mapView
        appWindow: controller.appWindow
        startupWindow: controller.startupWindow
        versionFolderDialog: controller.versionFolderDialog
        itemDataDialog: controller.itemDataDialog
        saveDialog: controller.saveDialog
        closeTabConfirm: controller.closeTabConfirm
        appCloseConfirm: controller.appCloseConfirm
    }

    function initialize() {
        documents.initialize();
    }

    function versionLabel(version) {
        return profiles.versionLabel(version);
    }
    function profileVer(key) {
        return profiles.profileVer(key);
    }
    function profileLabel(key) {
        return profiles.profileLabel(key);
    }
    function allProfileKeys() {
        return profiles.allProfileKeys();
    }
    function isCustomKey(key) {
        return profiles.isCustomKey(key);
    }
    function addCustomProfile(name, base) {
        return profiles.addCustomProfile(name, base);
    }
    function removeCustomProfile(name) {
        profiles.removeCustomProfile(name);
    }
    function clientFiles(folder) {
        return profiles.clientFiles(folder);
    }
    function itemTomlPath(key) {
        return profiles.itemTomlPath(key);
    }
    function setItemDataPath(key, path) {
        profiles.setItemDataPath(key, path);
    }
    function configuredProfileKeys() {
        return profiles.configuredProfileKeys();
    }
    function ensureClientLoaded(reader, preferredKey, preferredSource) {
        return profiles.ensureClientLoaded(reader, preferredKey, preferredSource);
    }
    function ensureClientVersion(key, source) {
        return profiles.ensureClientVersion(key, source);
    }
    function rememberMapProfile(path, key, source) {
        profiles.rememberMapProfile(path, key, source);
    }
    function switchMapProfile(key, source) {
        return profiles.switchMapProfile(key, source);
    }

    function addCustomPalette(name) {
        return palettes.addCustomPalette(name);
    }
    function deleteCustomPalette(name) {
        palettes.deleteCustomPalette(name);
    }
    function addItemToPalette(name, serverId) {
        palettes.addItemToPalette(name, serverId);
    }
    function removeItemFromPalette(name, serverId) {
        palettes.removeItemFromPalette(name, serverId);
    }
    function recordBrushUse(serverId) {
        palettes.recordBrushUse(serverId);
    }
    function isFavoriteBrush(serverId) {
        return palettes.isFavorite(serverId);
    }
    function toggleFavoriteBrush(serverId) {
        palettes.toggleFavorite(serverId);
    }
    function clearRecentBrushes() {
        palettes.clearRecentBrushes();
    }

    function addRecent(path) {
        documents.addRecent(path);
    }
    function createNewMap(key, width, height, source) {
        documents.createNewMap(key, width, height, source);
    }
    function loadEverything(path, preferredKey, source) {
        return documents.loadEverything(path, preferredKey, source);
    }
    function onVersionFolderPicked(folderUrl) {
        documents.onVersionFolderPicked(folderUrl);
    }
    function onItemDataPicked(pathUrl) {
        documents.onItemDataPicked(pathUrl);
    }
    function saveMap() {
        documents.saveMap();
    }
    function handleSaveAsAccepted(fileUrl) {
        documents.handleSaveAsAccepted(fileUrl);
    }
    function handleSaveAsRejected() {
        documents.handleSaveAsRejected();
    }
    function closeTab(index) {
        documents.closeTab(index);
    }
    function doCloseTab(index) {
        documents.doCloseTab(index);
    }
    function requestAppClose() {
        documents.requestAppClose();
    }
    function finishAppClose() {
        documents.finishAppClose();
    }
    function abortSaveAllAndClose(message) {
        documents.abortSaveAllAndClose(message);
    }
    function beginSaveAllAndClose() {
        documents.beginSaveAllAndClose();
    }
    function saveNextAndClose() {
        documents.saveNextAndClose();
    }
    function recoverPreviousSession() {
        documents.recoverPreviousSession();
    }
    function discardPreviousSession() {
        documents.discardPreviousSession();
    }
}
