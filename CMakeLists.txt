cmake_minimum_required(VERSION 3.16)

project(DewralMapEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick OpenGL REQUIRED)

# -----------------------------------------------------------------------------
# OTFormats - statyczna biblioteka parserow formatow OpenTibia
#
# Czyste parsowanie plikow klienta/serwera: .spr (sprite'y), .dat (metadane
# itemow), .otb (mapowanie server<->client id), .otbm (mapa), .otfi (nadpisanie
# autodetekcji formatu). Zero UI - zalezy tylko od Qt6::Core i Qt6::Gui (QImage).
# Modele (QAbstractListModel) wystawiane jako API - konsument podlacza je do
# QML we wlasnym projekcie.
# -----------------------------------------------------------------------------
add_library(OTFormats STATIC
    libs/otformats/sprreader.cpp
    libs/otformats/sprreader.h
    libs/otformats/datreader.cpp
    libs/otformats/datreader.h
    libs/otformats/otbreader.cpp
    libs/otformats/otbreader.h
    libs/otformats/otbmreader.cpp
    libs/otformats/otbmreader.h
    libs/otformats/nodefilereader.cpp
    libs/otformats/nodefilereader.h
    libs/otformats/binaryreader.cpp
    libs/otformats/binaryreader.h
    libs/otformats/canonicalflags.h
    libs/otformats/itemtype.h
    libs/otformats/otfireader.cpp
    libs/otformats/otfireader.h
)

target_link_libraries(OTFormats
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml    # dla QAbstractListModel/QVariant
)

target_include_directories(OTFormats PUBLIC libs/otformats)

# -----------------------------------------------------------------------------
# DewralMapEditor - edytor map (aplikacja QML). Binarka: DME.exe
#
# Struktura modulow:
#   editor/core/  - logika domenowa edytora bez UI (silnik brushy, tilesety
#                   palet, filtr palety, narzedzia plikowe)
#   editor/map/   - widok mapy: MapView (kontroler: dane, edycja, wejscie)
#                   + MapGLView (renderer OpenGL z instancingiem)
#   editor/qml/   - caly interfejs (QML)
# -----------------------------------------------------------------------------
add_executable(DewralMapEditor
    editor/main.cpp
    editor/core/brushstore.cpp
    editor/core/brushstore.h
    editor/core/tilesetstore.cpp
    editor/core/tilesetstore.h
    editor/core/palettefilter.cpp
    editor/core/palettefilter.h
    editor/core/filetools.cpp
    editor/core/filetools.h
    editor/core/uitheme.cpp
    editor/core/uitheme.h
    editor/core/documentmanager.cpp
    editor/core/documentmanager.h
    editor/core/creaturestore.cpp
    editor/core/creaturestore.h
    editor/map/mapview.cpp
    editor/map/mapview_edit.cpp
    editor/map/mapview_chunks.cpp
    editor/map/mapview_atlas.cpp
    editor/map/mapview_gl.cpp
    editor/map/mapview_input.cpp
    editor/map/mapview.h
    editor/map/mapview_p.h
    editor/map/mapglview.cpp
    editor/map/mapglview.h
    editor/resources.qrc
)

target_include_directories(DewralMapEditor PRIVATE editor/core editor/map)

target_link_libraries(DewralMapEditor
    PRIVATE
        OTFormats
        Qt6::Quick
        Qt6::OpenGL
)

# Binarka nazywa sie DME.exe (skrot), a nie DewralMapEditor.exe.
set_target_properties(DewralMapEditor PROPERTIES OUTPUT_NAME "DME")

# -----------------------------------------------------------------------------
# Kopiuje folder data/ (brushes.json + tilesets.json per wersja klienta) obok
# zbudowanego .exe, zeby BrushStore/TilesetStore (applicationDirPath()/data/...)
# je znalazly. Kopiowane po KAZDYM buildzie - zmiany w source data/ od razu widoczne.
# -----------------------------------------------------------------------------
add_custom_command(TARGET DewralMapEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:DewralMapEditor>/data"
)
