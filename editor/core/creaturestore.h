#ifndef CREATURESTORE_H
#define CREATURESTORE_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// -----------------------------------------------------------------------------
// CreatureStore
//
// Lista potworow/NPC do palety Creatures - z data/<wersja>/creatures.xml w
// formacie RME (ten sam czyta tibia-imgui-map-editor):
//   <creatures>
//     <creature name="Demon" type="monster" looktype="35"/>
//     <creature name="Sam" type="npc" looktype="128" lookhead="..." .../>
//   </creatures>
// Obslugiwane sa tez warianty <monsters><monster .../> i plaskie <monster/npc>.
//
// Model listy dla QML (paleta): nazwa + looktype/lookitem (podglad outfitu
// rysuje MapView z DatReader::outfitByLookType). Kolory outfitu trzymamy, ale
// MVP renderuje warstwe bazowa bez barwienia (template).
// -----------------------------------------------------------------------------
class CreatureStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY countChanged)

public:
    struct CreatureType {
        QString name;
        bool isNpc = false;
        int lookType = 0;   // outfit z .dat (kategoria Outfits)
        int lookItem = 0;   // wyglad itemu (rzadkie; np. pulapki)
        int lookHead = 0, lookBody = 0, lookLegs = 0, lookFeet = 0;
    };

    enum Roles {
        NameRole = Qt::UserRole + 1,
        IsNpcRole,
        LookTypeRole,
        LookItemRole,
    };

    explicit CreatureStore(QObject *parent = nullptr);

    // data/<wersja>/creatures.xml - opcjonalny (pusta paleta gdy brak pliku).
    Q_INVOKABLE bool loadForVersion(int version);

    int count() const { return static_cast<int>(m_creatures.size()); }
    bool hasData() const { return !m_creatures.isEmpty(); }

    const CreatureType *byName(const QString &name) const;

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    bool loadFile(const QString &path);

    QVector<CreatureType> m_creatures;   // posortowane po nazwie
};

#endif // CREATURESTORE_H
