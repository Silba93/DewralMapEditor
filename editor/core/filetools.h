#ifndef FILETOOLS_H
#define FILETOOLS_H

#include <QObject>
#include <QString>
#include <QUrl>

// -----------------------------------------------------------------------------
// FileTools
//
// Drobne pomocniki dla QML startowego loadera: skanowanie folderu klienta w
// poszukiwaniu plikow .dat/.spr/.otb, sprawdzanie istnienia, ladne nazwy.
// -----------------------------------------------------------------------------
class FileTools : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    Q_INVOKABLE bool exists(const QString &path) const;
    // Pierwszy plik o danym rozszerzeniu w folderze (pelna sciezka) lub "".
    // Jesli podano preferowana nazwe (np. "Tibia.dat"), bierze ja gdy istnieje.
    Q_INVOKABLE QString findByExt(const QString &folder, const QString &ext,
                                  const QString &preferred = QString()) const;
    Q_INVOKABLE QString fileName(const QString &path) const;   // sama nazwa pliku
    Q_INVOKABLE QString dirName(const QString &path) const;     // folder zawierajacy
    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void setClipboard(const QString &text) const;   // kopiuj tekst do schowka
};

#endif // FILETOOLS_H
