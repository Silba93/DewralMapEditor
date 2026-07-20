#ifndef DOCUMENTMANAGER_H
#define DOCUMENTMANAGER_H

#include "otbmreader.h"

#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class DocumentManager : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY tabsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(OtbmReader *current READ current NOTIFY currentChanged)

    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)

public:
    explicit DocumentManager(QObject *parent = nullptr);

    int count() const { return static_cast<int>(m_docs.size()); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i);
    OtbmReader *current() const;
    QVariantList tabs() const;

    Q_INVOKABLE OtbmReader *newDocument();

    Q_INVOKABLE bool closeDocument(int i);

    Q_INVOKABLE int indexOfPath(const QString &path) const;

signals:
    void currentChanged();
    void tabsChanged();

private:
    void hookDocument(OtbmReader *doc);

    QVector<OtbmReader *> m_docs;
    int m_current = 0;
};

#endif
