#ifndef FILETOOLS_H
#define FILETOOLS_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class FileTools : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    using QObject::QObject;

    Q_INVOKABLE bool exists(const QString &path) const;

    Q_INVOKABLE QString findByExt(const QString &folder, const QString &ext,
                                  const QString &preferred = QString()) const;
    Q_INVOKABLE QString findToml(const QString &path) const;
    Q_INVOKABLE QString fileName(const QString &path) const;
    Q_INVOKABLE QString dirName(const QString &path) const;
    Q_INVOKABLE QString canonicalPath(const QString &path) const;
    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void setClipboard(const QString &text) const;
    Q_INVOKABLE QString clipboardText() const;

    // Position strings shared by the map context menu entries: "Copy Position
    // As" writes them and the paste actions read them back in every style.
    Q_INVOKABLE QString positionText(const QString &format, int x, int y, int z) const;
    Q_INVOKABLE QVariantMap positionFromText(const QString &text) const;
};

#endif
