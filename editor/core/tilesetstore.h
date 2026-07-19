#ifndef TILESETSTORE_H
#define TILESETSTORE_H

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QVariantList>

// -----------------------------------------------------------------------------
// TilesetStore
//
// Zrodlo tilesetow dla palety (Terrain/Doodad/Item/RAW). JEDEN plik JSON na
// wersje klienta: <appDir>/data/<clientVersion>/tilesets.json - wspolny dla
// WSZYSTKICH folderow klienta tej samej wersji, w pelni odczyt+zapis (bez
// podzialu na "baze read-only" i "wlasne dopiski per-folder" - kazdy tileset,
// czy to zaimportowany z RME czy dodany recznie przez uzytkownika, jest tak
// samo edytowalny i usuwalny).
//
// Format: {"terrain": {"Nazwa": [serverId...]}, "doodad": {...}, "item": {...},
// "raw": {...}}.
// -----------------------------------------------------------------------------
class TilesetStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY tilesetsChanged)

public:
    explicit TilesetStore(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // Wczytuje <appDir>/data/<clientVersion>/tilesets.json. Kolejne mutacje
    // (addItem/newTileset/...) zapisuja sie z powrotem do tego samego pliku.
    // false = plik nie istnieje/pusty (mozna i tak zaczac dopisywac od zera).
    Q_INVOKABLE bool loadForVersion(int clientVersion);
    // Jak wyzej, ale z data/<dirName>/ (profile z nazwa wlasna). UWAGA: mutacje
    // tilesetow zapisuja sie do TEGO SAMEGO pliku (m_path) - dla nazwanego profilu
    // wlasne palety laduja wiec w data/<Nazwa>/tilesets.json, nie w bazie wersji.
    Q_INVOKABLE bool loadForDir(const QString &dirName);
    Q_INVOKABLE void clear();

    // Nazwy tilesetow majacych zawartosc w danej kategorii ("terrain"/"doodad"/"item"/"raw").
    Q_INVOKABLE QStringList namesFor(const QString &category) const;
    // Server idy tilesetu w danej kategorii.
    Q_INVOKABLE QVariantList itemsFor(const QString &category, const QString &name) const;
    // Czy tileset o tej nazwie istnieje (kazdy istniejacy jest usuwalny - patrz deleteTileset).
    Q_INVOKABLE bool isCustomOnly(const QString &category, const QString &name) const;

    // --- Mutacje (zapisuja od razu do tilesets.json wersji z loadForVersion) ---
    // Nowy pusty tileset. false gdy nazwa juz zajeta.
    Q_INVOKABLE bool newTileset(const QString &category, const QString &name);
    // Usuwa caly tileset.
    Q_INVOKABLE void deleteTileset(const QString &category, const QString &name);
    // Dopisuje item do tilesetu (tworzy go, jesli trzeba).
    Q_INVOKABLE void addItem(const QString &category, const QString &name, int serverId);
    // Usuwa item z tilesetu.
    Q_INVOKABLE void removeItem(const QString &category, const QString &name, int serverId);

signals:
    void tilesetsChanged();

private:
    static bool loadJsonInto(const QString &path,
                              QHash<QString, QStringList> &names,
                              QHash<QString, QHash<QString, QVariantList>> &items);
    void saveJson() const;
    void bump() { ++m_revision; emit tilesetsChanged(); }

    int m_revision = 0;
    QString m_path;   // <appDir>/data/<clientVersion>/tilesets.json

    QHash<QString, QStringList> m_names;
    QHash<QString, QHash<QString, QVariantList>> m_items;
};

#endif // TILESETSTORE_H
