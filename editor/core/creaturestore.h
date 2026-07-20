#ifndef CREATURESTORE_H
#define CREATURESTORE_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class CreatureStore : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY countChanged)

public:
    struct CreatureType {
        QString name;
        bool isNpc = false;
        int lookType = 0;
        int lookItem = 0;
        int lookHead = 0, lookBody = 0, lookLegs = 0, lookFeet = 0;
    };

    enum Roles {
        NameRole = Qt::UserRole + 1,
        IsNpcRole,
        LookTypeRole,
        LookItemRole,
    };

    explicit CreatureStore(QObject *parent = nullptr);

    Q_INVOKABLE bool loadForVersion(int version);

    Q_INVOKABLE bool loadForDir(const QString &dirName);

    int count() const { return static_cast<int>(m_creatures.size()); }
    bool hasData() const { return !m_creatures.isEmpty(); }

    const CreatureType *byName(const QString &name) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    bool loadFile(const QString &path);

    QVector<CreatureType> m_creatures;
};

#endif
