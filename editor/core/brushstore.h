#ifndef BRUSHSTORE_H
#define BRUSHSTORE_H

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <array>

// -----------------------------------------------------------------------------
// BrushStore - silnik brushy (Etap 1: ground brushe + auto-bordery).
//
// Laduje data/<wersja>/brushes.json (wygenerowany z RME borders.xml+grounds.xml)
// i odtwarza algorytm RME GroundBrush::doBorders + getBrushTo 1:1 (tablica
// kBorderTypes[256] przeniesiona z brush_tables.cpp).
//
// Czysta warstwa danych: computeBorderItems() dostaje nazwy brushy srodka i 8
// sasiadow, zwraca uporzadkowana liste server-idow kafli bordera do polozenia.
// Mutacja kafli mapy (kasowanie starych borderow, wstawianie nowych) nalezy do
// MapView, ktory ma dostep do OTBM i prymitywow edycji.
// -----------------------------------------------------------------------------
class BrushStore : public QObject
{
    Q_OBJECT
public:
    explicit BrushStore(QObject *parent = nullptr);

    // ===== EDYTOR BRUSHY (Tools > Brush Editor) =====
    // Zrodlem prawdy edycji jest SUROWY JSON (m_rawRoot) wczytany z brushes.json:
    // mutacja zmienia JSON, potem pelny reparse (jedna sciezka parsowania) i zapis
    // atomowy (QSaveFile). Dzieki temu pola, ktorych edytor nie zna (doodady,
    // friends, optional...), przechodza przez edycje NIETKNIETE.
    Q_INVOKABLE QStringList groundBrushNames() const;
    Q_INVOKABLE QStringList wallBrushNames() const;
    // Dane ground brusha do edycji: { zorder, items: [{id, chance}...],
    // borders: [ { to, tiles: [13 intow, indeksy BT_*] } ... ] }.
    // "to": "" = przeciw pustce (brak sasiada), "*" = przeciw dowolnemu innemu
    // brushowi, inaczej NAZWA konkretnego ground brusha (np. "water") - to jest
    // mechanizm "wodny border": grunt bordera SPECYFICZNIE z wybranym brushem.
    Q_INVOKABLE QVariantMap groundBrushEdit(const QString &name) const;
    // Zapis (tworzy brush, gdy nie istnieje). items: [{id, chance}]; borderBlocks:
    // [ { to, tiles: [13] } ] - kazdy blok to osobny border-do-celu (patrz wyzej).
    // Wszystkie zapisywane jako align="outer" (border rysuje sie na NIZSZYM z-order
    // z pary, wg z-order - patrz getBrushTo). friends/optional/hate zachowane.
    Q_INVOKABLE bool saveGroundBrush(const QString &name, int zorder,
                                     const QVariantList &items,
                                     const QVariantList &borderBlocks);
    Q_INVOKABLE void deleteGroundBrush(const QString &name);
    // Sciany: 17 slotow (align = maska sasiadow N=1 W=2 E=4 S=8; 16 = extra),
    // MVP: jeden item na slot. Zwraca/przyjmuje liste 17 intow (0 = pusty slot).
    Q_INVOKABLE QVariantList wallBrushEdit(const QString &name) const;
    Q_INVOKABLE bool saveWallBrush(const QString &name, const QVariantList &align17);
    Q_INVOKABLE void deleteWallBrush(const QString &name);

signals:
    // Po kazdej mutacji edytora i po (re)wczytaniu pliku - odswieza comba w QML.
    void brushesChanged();

public:

    Q_INVOKABLE bool loadForVersion(int clientVersion);
    // Jak wyzej, ale z data/<dirName>/ - profile z nazwa wlasna (np. "Midhem")
    // maja wlasny katalog danych. loadForVersion = loadForDir(numer).
    Q_INVOKABLE bool loadForDir(const QString &dirName);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool hasData() const { return !m_grounds.isEmpty(); }

    // Nazwa ground brusha, do ktorego nalezy dany server id (lub "" gdy zaden).
    QString groundBrushForServerId(int serverId) const { return m_groundByServerId.value(serverId); }
    Q_INVOKABLE bool isGroundBrushItem(int serverId) const { return m_groundByServerId.contains(serverId); }
    Q_INVOKABLE bool isGroundBrush(const QString &name) const { return m_grounds.contains(name); }

