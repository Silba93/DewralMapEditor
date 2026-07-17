#ifndef DATREADER_H
#define DATREADER_H

#include "binaryreader.h"
#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <cstdint>
#include <vector>

// -----------------------------------------------------------------------------
// ClientItem
//
// Wewnetrzna struktura danych wczytanych z jednego wpisu w Tibia.dat.
// Reprezentuje dane KLIENCKIE (wizualne) jednego itemu - wymiary, sprite ID,
// flagi z pliku .dat. Nie zawiera danych serwerowych (server_id, nazwa) -
// te pochodza z OtbReader i sa przechowywane w OtbItem.
//
// Uzywana przez OtbReader::data() do dostarczania ról spriteIds/itemWidth/
// itemHeight/layers do QML.
// -----------------------------------------------------------------------------
struct ClientItem {
    uint16_t id = 0; // client ID (pozycja w .dat, 1-indeksowane od 100 dla items)

    // Wymiary sprite'a - pojedynczy item moze skladac sie z kilku plytek 32x32
    uint8_t width = 1;
    uint8_t height = 1;
    uint8_t layers = 1;
    uint8_t pattern_x = 1;
    uint8_t pattern_y = 1;
    uint8_t pattern_z = 1;
    uint8_t frames = 1;

    std::vector<uint32_t> sprite_ids;

    // Flagi z .dat (podzbiór CanonicalFlags przeliczony przez transformFlag)
    bool is_ground = false;
    uint16_t ground_speed = 0;
    bool is_on_bottom = false;
    bool is_on_top = false;
    bool is_container = false;
    bool is_stackable = false;
    bool is_useable = false;
    bool is_writable = false;
    uint16_t max_text_length = 0;
    bool is_fluid_container = false;
    bool is_fluid = false;
    bool is_unpassable = false;
    bool is_unmoveable = false;
    bool blocks_missiles = false;
    bool blocks_pathfinder = false;
    bool is_pickupable = false;
    bool is_hangable = false;
    bool is_horizontal = false; // HOOK_EAST w .dat
    bool is_vertical = false;   // HOOK_SOUTH w .dat
    bool is_rotatable = false;
    bool has_light = false;
    uint16_t light_level = 0;
    uint16_t light_color = 0;
    bool dont_hide = false;
    bool is_translucent = false;
    bool has_offset = false;
    int16_t offset_x = 0;
    int16_t offset_y = 0;
    bool has_elevation = false;
    uint16_t elevation = 0;
    bool is_lying_object = false;
    bool animate_always = false;
    bool has_minimap_color = false;
    uint16_t minimap_color = 0;
    bool full_ground = false;
    bool ignore_look = false;
    uint16_t lens_help = 0;
    bool floor_change = false;

    // Pierwszy sprite ID - podglad/miniatura w liscie
    uint32_t previewSpriteId() const {
        return sprite_ids.empty() ? 0 : sprite_ids.front();
    }

    uint32_t getTotalSprites() const {
        return static_cast<uint32_t>(width) * height * layers
             * pattern_x * pattern_y * pattern_z * frames;
    }
};

