#include "uitheme.h"

#include <QSettings>
#include <QVariantMap>

namespace {

// Klucz w QSettings (org/app ustawione w main.cpp).
const char *kTintKey = "ui/tint";

struct Preset { const char *name; const char *color; };

// Kolory dobrane pod MNOZENIE na szaro-brazowych teksturach: swiadomie jasne, bo
// mnozenie i tak je przyciemni. Ciemniejsze wartosci daja czarne, nieczytelne panele.
const Preset kPresets[] = {
    { "Klasyczny",  "#ffffff" },   // biel = tekstura bez zmian
    { "Zielony",    "#8fd08f" },
    { "Niebieski",  "#8fa8e0" },
    { "Czerwony",   "#e08f8f" },
    { "Fioletowy",  "#b78fe0" },
    { "Zloty",      "#e0c88f" },
    { "Turkusowy",  "#8fd8d0" },
    { "Grafitowy",  "#9a9aa5" },
};

} // namespace

UiTheme::UiTheme(QObject *parent)
    : QObject(parent)
    , m_tint(Qt::white)
{
    const QString saved = QSettings().value(QLatin1String(kTintKey)).toString();
    const QColor c(saved);
    if (c.isValid()) {
        m_tint = c;
    }
}

QColor UiTheme::tint() const
{
    QMutexLocker lock(&m_mutex);
    return m_tint;
}

void UiTheme::setTint(const QColor &c)
{
    const QColor v = c.isValid() ? c : QColor(Qt::white);
    {
        QMutexLocker lock(&m_mutex);
        if (m_tint == v) {
            return;
        }
        m_tint = v;
    }
    ++m_version;   // nowy prefiks URL -> QML przeladuje tekstury z pominieciem cache
    QSettings().setValue(QLatin1String(kTintKey), v.name());
    emit themeChanged();
}

QString UiTheme::tex() const
{
    return QStringLiteral("image://tibiaui/%1/").arg(m_version);
}

QVariantList UiTheme::presets() const
{
    QVariantList out;
    for (const Preset &p : kPresets) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), QString::fromLatin1(p.name));
        m.insert(QStringLiteral("color"), QString::fromLatin1(p.color));
        out.push_back(m);
    }
    return out;
}

QImage UiTheme::texture(const QString &file) const
{
    QImage img(QStringLiteral(":/ui/") + file);
    if (img.isNull()) {
        return img;
    }

    QColor t;
    {
        QMutexLocker lock(&m_mutex);
        t = m_tint;
    }
    if (t == QColor(Qt::white)) {
        return img;   // brak przebarwienia - oddaj oryginal
    }

    // ARGB32 (NIE premultiplied): kanaly RGB sa wtedy "proste", wiec mnozenie nie
    // wymaga uwzgledniania alpha. Recznie zamiast QPainter+CompositionMode_Multiply,
    // bo tam mnozenie miesza sie z alpha tla i zjadaloby przezroczystosc pixel-artu.
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int tr = t.red();
    const int tg = t.green();
    const int tb = t.blue();
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = line[x];
            line[x] = qRgba(qRed(c) * tr / 255,
                            qGreen(c) * tg / 255,
                            qBlue(c) * tb / 255,
                            qAlpha(c));   // alpha nietknieta - ksztalt/bordery zostaja
        }
    }
    return img;
}

// -----------------------------------------------------------------------------

UiThemeImageProvider::UiThemeImageProvider(UiTheme *theme)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_theme(theme)
{
}

QImage UiThemeImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id = "<wersja>/<plik.png>" - wersja jest tylko cache-busterem, pomijamy ja.
    const int slash = id.indexOf(QLatin1Char('/'));
    const QString file = (slash >= 0) ? id.mid(slash + 1) : id;

    const QImage img = m_theme->texture(file);
    if (size) {
        *size = img.size();
    }
    // requestedSize celowo ignorowane: to pixel-art dla BorderImage, ktory sam tnie
    // marginesy - przeskalowanie tutaj rozjechaloby bordery.
    Q_UNUSED(requestedSize);
    return img;
}