    // Wazone losowanie ground itemu z brusha (jak GroundBrush::draw). 0 gdy brak.
    int pickGroundItem(const QString &name) const;

    // Czy dany server id to kafelek bordera (nalezy do jakiejs definicji <border>).
    // Uzywane do "cleanBorders" - kasowania starych borderow przed przeliczeniem.
    bool isManagedBorderItem(int serverId) const { return m_borderItemIds.contains(serverId); }

    // Port GroundBrush::doBorders: dla brusha srodka (center, "" = pusty kafel) i
    // 8 sasiadow (indeksy: 0=NW 1=N 2=NE 3=W 4=E 5=SW 6=S 7=SE; "" = pusty/poza mapa)
    // zwraca uporzadkowana liste server-idow kafli bordera do polozenia na kaflu.
    QVector<int> computeBorderItems(const QString &center, const QStringList &neighbours8) const;

    // --- Wall brushe (RME WallBrush::doWalls) ---
    Q_INVOKABLE bool hasWallData() const { return !m_walls.isEmpty(); }
    // Nazwa wall brusha, do ktorego nalezy server id (lub "" gdy zaden wall item).
    QString wallBrushForServerId(int serverId) const { return m_wallByServerId.value(serverId); }
    Q_INVOKABLE bool isWallBrushItem(int serverId) const { return m_wallByServerId.contains(serverId); }
    Q_INVOKABLE bool isWallBrush(const QString &name) const { return m_walls.contains(name); }
    // Item bazowy (pole, wyrownanie 0) danego wall brusha - do wstepnego postawienia
    // markera przed przeliczeniem wyrownan. 0 gdy brak.
    int wallPoleItem(const QString &name) const;
    // Wybor server-id itemu sciany dla stanu 4 sasiadow (N/W/E/S = czy sasiad ma
    // sciane tego brusha). Dwuprzebiegowo jak doWalls: full_border_types -> gdy brak
    // ksztaltu, half_border_types. 0 gdy nic pasujacego.
    int computeWallItem(const QString &name, bool n, bool w, bool e, bool s) const;

    // --- Doodad brushe (RME DoodadBrush) ---
    Q_INVOKABLE bool hasDoodadData() const { return !m_doodads.isEmpty(); }
    QString doodadBrushForServerId(int serverId) const { return m_doodadByServerId.value(serverId); }
    Q_INVOKABLE bool isDoodadBrushItem(int serverId) const { return m_doodadByServerId.contains(serverId); }
    Q_INVOKABLE bool isDoodadBrush(const QString &name) const { return m_doodads.contains(name); }
    // Jeden kafel doodada: offset (dx,dy) od kursora, dz = offset PIETRA (np. wodospad
    // ma czesci na kilku poziomach), + itemy do polozenia (od dolu stosu).
    struct DoodadTile { int dx = 0, dy = 0, dz = 0; QVector<int> items; };
    // Losuje jedno wystapienie doodada (RME: wariant->single/composite wg szans).
    // Zwraca liste kafli do polozenia (single = 1 kafel dx=dy=0; composite = wiele).
    QVector<DoodadTile> pickDoodad(const QString &name) const;
    // Liczba WARIANTOW doodada (kazdy single + kazdy composite ze wszystkich alts) -
    // do rotacji klawiszem R. 0/1 = brak sensu rotowac.
    int doodadVariantCount(const QString &name) const;
    // Konkretny wariant po indeksie (deterministyczny, do rotacji/ghost). index poza
    // zakresem jest zawijany. Pusty gdy doodad nie ma wariantow.
    QVector<DoodadTile> doodadVariantTiles(const QString &name, int index) const;
    // Reprezentatywny composite do PODGLADU w palecie (pierwszy znaleziony, bez
    // losowania - ikona ma byc stabilna). Pusto gdy doodad nie ma compositow
    // (wtedy paleta rysuje zwykla ikone pojedynczego itemu).
    QVector<DoodadTile> doodadPreviewTiles(const QString &name) const;
    // Wszystkie unikalne server id itemow doodada (singles + composites, wszystkie
    // warianty) - do proaktywnego dodania sprite'ow do atlasu przy wyborze pedzla,
    // zeby ghost pokazywal CALY doodad zanim postawimy go raz.
    QVector<int> doodadItemIds(const QString &name) const;

private:
    struct BorderBlock {
        bool outer = true;
        QString to;         // "*" = dowolny inny, "" = nic/pustka, inaczej nazwa brusha
        QString borderKey;  // klucz w m_borders
    };
    struct GroundDef {
        int zorder = 0;
        int lookid = 0;
        QVector<QPair<int, int>> items;   // (serverId, skumulowana szansa)
        int totalChance = 0;
        QVector<BorderBlock> borders;
        QSet<QString> friends;
        bool friendsAll = false;
        bool hateFriends = false;
        QString optional;                 // klucz w m_borders lub ""

