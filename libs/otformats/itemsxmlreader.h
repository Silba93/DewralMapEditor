#ifndef ITEMSXMLREADER_H
#define ITEMSXMLREADER_H

#include <QHash>
#include <QObject>
#include <QString>

// -----------------------------------------------------------------------------
// ItemsXmlReader
//
// Parser serwerowego items.xml (format TFS/OTServ) - uzupelnia dane z items.otb.
//
// PO CO: items.otb czesto NIE niesie ani nazw itemow, ani ich typu. Nazwy i typ
// ("teleport", "depot", "door", "container"...) siedza dopiero w items.xml.
// RME czyta oba pliki (ItemDatabase::loadFromOtb + loadFromGameXml), dlatego
// pokazuje "magic forcefield" i wie, ze 1387 to teleport. Bez tego edytor widzi
// tylko gole id i nie potrafi rozpoznac teleportu.
//
// Format:
//   <items>
//     <item id="1387" article="a" name="magic forcefield">
//       <attribute key="type" value="teleport"/>
//     </item>
//     <item fromid="1500" toid="1510" name="wall"/>   <!-- zakres id -->
//   </items>
//
// Id w items.xml to SERWEROWE id - te same, ktorymi posluguje sie OTB i mapa.
// -----------------------------------------------------------------------------
class ItemsXmlReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY loadedChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY loadedChanged)

public:
    explicit ItemsXmlReader(QObject *parent = nullptr);

    // data/<wersja>/items.xml obok pliku wykonywalnego - ta sama konwencja co
    // creatures.xml (patrz CreatureStore). Plik jest OPCJONALNY: brak = edytor
    // dziala jak dotad, tylko bez nazw i typow z XML.
    Q_INVOKABLE bool loadForVersion(int version);
    // Jak wyzej, ale z data/<dirName>/ (profile z nazwa wlasna, np. "Midhem").
    Q_INVOKABLE bool loadForDir(const QString &dirName);
    Q_INVOKABLE bool loadFile(const QString &path);
    Q_INVOKABLE void clear();

    int count() const { return m_items.size(); }
    bool hasData() const { return !m_items.isEmpty(); }

    // Nazwa z items.xml lub pusty string, gdy itemu tam nie ma.
    QString nameForServerId(int serverId) const;
    // Wartosc <attribute key="type"> (np. "teleport", "depot", "door") lub "".
    QString typeForServerId(int serverId) const;
    bool isTeleport(int serverId) const;

signals:
    void loadedChanged();

private:
    struct Entry {
        QString name;
        QString type;   // z <attribute key="type" value="..."/>, male litery
    };

    QHash<int, Entry> m_items;   // serverId -> wpis
};

#endif // ITEMSXMLREADER_H
