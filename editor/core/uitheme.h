#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>
#include <QVariantList>

// -----------------------------------------------------------------------------
// UiTheme
//
// Motyw kolorystyczny UI: jeden kolor nakladany per-kanal MULTIPLY na tekstury
// classic UI (qrc:/ui/*.png - panele, przyciski, menu, scrollbary). Tekstury sa
// szaro-brazowym pixel-artem, wiec mnozenie przez kolor przebarwia je ZACHOWUJAC
// cieniowanie i bevel. Tak samo robi otcv8 ("custom UI color"). Biel = brak zmiany.
//
// Dlaczego provider obrazkow, a nie efekt QML:
//  - ColorOverlay zastepuje kolor plaska plama i kasuje detal tekstury,
//  - MultiEffect wymaga wlasnej warstwy (FBO) na KAZDY element UI.
// Tutaj przebarwiamy raz przy zmianie motywu, a samo rysowanie kosztuje dokladnie
// tyle co bez motywu.
//
// Mnozenie moze tylko przyciemniac/przebarwiac - nie rozjasni tekstury. To celowe:
// gwarantuje, ze zaden motyw nie "wypali" pixel-artu na biale placki.
// -----------------------------------------------------------------------------
class UiTheme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor tint READ tint WRITE setTint NOTIFY themeChanged)
    // Prefiks URL tekstur, np. "image://tibiaui/3/". Zmienia sie razem z motywem,
    // wiec bindingi `source: uiTheme.tex + "panel_flat.png"` same przeladuja obrazki.
    // Wersja w URL jest po to, by ominac cache QQuickPixmap (kluczuje po URL) -
    // bez niej zmiana motywu nie bylaby widoczna do restartu.
    Q_PROPERTY(QString tex READ tex NOTIFY themeChanged)
    Q_PROPERTY(QVariantList presets READ presets CONSTANT)
    // Styl calego UI: "classic" = kamienne tekstury z qrc, "flat" = plaski ciemny
    // (GitHub dark) SYNTETYZOWANY w locie - te same nazwy plikow i metryki border,
    // wiec ZERO zmian w QML (BorderImage tnie tak samo). Patrz flatTexture().
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY themeChanged)
    Q_PROPERTY(QVariantList styles READ styles CONSTANT)

public:
    explicit UiTheme(QObject *parent = nullptr);

    QColor tint() const;
    void setTint(const QColor &c);

    QString tex() const;
    QVariantList presets() const;
    QString style() const;
    void setStyle(const QString &s);
    QVariantList styles() const;

    // Tekstura z qrc:/ui/<file> po przebarwieniu. Wolane przez provider, ktory moze
    // dzialac na innym watku niz GUI - stad mutex na m_tint.
    QImage texture(const QString &file) const;

signals:
    void themeChanged();

private:
    // Synteza plaskiej tekstury (styl "flat") dla nazwy pliku classic UI: te same
    // metryki border, stany rozrozniane po slowach w nazwie (hover/pressed/active/
    // checked), specjalne przypadki (popupwindow z paskiem tytulu, strzalki
    // spinboxa, separatory). Nieznane pliki dostaja generyczny ciemny panel -
    // nowe tekstury dzialaja w flat od razu, bez dopisywania.
    QImage flatTexture(const QString &file) const;

    mutable QMutex m_mutex;
    QColor m_tint;
    QString m_style;   // "classic" | "flat"
    int m_version = 0;
};

// Provider dla URL-i "image://tibiaui/<wersja>/<plik.png>".
class UiThemeImageProvider : public QQuickImageProvider
{
public:
    explicit UiThemeImageProvider(UiTheme *theme);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    UiTheme *m_theme;
};

#endif // UITHEME_H
