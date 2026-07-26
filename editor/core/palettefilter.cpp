#include "palettefilter.h"
#include "otbreader.h"

#include <utility>

PaletteFilter::PaletteFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void PaletteFilter::setMode(const QString &m)
{
    if (m_mode == m) return;
    beginFilterChange();
    m_mode = m;
    endFilterChange(Direction::Rows);
    emit modeChanged();
}

void PaletteFilter::setSearchText(const QString &t)
{
    if (m_search == t) return;
    beginFilterChange();
    m_search = t;
    endFilterChange(Direction::Rows);
    emit searchTextChanged();
}

void PaletteFilter::setIds(const QVariantList &ids)
{
    QSet<int> newIds;
    newIds.reserve(ids.size());
    for (const QVariant &v : ids) newIds.insert(v.toInt());

    const bool idsChanged = m_ids != newIds;
    const bool modeWasChanged = m_mode != QLatin1String("ids");
    if (!idsChanged && !modeWasChanged) return;

    beginFilterChange();
    m_ids = std::move(newIds);
    if (modeWasChanged) {
        m_mode = QStringLiteral("ids");
    }
    endFilterChange(Direction::Rows);
    if (modeWasChanged) emit modeChanged();
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