        bool hasOuter = false, hasInner = false, hasZilchOuter = false, hasZilchInner = false;

        bool hasOptional() const { return !optional.isEmpty(); }
        bool outerBorderFlag() const { return hasOuter || hasOptional(); }
        bool innerBorderFlag() const { return hasInner; }
        bool outerZilchFlag() const { return hasZilchOuter || hasOptional(); }
        bool innerZilchFlag() const { return hasZilchInner; }
        bool useSoloOptional = false;
    };

    // getBrushTo: klucz bordera miedzy dwoma bruszami (lub "" gdy brak). Nazwy
    // "" oznaczaja pusty kafel (brak brusha), zgodnie z RME getBrushTo(first,second).
    QString getBrushTo(const QString &firstName, const QString &secondName) const;
    // friendOf: czy 'self' traktuje 'otherName' jako przyjaciela (brak bordera).
    bool friendOf(const GroundDef &self, const QString &otherName) const;
    const std::array<int, 13> *borderTiles(const QString &key) const;
    // Definicja ground brusha po nazwie lub nullptr ("" tez -> nullptr). const-safe
    // (bez QHash operator[], ktory wstawialby domyslny wpis w metodzie const).
    const GroundDef *groundDef(const QString &name) const;

    // Definicja wall brusha: 17 wyrownan (WALL_* z RME), kazde to wazona lista itemow.
    struct WallDef {
        int lookid = 0;
        struct Node {
            QVector<QPair<int, int>> items;   // (serverId, skumulowana szansa)
            int total = 0;
        };
        std::array<Node, 17> align;
    };
    int pickWallFromNode(const WallDef::Node &node) const;
    const WallDef *wallDef(const QString &name) const;

    // Definicja doodada: lista wariantow (RME <alternate>); kazdy wariant ma wazona
    // liste singli (1 kafel) i wazona liste compositow (stempel wielokaflowy).
    struct DoodadDef {
        int lookid = 0;
        struct Composite { int chance = 0; QVector<DoodadTile> tiles; };
        struct Alt {
            QVector<QPair<int, int>> singles;   // (serverId, szansa) - NIE skumulowane
            int singleTotal = 0;
            QVector<Composite> composites;      // (szansa, kafle) - NIE skumulowane
            int compositeTotal = 0;
        };
        QVector<Alt> alts;
    };
    const DoodadDef *doodadDef(const QString &name) const;

    QHash<QString, std::array<int, 13>> m_borders;
    QHash<QString, GroundDef> m_grounds;
    QHash<int, QString> m_groundByServerId;
    QSet<int> m_borderItemIds;

    QHash<QString, WallDef> m_walls;
    QHash<int, QString> m_wallByServerId;

    QHash<QString, DoodadDef> m_doodads;
    QHash<int, QString> m_doodadByServerId;   // lookid (ikona w palecie) -> nazwa doodada

    // --- Edytor brushy (patrz sekcja publiczna) ---
    void parseRoot(const QJsonObject &root);   // wspolna sciezka parsowania
    bool saveJson() const;                     // atomowy zapis m_rawRoot do m_path
    void applyRawAndSave();                    // reparse + zapis + brushesChanged
    QJsonObject m_rawRoot;                     // surowy brushes.json (zrodlo edycji)
    QString m_path;                            // sciezka pliku (takze gdy nie istnial)
};

#endif // BRUSHSTORE_H
