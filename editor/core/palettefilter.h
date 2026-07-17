#ifndef PALETTEFILTER_H
#define PALETTEFILTER_H

#include <QSortFilterProxyModel>
#include <QSet>
#include <QVariantList>

// -----------------------------------------------------------------------------
// PaletteFilter
//
// Proxy nad OtbReader do palety itemow: filtruje po zbiorze server id (tileset
// RME / wlasna paleta uzytkownika) i/lub tekscie wyszukiwania (nazwa lub id).
// Przekazuje role zrodla 1:1, wiec delegat GridView pozostaje bez zmian.
// mode: "all" = wszystkie itemy | "ids" = tylko id z setIds().
// -----------------------------------------------------------------------------
class PaletteFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
    explicit PaletteFilter(QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    void setMode(const QString &m);
    QString searchText() const { return m_search; }
    void setSearchText(const QString &t);

    // Zbior server id dla mode="ids" (kolejnosc zrodla; duplikaty ignorowane).
    Q_INVOKABLE void setIds(const QVariantList &ids);
    // Wiersz proxy dla server id (auto-scroll palety do aktywnego pedzla). -1 = brak.
    Q_INVOKABLE int rowForServerId(int serverId) const;

signals:
    void modeChanged();
    void searchTextChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_mode = QStringLiteral("all");
    QString m_search;
    QSet<int> m_ids;
};

#endif // PALETTEFILTER_H
