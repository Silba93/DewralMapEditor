#include "filetools.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>

bool FileTools::exists(const QString &path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

QString FileTools::findByExt(const QString &folder, const QString &ext,
                             const QString &preferred) const
{
    if (folder.isEmpty()) {
        return QString();
    }
    QDir dir(folder);
    if (!dir.exists()) {
        return QString();
    }

    if (!preferred.isEmpty() && dir.exists(preferred)) {
        return dir.absoluteFilePath(preferred);
    }

    const QStringList matches = dir.entryList({QStringLiteral("*.%1").arg(ext)},
                                              QDir::Files, QDir::Name);
    if (matches.isEmpty()) {
        return QString();
    }
    return dir.absoluteFilePath(matches.first());
}

QString FileTools::findToml(const QString &path) const
{
    if (path.isEmpty()) return QString();

    const QFileInfo input(path);
    if (input.isFile()) {
        return input.suffix().compare(QStringLiteral("toml"), Qt::CaseInsensitive) == 0
                   ? input.absoluteFilePath() : QString();
    }
    if (!input.isDir()) return QString();

    const QDir dir(input.absoluteFilePath());
    const QString direct = dir.filePath(QStringLiteral("items.toml"));
    if (QFileInfo::isFile(direct)) return QFileInfo(direct).absoluteFilePath();

    const QString nested = dir.filePath(QStringLiteral("items/items.toml"));
    if (QFileInfo::isFile(nested)) return QFileInfo(nested).absoluteFilePath();
    return QString();
}

QString FileTools::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString FileTools::dirName(const QString &path) const
{
    return QFileInfo(path).absolutePath();
}

QString FileTools::canonicalPath(const QString &path) const
{
    if (path.isEmpty()) return QString();
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

void FileTools::setClipboard(const QString &text) const
{
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(text);
    }
}

QString FileTools::clipboardText() const
{
    if (const QClipboard *cb = QGuiApplication::clipboard()) {
        return cb->text();
    }
    return QString();
}
