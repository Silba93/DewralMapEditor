import QtQml
import Tibia 1.0

QtObject {
    id: controller
    required property var settings
    required property var mapView

    property var clientPaths: ({})
    property var itemDataPaths: ({})
    property var customProfiles: []
    property var mapProfiles: ({})
    readonly property var knownVersions: [760, 772, 780, 792, 800, 810, 820, 840, 850, 854, 860, 870, 910, 920, 946, 954, 960, 986, 1010, 1030, 1041, 1077, 1098]
    property int loadedClientVersion: 0
    property string loadedClientKey: ""
    property string loadedClientFolder: ""
    property string loadedItemSource: "standard"
    property string loadedItemTomlPath: ""
    property string lastLoadError: ""

    function versionLabel(version) {
        if (version >= 10100)
            return "10.100";
        return Math.floor(version / 100) + "." + ("0" + (version % 100)).slice(-2);
    }

    function profileVer(key) {
        var numeric = Number(key);
        if (!isNaN(numeric) && numeric > 0)
            return numeric;
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name === key)
                return customProfiles[i].base;
        return 0;
    }

    function profileLabel(key) {
        var numeric = Number(key);
        if (!isNaN(numeric) && numeric > 0)
            return versionLabel(numeric);
        return key + "  (" + versionLabel(profileVer(key)) + ")";
    }

    function allProfileKeys() {
        return knownVersions.map(function (version) {
            return String(version);
        }).concat(customProfiles.map(function (profile) {
            return profile.name;
        }));
    }

    function isCustomKey(key) {
        return isNaN(Number(key)) || Number(key) <= 0;
    }

    function addCustomProfile(name, base) {
        name = (name || "").trim();
        if (name === "" || !isNaN(Number(name)))
            return false;
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name.toLowerCase() === name.toLowerCase())
                return false;
        var copy = customProfiles.slice();
        copy.push({
            name: name,
            base: base
        });
        customProfiles = copy;
        settings.customProfilesJson = JSON.stringify(customProfiles);
        return true;
    }

    function removeCustomProfile(name) {
        customProfiles = customProfiles.filter(function (profile) {
            return profile.name !== name;
        });
        settings.customProfilesJson = JSON.stringify(customProfiles);
        var paths = JSON.parse(JSON.stringify(clientPaths));
        delete paths[name];
        clientPaths = paths;
        saveClientPaths();
        var itemPaths = JSON.parse(JSON.stringify(itemDataPaths));
        delete itemPaths[name];
        itemDataPaths = itemPaths;
        saveItemDataPaths();
    }

    function load() {
        try {
            clientPaths = JSON.parse(settings.clientPathsJson) || ({});
        } catch (e) {
            clientPaths = ({});
        }
        try {
            itemDataPaths = JSON.parse(settings.itemDataPathsJson) || ({});
        } catch (e) {
            itemDataPaths = ({});
        }
        try {
            customProfiles = JSON.parse(settings.customProfilesJson) || [];
        } catch (e) {
            customProfiles = [];
        }
        try {
            mapProfiles = JSON.parse(settings.mapProfilesJson) || ({});
        } catch (e) {
            mapProfiles = ({});
        }

        var migratedProfiles = ({});
        Object.keys(mapProfiles).forEach(function (path) {
            var value = mapProfiles[path];
            var canonicalPath = Backend.fileTools.canonicalPath(path) || path;
            migratedProfiles[canonicalPath] = typeof value === "string"
                ? { profileKey: value, itemSource: "standard" } : {
                    profileKey: value.profileKey || "",
                    itemSource: value.itemSource === "blacktek" ? "blacktek" : "standard"
                };
        });
        mapProfiles = migratedProfiles;
        settings.mapProfilesJson = JSON.stringify(mapProfiles);

        if (Object.keys(clientPaths).length === 0 && settings.clientFolder !== "") {
            var paths = ({});
            paths["772"] = settings.clientFolder;
            clientPaths = paths;
            saveClientPaths();
        }
    }

    function saveClientPaths() {
        settings.clientPathsJson = JSON.stringify(clientPaths);
    }

    function saveItemDataPaths() {
        settings.itemDataPathsJson = JSON.stringify(itemDataPaths);
    }

    function setVersionFolder(key, folder) {
        var copy = JSON.parse(JSON.stringify(clientPaths));
        copy[String(key)] = folder;
        clientPaths = copy;
        saveClientPaths();
    }

    function setItemDataPath(key, path) {
        var copy = JSON.parse(JSON.stringify(itemDataPaths));
        copy[String(key)] = path || "";
        itemDataPaths = copy;
        saveItemDataPaths();
    }

    function loadProfileData(key, itemSource) {
        var version = profileVer(key);
        if (!isCustomKey(key)) {
            Backend.tilesetStore.loadForVersion(version);
            Backend.brushStore.loadForVersion(version);
            Backend.creatureStore.loadForVersion(version);
            if (itemSource === "blacktek")
                Backend.itemsXml.clear();
            else
                Backend.itemsXml.loadForVersion(version);
            return;
        }
        Backend.tilesetStore.loadForDir(key);
        if (!Backend.brushStore.loadForDir(key))
            Backend.brushStore.loadForVersion(version);
        if (!Backend.creatureStore.loadForDir(key))
            Backend.creatureStore.loadForVersion(version);
        if (itemSource === "blacktek")
            Backend.itemsXml.clear();
        else if (!Backend.itemsXml.loadForDir(key))
            Backend.itemsXml.loadForVersion(version);
    }

    function clientFiles(folder) {
        if (!folder)
            return {
                dat: "",
                spr: "",
                otb: ""
            };
        return {
            dat: Backend.fileTools.findByExt(folder, "dat", "Tibia.dat"),
            spr: Backend.fileTools.findByExt(folder, "spr", "Tibia.spr"),
            otb: Backend.fileTools.findByExt(folder, "otb", "items.otb")
        };
    }

    function itemTomlPath(key) {
        var configured = itemDataPaths[String(key)] || "";
        return Backend.fileTools.findToml(configured);
    }

    function normalizeItemSource(source) {
        return String(source || "").toLowerCase() === "blacktek" ? "blacktek" : "standard";
    }

    function mapProfileFor(path) {
        var canonical = Backend.fileTools.canonicalPath(path);
        if (!canonical || !mapProfiles[canonical]) return null;
        var value = mapProfiles[canonical];
        return typeof value === "string"
            ? { profileKey: value, itemSource: "standard" } : value;
    }

    function rememberMapProfile(mapPath, key, itemSource) {
        if (mapPath === "" || key === "")
            return;
        mapPath = Backend.fileTools.canonicalPath(mapPath);
        var source = normalizeItemSource(itemSource || (mapProfileFor(mapPath) || {}).itemSource);
        var current = mapProfileFor(mapPath);
        if (current && current.profileKey === String(key) && current.itemSource === source)
            return;
        var copy = JSON.parse(JSON.stringify(mapProfiles));
        copy[mapPath] = { profileKey: String(key), itemSource: source };
        mapProfiles = copy;
        settings.mapProfilesJson = JSON.stringify(mapProfiles);
    }

    function switchMapProfile(key, itemSource) {
        if (itemSource === undefined || itemSource === null || String(itemSource) === "")
            itemSource = Backend.docMgr.currentItemSource;
        itemSource = normalizeItemSource(itemSource);
        if (!ensureClientVersion(key, itemSource))
            return false;
        Backend.docMgr.setCurrentItemSource(itemSource);
        if (Backend.otbmReader.filePath !== "")
            rememberMapProfile(Backend.otbmReader.filePath, key, itemSource);
        return true;
    }

    function configuredProfileKeys() {
        var keys = Object.keys(clientPaths).filter(function (key) {
            return (clientPaths[key] || "") !== "";
        });
        keys.sort(function (a, b) {
            var na = Number(a), nb = Number(b);
            var ca = isNaN(na), cb = isNaN(nb);
            if (ca !== cb)
                return ca ? 1 : -1;
            return ca ? a.localeCompare(b) : na - nb;
        });
        return keys;
    }

    function resolveKeyForVersion(version) {
        if ((clientPaths[String(version)] || "") !== "")
            return String(version);
        for (var i = 0; i < customProfiles.length; ++i) {
            var profile = customProfiles[i];
            if (profile.base === version && (clientPaths[profile.name] || "") !== "")
                return profile.name;
        }
        return String(version);
    }

    function ensureClientLoaded(reader, preferredProfileKey, preferredItemSource) {
        var version = reader.suggestedClientVersion();
        if (version <= 0)
            version = 772;
        var preferred = preferredProfileKey === undefined || preferredProfileKey === null ? "" : String(preferredProfileKey);
        var compatible = preferred !== "" && profileVer(preferred) === version;
        var remembered = reader.filePath !== "" ? mapProfileFor(reader.filePath) : null;
        var rememberedKey = remembered ? remembered.profileKey : "";
        var key = compatible ? preferred : (rememberedKey !== "" && (clientPaths[rememberedKey] || "") !== "" ? rememberedKey : resolveKeyForVersion(version));
        var source = preferredItemSource !== undefined && preferredItemSource !== null && String(preferredItemSource) !== ""
            ? normalizeItemSource(preferredItemSource)
            : remembered ? normalizeItemSource(remembered.itemSource) : normalizeItemSource(reader.itemSource);
        return ensureClientVersion(key, source);
    }

    function ensureClientVersion(key, itemSource) {
        key = String(key);
        itemSource = normalizeItemSource(itemSource);
        var version = profileVer(key);
        var folder = clientPaths[key] || "";
        var files = clientFiles(folder);
        var tomlPath = itemTomlPath(key);
        lastLoadError = "";
        if (version <= 0 || folder === "" || !files.dat || !files.spr) {
            lastLoadError = "Missing BlackTek DAT/SPR files";
            if (itemSource === "standard") lastLoadError = "Missing standard DAT/SPR files";
            return false;
        }
        if (itemSource === "standard" && !files.otb) {
            lastLoadError = "Missing standard items.otb file";
            return false;
        }
        if (itemSource === "blacktek" && !tomlPath) {
            lastLoadError = "Missing BlackTek items.toml file";
            return false;
        }

        if (loadedClientKey === key && loadedClientFolder === folder
            && loadedItemSource === itemSource && loadedItemTomlPath === tomlPath)
        {
            if (Backend.otbmReader.loading) {
                Backend.otbmReader.reportLoadingProgress(94, "Rebuilding sprite atlas...");
                mapView.rebuildAtlas();
            }
            return true;
        }

        var hasOtfi = Backend.otfiReader.loadFromFolder(folder);
        var datFile = hasOtfi ? folder + "/" + Backend.otfiReader.metadataFile : files.dat;
        var sprFile = hasOtfi ? folder + "/" + Backend.otfiReader.spritesFile : files.spr;

        Backend.datReader.clientVersion = version;
        Backend.datReader.setOtfiOverrides(hasOtfi, Backend.otfiReader.extended, Backend.otfiReader.frameDurations, Backend.otfiReader.frameGroups);
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(78, "Loading item definitions...");
        var datOk = Backend.datReader.loadFile(datFile, 0);
        var extendedSpr = hasOtfi ? Backend.otfiReader.extended : version >= 960;
        var alphaSpr = hasOtfi ? Backend.otfiReader.transparency : false;
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(83, "Loading item sprites...");
        var sprOk = Backend.sprReader.loadFile(sprFile, 0, extendedSpr, alphaSpr);
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(88, "Loading server items...");
        var itemOk = itemSource === "blacktek"
            ? Backend.otbReader.loadTomlFile(tomlPath, version)
            : Backend.otbReader.loadFile(files.otb);

        if (!datOk || !sprOk || !itemOk) {
            lastLoadError = !datOk ? Backend.datReader.errorString
                : !sprOk ? Backend.sprReader.errorString : Backend.otbReader.errorString;
            loadedClientVersion = 0;
            loadedClientKey = "";
            loadedClientFolder = "";
            loadedItemSource = "standard";
            loadedItemTomlPath = "";
            mapView.rebuildAtlas();
            return false;
        }

        loadedClientVersion = version;
        loadedClientKey = key;
        loadedClientFolder = folder;
        loadedItemSource = itemSource;
        loadedItemTomlPath = tomlPath;
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(90, "Preparing palette sprites...");
        Backend.preloadPaletteSprites();
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(92, "Loading editor palettes...");
        loadProfileData(key, itemSource);
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(94, "Rebuilding sprite atlas...");
        if (Backend.otbmReader.loading)
            mapView.rebuildAtlas();
        else
            mapView.refreshItemData();
        return true;
    }
}
