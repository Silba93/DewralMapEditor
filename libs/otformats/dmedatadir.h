#ifndef DMEDATADIR_H
#define DMEDATADIR_H

#include <QCoreApplication>
#include <QDir>
#include <QString>

// -----------------------------------------------------------------------------
// Katalog data/ edytora (brushes.json, tilesets.json, creatures.xml, items.xml).
//
// W buildach deweloperskich CMake definiuje DME_DATA_DIR = <zrodla>/data i
// aplikacja czyta ORAZ ZAPISUJE bezposrednio tam. Powod: wczesniej zapis szedl
// do data/ obok binarki (build/), a POST_BUILD copy_directory nadpisywal go
// zawartoscia zrodel przy kazdym buildzie - edycje palet/brushy robione w
// aplikacji "znikaly" i nigdy nie pojawialy sie w repo. Teraz pliki laduja od
// razu w zrodlach (widoczne w gicie), a build nie ma czego nadpisywac.
//
// Bez definicji (build wydaniowy/paczka) - fallback do data/ obok binarki.
// -----------------------------------------------------------------------------
inline QString dmeDataDir()
{
#ifdef DME_DATA_DIR
    return QStringLiteral(DME_DATA_DIR);
#else
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("data"));
#endif
}

#endif // DMEDATADIR_H
