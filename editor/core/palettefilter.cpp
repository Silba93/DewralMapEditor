#include "palettefilter.h"
#include "otbreader.h"

PaletteFilter::PaletteFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void PaletteFilter::setMode(const QString &m)
{
    if (m_mode == m) return;
    m_mode = m;
    emit modeChanged();
    invalidateFilter();
}

void PaletteFilter::setSearchText(const QString &t)
{
    if (m_search == t) return;
    m_search = t;
    emit searchTextChanged();
    invalidateFilter();
}

void PaletteFilter::setIds(const QVariantList &ids)
{
    m_ids.clear();
    for (const QVariant &v : ids) m_ids.insert(v.toInt());
    invalidateFilter();
}

int PaletteFilter::rowForServerId(int serverId) const
{
    auto *otb = qobject_cast<OtbReader *>(sourceModel());
    if (!otb) return -1;
    const int srcRow = otb->rowForServerId(serverId);
    if (srcRow < 0) return -1;
    const QModelIndex proxyIdx = mapFromSource(otb->index(srcRow, 0));
    return proxyIdx.isValid() ? proxyIdx.row() : -1;
}

bool PaletteFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *src = sourceModel();
    if (!src) return false;
    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (m_mode == QLatin1String("ids")) {
        const int sid = idx.data(OtbReader::ServerIdRole).toInt();
        if (!m_ids.contains(sid)) return false;
    }

    if (!m_search.isEmpty()) {
        const QString name = idx.data(OtbReader::NameRole).toString();
        const QString sid = idx.data(OtbReader::ServerIdRole).toString();
        if (!name.contains(m_search, Qt::CaseInsensitive) && !sid.startsWith(m_search))
            return false;
    }
    return true;
}
