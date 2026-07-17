#include "sprreader.h"

#include <QFile>
#include <QBuffer>
#include <QIODevice>
#include <QPainter>

namespace {

QString imageToDataUrl(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

QString makeItemCacheKey(const QVariantList &spriteIds, int itemWidth, int itemHeight, int layers)
{
    QString key = QStringLiteral("%1x%2:%3|").arg(itemWidth).arg(itemHeight).arg(layers);
    for (const QVariant &spriteId : spriteIds) {
        key += QString::number(spriteId.toUInt());
        key += QLatin1Char(',');
    }
    return key;
}

} // namespace

// -----------------------------------------------------------------------------
// SpriteData::decode
//
// Odpowiednik SpriteData::decode() z oryginalnego API. Dekoduje surowe dane
// RLE pod danym offsetem w pliku do QImage RGBA8888.
//
// use_transparency w oryginale przelacza kodowanie koloru-klucza (magenta).
// W formacie .spr przezroczystosc jest jednak kodowana strukturalnie przez
// RLE (transparent_pixel_count), a 3-bajtowy "colorkey" naglowka sprite'a
// to legacy pole z bardzo starych klientow - tutaj go odczytujemy (zeby
// poprawnie przesunac wskaznik strumienia), ale o przezroczystosci decyduje
// wylacznie struktura RLE, zgodnie z faktycznym formatem plikow 7.72.
//
// useAlpha (flaga "transparency" z .otfi, jak RME): gdy true, kazdy kolorowy
// piksel ma 4 bajty (RGBA - realny kanal alpha z pliku) zamiast domyslnych
// 3 (RGB, alpha zawsze 255 dla kolorowych pikseli).
// -----------------------------------------------------------------------------
bool SpriteData::decode(const QByteArray &fileData, uint32_t offset, int spriteSize, bool useAlpha)
{
    image = QImage(spriteSize, spriteSize, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    if (offset == 0) {
        is_empty = true;
        return true; // pusty sprite to poprawny, oczekiwany stan
    }

    const qint64 fileSize = fileData.size();
    if (static_cast<qint64>(offset) >= fileSize) {
        is_empty = true;
        return false;
    }

    const uchar *raw = reinterpret_cast<const uchar *>(fileData.constData());
    qint64 pos = static_cast<qint64>(offset);

    // 3 bajty colorkey (RGB) - legacy pole, pomijamy wartosc
    if (pos + 3 > fileSize) { is_empty = true; return false; }
    pos += 3;

    // 2 bajty: compressed_size (uint16 LE)
    if (pos + 2 > fileSize) { is_empty = true; return false; }
    uint16_t compressedSize = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
    pos += 2;

    const qint64 dataStart = pos;
    const qint64 dataEnd = qMin(dataStart + static_cast<qint64>(compressedSize), fileSize);

    const int totalPixels = spriteSize * spriteSize;
    int pixelIndex = 0;

    while (pos + 4 <= dataEnd && pixelIndex < totalPixels) {
        uint16_t transparentCount = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
        pos += 2;
        uint16_t coloredCount = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
        pos += 2;

        // Piksele przezroczyste - alpha juz 0 z fill(), przesuwamy tylko indeks
        pixelIndex = qMin(pixelIndex + static_cast<int>(transparentCount), totalPixels);

        const int bpp = useAlpha ? 4 : 3;   // RGBA (realny alpha) vs RGB (alpha=255)
        for (int i = 0; i < coloredCount && pixelIndex < totalPixels; ++i) {
            if (pos + bpp > dataEnd) break;

            uchar r = raw[pos];
            uchar g = raw[pos + 1];
            uchar b = raw[pos + 2];
            uchar a = useAlpha ? raw[pos + 3] : 255;
            pos += bpp;

            int x = pixelIndex % spriteSize;
            int y = pixelIndex / spriteSize;
            image.setPixelColor(x, y, QColor(r, g, b, a));

            ++pixelIndex;
        }
    }

    is_empty = false;
    return true;
}

// -----------------------------------------------------------------------------
// SprReader
// -----------------------------------------------------------------------------

SprReader::SprReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

SprReader::~SprReader() = default;

void SprReader::reset()
{
    beginResetModel();
    m_fileData.clear();
    m_offsets.clear();
    m_signature = 0;
    m_spriteCount = 0;
    m_extended = false;
    m_cache.clear();
    m_dataUrlCache.clear();
    m_itemDataUrlCache.clear();
    m_loaded = false;
    endResetModel();

    emit spriteCountChanged();
    emit loadedChanged();
}

void SprReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool SprReader::loadFile(const QString &path, quint32 expectedSignature, bool extended, bool useAlpha)
{
    reset();
    m_extended = extended;
    m_useAlpha = useAlpha;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Nie mozna otworzyc pliku: %1").arg(path));
        return false;
    }

    m_fileData = file.readAll();
    file.close();

    // Minimalny naglowek: 4B signature + (2B lub 4B) sprite_count
    const int headerCountSize = m_extended ? 4 : 2;
    const qint64 minHeaderSize = 4 + headerCountSize;

    if (m_fileData.size() < minHeaderSize) {
        setError(QStringLiteral("Plik jest za maly, by byc poprawnym .spr"));
        m_fileData.clear();
        return false;
    }

    const uchar *raw = reinterpret_cast<const uchar *>(m_fileData.constData());

    // 4 bajty signature (uint32 LE)
    m_signature = static_cast<uint32_t>(raw[0])
                | (static_cast<uint32_t>(raw[1]) << 8)
                | (static_cast<uint32_t>(raw[2]) << 16)
                | (static_cast<uint32_t>(raw[3]) << 24);

    if (expectedSignature != 0 && m_signature != expectedSignature) {
        setError(QStringLiteral("Nieprawidlowa sygnatura pliku .spr (oczekiwano 0x%1, otrzymano 0x%2)")
                      .arg(expectedSignature, 0, 16)
                      .arg(m_signature, 0, 16));
        m_fileData.clear();
        return false;
    }

    qint64 pos = 4;

    // sprite_count: 2 bajty dla 7.72 (extended=false), 4 bajty dla extended
    if (m_extended) {
        m_spriteCount = static_cast<uint32_t>(raw[pos])
                       | (static_cast<uint32_t>(raw[pos + 1]) << 8)
                       | (static_cast<uint32_t>(raw[pos + 2]) << 16)
                       | (static_cast<uint32_t>(raw[pos + 3]) << 24);
        pos += 4;
    } else {
        m_spriteCount = static_cast<uint32_t>(raw[pos])
                       | (static_cast<uint32_t>(raw[pos + 1]) << 8);
        pos += 2;
    }

    // Tablica offsetow: sprite_count wpisow po 4 bajty (uint32 LE)
    const qint64 offsetsStart = pos;
    const qint64 offsetsBytes = static_cast<qint64>(m_spriteCount) * 4;

    if (offsetsStart + offsetsBytes > m_fileData.size()) {
        setError(QStringLiteral("Plik jest uszkodzony - tablica offsetow wykracza poza plik"));
        m_fileData.clear();
        m_spriteCount = 0;
        return false;
    }

    m_offsets.reserve(static_cast<int>(m_spriteCount));

    for (uint32_t i = 0; i < m_spriteCount; ++i) {
        qint64 p = offsetsStart + static_cast<qint64>(i) * 4;
        uint32_t offset = static_cast<uint32_t>(raw[p])
                         | (static_cast<uint32_t>(raw[p + 1]) << 8)
                         | (static_cast<uint32_t>(raw[p + 2]) << 16)
                         | (static_cast<uint32_t>(raw[p + 3]) << 24);
        m_offsets.append(offset);
    }

    m_loaded = true;
    emit spriteCountChanged();
    emit loadedChanged();

    beginInsertRows(QModelIndex(), 0, static_cast<int>(m_spriteCount) - 1);
    endInsertRows();

    return true;
}

std::shared_ptr<SpriteData> SprReader::loadSprite(uint32_t spriteId)
{
    // sprite ID sa 1-indeksowane; ID 0 lub poza zakresem -> pusty sprite
    if (spriteId < 1 || spriteId > m_spriteCount) {
        auto empty = std::make_shared<SpriteData>();
        empty->id = spriteId;
        empty->decode(m_fileData, 0, kDefaultSpriteSize, m_useAlpha);
        return empty;
    }

    auto it = m_cache.find(spriteId);
    if (it != m_cache.end()) {
        return it.value();
    }

    auto sprite = std::make_shared<SpriteData>();
    sprite->id = spriteId;

    uint32_t offset = m_offsets.at(static_cast<int>(spriteId) - 1);
    sprite->decode(m_fileData, offset, kDefaultSpriteSize, m_useAlpha);

    m_cache.insert(spriteId, sprite);
    return sprite;
}

QImage SprReader::spriteImage(int spriteId)
{
    auto sprite = loadSprite(static_cast<uint32_t>(spriteId));
    return sprite->image;
}

QString SprReader::spriteImageSource(int spriteId)
{
    auto cached = m_dataUrlCache.find(spriteId);
    if (cached != m_dataUrlCache.end()) {
        return cached.value();
    }

    QImage img = spriteImage(spriteId);
    QString dataUrl = imageToDataUrl(img);
    m_dataUrlCache.insert(spriteId, dataUrl);
    return dataUrl;
}

QString SprReader::itemImageSource(const QVariantList &spriteIds,
                                   int itemWidth,
                                   int itemHeight,
                                   int layers)
{
    const int width = qMax(1, itemWidth);
    const int height = qMax(1, itemHeight);
    const int layerCount = qMax(1, layers);

    if (spriteIds.isEmpty()) {
        return spriteImageSource(0);
    }

    const QString key = makeItemCacheKey(spriteIds, width, height, layerCount);
    auto cached = m_itemDataUrlCache.find(key);
    if (cached != m_itemDataUrlCache.end()) {
        return cached.value();
    }

    QImage composite(width * kDefaultSpriteSize,
                     height * kDefaultSpriteSize,
                     QImage::Format_RGBA8888);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (int layer = 0; layer < layerCount; ++layer) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                const int spriteIndex = ((layer * height) + h) * width + w;
                if (spriteIndex < 0 || spriteIndex >= spriteIds.size()) {
                    continue;
                }

                const uint32_t spriteId = spriteIds.at(spriteIndex).toUInt();
                if (spriteId == 0) {
                    continue;
                }

                auto sprite = loadSprite(spriteId);
                if (!sprite || sprite->image.isNull()) {
                    continue;
                }

                const int destX = (width - w - 1) * kDefaultSpriteSize;
                const int destY = (height - h - 1) * kDefaultSpriteSize;
                painter.drawImage(destX, destY, sprite->image);
            }
        }
    }

    painter.end();

    const QString dataUrl = imageToDataUrl(composite);
    m_itemDataUrlCache.insert(key, dataUrl);
    return dataUrl;
}


int SprReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_spriteCount);
}

QVariant SprReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_spriteCount)) {
        return QVariant();
    }

    const int spriteId = index.row() + 1; // model row 0 -> sprite ID 1

    // data() jest const w QAbstractListModel, ale lazy-load wymaga mutacji
    // cache'u - rzutujemy const-away tak jak robi to wzorzec "mutable cache"
    // (analogicznie do mutable cache_ w oryginalnym SprReader z repo).
    auto *self = const_cast<SprReader *>(this);

    switch (role) {
    case SpriteIdRole:
        return spriteId;
    case SpriteImageRole:
        return self->spriteImage(spriteId);
    case SpriteImageSourceRole:
        return self->spriteImageSource(spriteId);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SprReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SpriteIdRole] = "spriteId";
    roles[SpriteImageRole] = "spriteImage";
    roles[SpriteImageSourceRole] = "spriteImageSource";
    return roles;
}
