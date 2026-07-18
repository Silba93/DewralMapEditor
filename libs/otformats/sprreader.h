#ifndef SPRREADER_H
#define SPRREADER_H

#include <QAbstractListModel>
#include <QImage>
#include <QVector>
#include <QHash>
#include <QString>
#include <QFile>
#include <QUrl>
#include <QVariantList>
#include <memory>
#include <cstdint>

// -----------------------------------------------------------------------------
// SprReader
//
// Parser plikow Tibia.spr, zaadaptowany pod Qt/QML z architektury
// MapEditor::IO::SprReader (tibia-imgui-map-editor): lazy-loading
// pojedynczych sprite'ow z cache'owaniem, zamiast dekodowania calego
// pliku naraz.
//
// Konfiguracja docelowa: Tibia 7.72 -> extended = false (2-bajtowy
// sprite_count w naglowku). Parametr "extended" jest zachowany 1:1
// z oryginalnego API, zeby klasa byla zgodna takze z nowszymi formatami
// (9.60+, 4-bajtowy sprite_count) bez zmiany sygnatury metod.
//
// Uklad pliku .spr:
//   [4 bajty]  signature (uint32 LE) - znacznik wersji klienta
//   [2 lub 4 bajty] sprite_count (uint16 dla 7.72 / uint32 dla extended)
//   [N * 4B]   tablica offsetow (uint32 LE kazdy), liczona od poczatku pliku
//              Indeks 0 w tablicy = sprite ID 1 (sprite ID sa 1-indeksowane,
//              ID 0 oznacza zawsze "pusty" sprite).
//
//   Dane pojedynczego sprite'a (pod jego offsetem):
//     [3 bajty]  magenta colorkey (RGB, zazwyczaj 0xFF,0x00,0xFF) - kolor
//                ktory podczas dekodowania jest mapowany na alpha=0
//     [2 bajty]  compressed_size (uint16 LE) - rozmiar danych RLE ponizej
//     [dane RLE], powtarzane bloki az do wyczerpania compressed_size bajtow:
//        [2 bajty] transparent_pixel_count (uint16) - piksele przezroczyste
//        [2 bajty] colored_pixel_count (uint16) - piksele kolorowe
//        [colored_pixel_count * 3 bajty] dane RGB kolejnych pikseli
//
//   Kazdy sprite to bitmapa SPRITE_SIZE x SPRITE_SIZE (32x32 dla 7.72),
//   piksele ukladane row-major od lewego-gornego rogu.
// -----------------------------------------------------------------------------

struct SpriteData {
    uint32_t id = 0;
    bool is_empty = true;
    QImage image; // zdekodowany obraz RGBA8888, wypelniany przez decode()

    bool decode(const QByteArray &fileData, uint32_t offset, int spriteSize, bool useAlpha = false);
};

class SprReader : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int spriteCount READ spriteCount NOTIFY spriteCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    enum SpriteRoles {
        SpriteIdRole = Qt::UserRole + 1,
        SpriteImageRole,
        SpriteImageSourceRole
    };

    explicit SprReader(QObject *parent = nullptr);
    ~SprReader() override;

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int spriteCount() const { return static_cast<int>(m_spriteCount); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }

    // Otwiera plik .spr. expectedSignature == 0 pomija walidacje sygnatury.
    // extended = false -> format 7.72 (2-bajtowy sprite_count).
    // useAlpha = true -> flaga "transparency" z .otfi (jak RME): piksele maja
    // realny kanal alpha (4B/piksel RGBA) zamiast domyslnego RGB (3B, alpha=255).
    // Wystawione do QML jako wywolywalna metoda (np. z przycisku "Open").
    Q_INVOKABLE bool loadFile(const QString &path,
                               quint32 expectedSignature = 0,
                               bool extended = false,
                               bool useAlpha = false);

    // Pomocnicze dla QML: konwertuje QUrl zwracany przez FileDialog
    // (np. "file:///C:/Users/..." na Windows, "file:///home/..." na Linux)
    // na poprawna lokalna sciezke, niezaleznie od platformy. Recznie
    // robione string-replace na "file://" psuje sciezki na Windows
    // (zostawia dodatkowy "/" przed litera dysku), wiec uzywamy QUrl::toLocalFile().
    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }

    // Lazy-load + cache pojedynczego sprite'a po ID (1-indeksowane).
    // Odpowiednik SprReader::loadSprite() z oryginalnego API.
    std::shared_ptr<SpriteData> loadSprite(uint32_t spriteId);

    // Wygodne dla QML: zwraca zdekodowany obraz wprost jako QImage.
    Q_INVOKABLE QImage spriteImage(int spriteId);

    // Wygodne dla QML: data-URL (image/png base64) do podstawienia
    // bezposrednio w Image.source bez wlasnego QQuickImageProvider.
    Q_INVOKABLE QString spriteImageSource(int spriteId);

    // Sklada item z wielu sprite'ow 32x32 wedlug kolejnosci zapisanej w .dat.
    // Uzywane dla itemow 64x64 i wiekszych, ktore w items.otb/DAT nadal sa
    // jednym itemem, ale maja kilka czesci graficznych.
    Q_INVOKABLE QString itemImageSource(const QVariantList &spriteIds,
                                        int itemWidth,
                                        int itemHeight,
                                        int layers);

signals:
    void spriteCountChanged();
    void loadedChanged();
    void errorChanged();

private:
    static constexpr int kDefaultSpriteSize = 32; // 7.72: sprite'y zawsze 32x32

    // Limity cache'ow - bez nich rosly bez ograniczen przez caly czas zycia
    // aplikacji (kazdy obejrzany sprite/item zostawal w pamieci na zawsze; przy
    // duzym kliencie i dlugiej sesji setki MB). Strategia: po przekroczeniu
    // limitu cache jest CZYSZCZONY w calosci - prosciej niz LRU, a wystarcza:
    // dekodowanie jest leniwe i tanie (po fixie decode()), wiec ponowne zapelnienie
    // biezaco uzywanych wpisow jest niezauwazalne.
    //  - sprite'y: 4 KB kazdy (32x32 RGBA) -> 16384 = ~64 MB max
    //  - data-URLe (PNG base64 dla QML palet): mniejsze, ale tez ograniczamy
    static constexpr int kMaxSpriteCache = 16384;
    static constexpr int kMaxDataUrlCache = 8192;

    void setError(const QString &message);
    void reset();

    QByteArray m_fileData;              // cala zawartosc pliku .spr w pamieci
    QVector<uint32_t> m_offsets;        // tablica offsetow, indeks 0 = sprite ID 1
    uint32_t m_signature = 0;
    uint32_t m_spriteCount = 0;
    bool m_extended = false;
    bool m_useAlpha = false;   // flaga "transparency" z .otfi (RGBA vs RGB+255)
    bool m_loaded = false;
    QString m_errorString;

    QHash<uint32_t, std::shared_ptr<SpriteData>> m_cache; // cache zdekodowanych sprite'ow
    QHash<int, QString> m_dataUrlCache;                   // cache data-URL dla QML
    QHash<QString, QString> m_itemDataUrlCache;            // cache zlozonych itemow dla QML
};

#endif // SPRREADER_H