// -----------------------------------------------------------------------------
// DatReader
//
// Parser Tibia.dat, format 7.55-7.72 (DatReaderV755 z repo).
//
// Czyta TYLKO kategorie Items (ID 100..max_item_id). Outfits/Effects/Missiles
// sa parsowane w celu utrzymania poprawnego offsetu w strumieniu plikowym,
// ale nie sa przechowywane - ta klasa nie wystawia ich przez API.
//
// Udostepnia dane jako QAbstractListModel (model dla GridView w QML)
// oraz przez itemByClientId() dla OtbReader (lookup po client_id).
// -----------------------------------------------------------------------------
class DatReader : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    enum ItemRoles {
        ItemIdRole = Qt::UserRole + 1,
        PreviewSpriteIdRole,
        SpriteIdsRole,
        ItemWidthRole,
        ItemHeightRole,
        LayersRole,
        IsGroundRole,
        IsStackableRole,
        IsContainerRole,
        IsUnpassableRole
    };

    explicit DatReader(QObject *parent = nullptr);
    ~DatReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int itemCount() const { return static_cast<int>(m_items.size()); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }

    // Wersja klienta jako liczba (772, 800, 860, 1098...). Steruje formatem .dat:
    //   <780: flagi 7.55-7.72 | 780-854: +Chargeable | 860-986: kanoniczny |
    //   >=960: sprite ID u32 | >=1010: +NoMoveAnim/Market | >=1050: frame durations |
    //   >=1057: frame groups (outfity). Ustawic PRZED loadFile.
    Q_PROPERTY(int clientVersion READ clientVersion WRITE setClientVersion NOTIFY clientVersionChanged)

    int clientVersion() const { return m_clientVersion; }
    Q_INVOKABLE void setClientVersion(int v);

    // Nadpisanie autodetekcji formatu z pliku .otfi (jak RME) - dla klientow z
    // niestandardowymi flagami (np. OTClient enableFeature(...)) niezgodnymi z
    // tym, co sugerowalaby sama wersja klienta. Ustawic PRZED loadFile(); wywolac
    // z has=false (lub w ogole nie wolac) zeby wrocic do autodetekcji wg wersji.
    Q_INVOKABLE void setOtfiOverrides(bool has, bool extended, bool frameDurations, bool frameGroups) {
        m_otfiActive = has;
        m_otfiExtended = extended;
        m_otfiFrameDurations = frameDurations;
        m_otfiFrameGroups = frameGroups;
    }

    Q_INVOKABLE bool loadFile(const QString &path, quint32 expectedSignature = 0);

    Q_INVOKABLE quint32 previewSpriteIdAt(int row) const;
    Q_INVOKABLE int itemIdAt(int row) const;
    Q_INVOKABLE QVariantMap detailsAt(int row) const;

    // Lookup po client ID - uzywany przez OtbReader do pobrania danych sprite
    const ClientItem *itemByClientId(uint16_t clientId) const;

    // Outfit (looktype z creatures.xml/spawnow) - geometria+sprite'y jak item.
    // Looktype jest 1-indeksowany (1..maxOutfitId). Kierunek = pattern_x (4),
    // kolory template = layers 2 (warstwa 1 to maska barwienia; MVP: layer 0).
    const ClientItem *outfitByLookType(uint16_t lookType) const;

    // Podglad outfitu do palety Creatures: {ids: [sprite id...], width, height}
    // (kierunek poludnie, warstwa bazowa, klatka 0) - w kolejnosci zgodnej z
    // SprReader::itemImageSource. Pusta mapa gdy looktype nieznany.
    Q_INVOKABLE QVariantMap outfitPreview(int lookType) const;

    // Podglad ITEMU po client id: {ids, width, height} (warstwa 0, pattern 0,
    // klatka 0) - do ikon w UI (np. przyciski topbaru). Format jak outfitPreview.
    Q_INVOKABLE QVariantMap itemPreview(int clientId) const;
    // Efekt magiczny po ID (1-indeksowane, jak w Tibii). nullptr gdy brak.
    const ClientItem *effectById(int id) const;

signals:
    void itemCountChanged();
    void loadedChanged();
    void errorChanged();
    void clientVersionChanged();

private:
    // Parsuje blok obiektow z pliku (items, outfits, effects, missiles).
    // Jesli storeItems == true, wczytane ClientItem sa zapisywane do outItems.
    // Jesli storeItems == false, obiekty sa parsowane i odrzucane (tylko
    // po to, zeby poprawnie przesunac wskaznik w strumieniu plikowym).
    // outfits=true: kategoria outfitow (frame groups w 10.57+).
    void readCategory(BinaryReader &reader,
                      std::vector<ClientItem> *outItems, // nullptr = parsuj i odrzuc
                      uint16_t minId, uint16_t maxId, bool outfits = false);

    void readItemFlags(ClientItem &item, BinaryReader &reader);
    void readSpriteData(ClientItem &item, BinaryReader &reader, bool outfits);

    // Mapuje surowy bajt flagi na flage kanoniczna wg wersji klienta (jak RME):
    //   <780: 23->FLOOR_CHANGE | 780-854: 8->CHARGEABLE, >8 -> -1 |
    //   860-986: identycznosc | >=1010: 16->NO_MOVE_ANIMATION, >16 -> -1
    uint8_t transformFlag(uint8_t raw) const;

    // Kazda flaga: nadpisana przez .otfi jesli aktywne (patrz setOtfiOverrides),
    // inaczej autodetekcja wg wersji klienta (jak dotychczas).
    bool extendedSprites() const { return m_otfiActive ? m_otfiExtended : m_clientVersion >= 960; }
    bool frameDurations() const  { return m_otfiActive ? m_otfiFrameDurations : m_clientVersion >= 1050; }
    bool frameGroups() const     { return m_otfiActive ? m_otfiFrameGroups : m_clientVersion >= 1057; }

    void setError(const QString &message);
    void reset();

    std::vector<ClientItem> m_items;   // tylko Items (ID 100..max_item_id)
    std::vector<ClientItem> m_effects; // efekty magiczne (ID 1..max_effect_id)
    std::vector<ClientItem> m_outfits; // outfity (looktype 1..max_outfit_id) - spawny
    int m_clientVersion = 772;         // wersja klienta (steruje formatem .dat)
    bool m_otfiActive = false;         // czy uzywac nadpisan z .otfi (patrz setOtfiOverrides)
    bool m_otfiExtended = false;
    bool m_otfiFrameDurations = false;
    bool m_otfiFrameGroups = false;
    uint32_t m_signature = 0;
    uint16_t m_maxItemId = 0;
    bool m_loaded = false;
    QString m_errorString;
};

#endif // DATREADER_H
