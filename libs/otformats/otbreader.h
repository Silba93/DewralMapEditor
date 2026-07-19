#ifndef OTBREADER_H
#define OTBREADER_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <vector>

class DatReader;
class ItemsXmlReader;

enum class OtbItemGroup : uint8_t {
    None = 0,
    Ground = 1,
    Container = 2,
    Weapon = 3,
    Ammunition = 4,
    Armor = 5,
    Changes = 6,
    Teleport = 7,
    MagicField = 8,
    Writeable = 9,
    Key = 10,
    Splash = 11,
    Fluid = 12,
    Door = 13,
    Deprecated = 14,
    Podium = 15
};

struct OtbItem {
    uint16_t server_id = 0;
    uint16_t client_id = 0;
    uint8_t group = 0;
    uint32_t flags = 0;
    QString name;
    QString description;
    uint16_t speed = 0;
    uint16_t max_text_length = 0;
    uint8_t light_level = 0;
    uint8_t light_color = 0;
    int8_t stack_order = 0;

    bool is_unpassable = false;
    bool blocks_missiles = false;
    bool blocks_pathfinder = false;
    bool has_elevation = false;
    bool is_useable = false;
    bool is_pickupable = false;
    bool is_moveable = true;
    bool is_stackable = false;
    bool always_on_top = false;
    bool is_readable = false;
    bool is_rotatable = false;
    bool is_hangable = false;
    bool hook_east = false;
    bool hook_south = false;
};

class OtbReader : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(quint32 majorVersion READ majorVersion NOTIFY loadedChanged)
    Q_PROPERTY(quint32 minorVersion READ minorVersion NOTIFY loadedChanged)
    Q_PROPERTY(quint32 buildNumber READ buildNumber NOTIFY loadedChanged)

public:
    enum ItemRoles {
        ItemIdRole = Qt::UserRole + 1,
        ServerIdRole,
        ClientIdRole,
        NameRole,
        GroupRole,
        SpriteIdsRole,
        ItemWidthRole,
        ItemHeightRole,
        LayersRole,
        IsRenderableRole,
        IsGroundRole,
        IsStackableRole,
        IsContainerRole,
        IsUnpassableRole
    };

    explicit OtbReader(QObject *parent = nullptr);
    ~OtbReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int itemCount() const { return static_cast<int>(m_items.size()); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }
    quint32 majorVersion() const { return m_majorVersion; }
    quint32 minorVersion() const { return m_minorVersion; }
    quint32 buildNumber() const { return m_buildNumber; }

    void setDatReader(DatReader *datReader);
    // Serwerowe items.xml - zrodlo NAZW i TYPOW itemow, ktorych items.otb zwykle
    // nie niesie (patrz ItemsXmlReader). Wpiete tutaj, bo dzieki temu caly edytor
    // (paleta, pasek statusu, okno Properties) dostaje lepsze dane przez te sama
    // metode nameForServerId, bez zmian w konsumentach. Jak RME, ktore laczy
    // loadFromOtb + loadFromGameXml w jedna baze itemow.
    void setItemsXml(ItemsXmlReader *itemsXml);

    Q_INVOKABLE bool loadFile(const QString &path);
    Q_INVOKABLE int clientIdForServerId(int serverId) const;
    Q_INVOKABLE QString nameForServerId(int serverId) const;
    // Wiersz w modelu dla danego server id (do scrollowania palety). -1 = brak.
    Q_INVOKABLE int rowForServerId(int serverId) const;
    // Kolejnosc ukladania w stosie (OTB "TopOrder", atrybut 0x2B) dla itemow
    // always-on-bottom (bordery/dywaniki/stoly itp.) - jak RME Item::getTopOrder().
    // Przy WIELU itemach always-on-bottom na jednym kafelku decyduje kto jest
    // BLIZEJ podlogi: mniejszy TopOrder = nizej. 0 gdy nieznany/brak atrybutu.
    int topOrderForServerId(int serverId) const;
    // Grupa OTB itemu (OtbItemGroup) lub 0 gdy nieznany. Okno Properties uzywa
    // tego, by pokazac pole celu teleportu tylko dla itemow grupy Teleport -
    // dokladnie jak RME, gdzie atrybut TELE_DEST obsluguje klasa Teleport.
    Q_INVOKABLE int groupForServerId(int serverId) const;
    // Czytelna nazwa grupy ("Teleport", "Container", "None"...) - okno Properties
    // pokazuje ja wprost, zeby dalo sie od reki sprawdzic, czy items.otb w ogole
    // uznaje dany item za teleport/kontener (serwer decyduje o zachowaniu wlasnie
    // po tym, nie po wygladzie sprite'a).
    Q_INVOKABLE QString groupNameForServerId(int serverId) const;
    // Czy item jest teleportem (okno Properties pokazuje wtedy cel, a menu PPM
    // "Go To Destination"). Zrodlem prawdy jest grupa z items.otb, ale wiele OTB
    // (zwlaszcza TFS) trzyma teleporty jako grupe None i deklaruje typ dopiero w
    // items.xml - RME czyta oba pliki, my na razie tylko OTB. Do czasu wsparcia
    // items.xml rozpoznajemy dodatkowo klasyczne id teleportu.
    Q_INVOKABLE bool isTeleportItem(int serverId) const;
    Q_INVOKABLE QVariantMap detailsAt(int row) const;

signals:
    void itemCountChanged();
    void loadedChanged();
    void errorChanged();

private:
    static uint8_t mapGroup(uint8_t group);

    void reset();
    void setError(const QString &message);
    void refreshDatRoles();

    std::vector<OtbItem> m_items;
    QHash<uint16_t, int> m_serverIdToRow;
    QHash<uint16_t, int> m_clientIdToRow;
    DatReader *m_datReader = nullptr;
    ItemsXmlReader *m_itemsXml = nullptr;

    uint32_t m_majorVersion = 0;
    uint32_t m_minorVersion = 0;
    uint32_t m_buildNumber = 0;
    bool m_loaded = false;
    QString m_errorString;
};

#endif // OTBREADER_H
