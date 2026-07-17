#ifndef OTFIREADER_H
#define OTFIREADER_H

#include <QObject>
#include <QString>

// -----------------------------------------------------------------------------
// OtfiReader
//
// Parsuje pliki .otfi (uzywane przez RME/OTClient), ktore JAWNIE nadpisuja
// wykrywanie formatu .dat/.spr - potrzebne gdy niestandardowy klient/serwer ma
// wlaczone flagi (np. OTClient g_game.enableFeature(GameSpritesU32) itp.)
// niezgodne z autodetekcja samej wersji/sygnatury .dat.
//
// Format 1:1 z RME (ClientVersion::hasValidPaths / GraphicManager::loadOTFI):
// prosty wciety klucz:wartosc pod naglowkiem "DatSpr":
//
//   DatSpr
//     extended: true
//     transparency: false
//     frame-durations: true
//     frame-groups: true
//     metadata-file: Tibia.dat
//     sprites-file: Tibia.spr
//
// Znaczenie flag:
//   extended         - sprite ID w .dat to u32 (nie u16); naglowek .spr ma
//                       4-bajtowy sprite_count (nie 2-bajtowy).
//   transparency     - piksele sprite'ow maja realny kanal alpha (4B/piksel:
//                       RGBA), zamiast domyslnego RGB+kolorklucz (3B/piksel).
//   frame-durations  - animowane rzeczy maja tabele czasow trwania klatek.
//   frame-groups     - outfity maja grupy klatek (idle/moving) zamiast jednej.
//
// Nieznane klucze (np. "sprite-size", "sprite-data-size" - rozszerzenia spoza
// RME) sa ciche ignorowane.
// -----------------------------------------------------------------------------
class OtfiReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool found READ found NOTIFY foundChanged)
    Q_PROPERTY(bool extended READ extended NOTIFY foundChanged)
    Q_PROPERTY(bool transparency READ transparency NOTIFY foundChanged)
    Q_PROPERTY(bool frameDurations READ frameDurations NOTIFY foundChanged)
    Q_PROPERTY(bool frameGroups READ frameGroups NOTIFY foundChanged)
    Q_PROPERTY(QString metadataFile READ metadataFile NOTIFY foundChanged)
    Q_PROPERTY(QString spritesFile READ spritesFile NOTIFY foundChanged)

public:
    explicit OtfiReader(QObject *parent = nullptr);

    // Szuka *.otfi w folderze i parsuje pierwszy znaleziony. false = brak pliku
    // (wowczas found()==false, reszta getterow ma wartosci domyslne/nieuzywane -
    // wolajacy powinien wtedy autodetekcje wg wersji klienta, jak dotychczas).
    Q_INVOKABLE bool loadFromFolder(const QString &folder);

    bool found() const { return m_found; }
    bool extended() const { return m_extended; }
    bool transparency() const { return m_transparency; }
    bool frameDurations() const { return m_frameDurations; }
    bool frameGroups() const { return m_frameGroups; }
    QString metadataFile() const { return m_metadataFile; }
    QString spritesFile() const { return m_spritesFile; }

signals:
    void foundChanged();

private:
    bool m_found = false;
    bool m_extended = false;
    bool m_transparency = false;
    bool m_frameDurations = false;
    bool m_frameGroups = false;
    QString m_metadataFile;
    QString m_spritesFile;
};

#endif // OTFIREADER_H
