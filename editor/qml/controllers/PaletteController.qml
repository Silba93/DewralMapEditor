import QtQml

QtObject {
    id: controller
    required property var settings

    property var customPalettes: ({})
    readonly property var customPaletteNames: Object.keys(customPalettes).sort()
    readonly property int iconSizePx: settings.iconSize

    function load() {
        try {
            customPalettes = JSON.parse(settings.customPalettesJson) || ({});
        } catch (e) {
            customPalettes = ({});
        }
    }

    function save() {
        settings.customPalettesJson = JSON.stringify(customPalettes);
    }

    function addCustomPalette(name) {
        if (!name || customPalettes[name] !== undefined)
            return false;
        var copy = JSON.parse(JSON.stringify(customPalettes));
        copy[name] = [];
        customPalettes = copy;
        save();
        return true;
    }

    function deleteCustomPalette(name) {
        var copy = JSON.parse(JSON.stringify(customPalettes));
        delete copy[name];
        customPalettes = copy;
        save();
    }

    function addItemToPalette(name, serverId) {
        if (customPalettes[name] === undefined)
            return;
        var copy = JSON.parse(JSON.stringify(customPalettes));
        if (copy[name].indexOf(serverId) < 0)
            copy[name].push(serverId);
        customPalettes = copy;
        save();
    }

    function removeItemFromPalette(name, serverId) {
        if (customPalettes[name] === undefined)
            return;
        var copy = JSON.parse(JSON.stringify(customPalettes));
        var index = copy[name].indexOf(serverId);
        if (index >= 0)
            copy[name].splice(index, 1);
        customPalettes = copy;
        save();
    }
}
