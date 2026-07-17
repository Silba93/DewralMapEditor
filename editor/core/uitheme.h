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

public:
    explicit UiTheme(QObject *parent = nullptr);

    QColor tint() const;
    void setTint(const QColor &c);

    QString tex() const;
    QVariantList presets() const;

    // Tekstura z qrc:/ui/<file> po przebarwieniu. Wolane przez provider, ktory moze
    // dzialac na innym watku niz GUI - stad mutex na m_tint.
    QImage texture(const QString &file) const;

signals:
    void themeChanged();

private:
    mutable QMutex m_mutex;
    QColor m_tint;
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
