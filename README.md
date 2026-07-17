# Dewral Map Editor (DME)

An OpenTibia (**OTBM**) map editor built with **Qt 6 / QML** and a custom
**OpenGL instanced renderer**. Inspired by, and data-compatible with,
[Remere's Map Editor](https://github.com/hampusborgos/rme) (RME), with a few
ideas borrowed from the [TIME](https://github.com/Open-Tibia-Tools/tibia-imgui-map-editor)
imgui editor (e.g. lighting preview).

The build produces a single executable: **`DME.exe`**.

> ⚠️ **Client assets are not included.** DME reads the CipSoft Tibia client
> files (`Tibia.spr`, `Tibia.dat`, `items.otb`). These are copyrighted and are
> **not** part of this repository — supply your own locally. See
> [Running](#running).

---

## Features

- Opens `.otbm` maps for clients **7.72 – 10.98+** — the client version is
  auto-detected from the OTBM header, with per-version client folders (like RME).
- **Fast OpenGL instanced renderer** — large maps, smooth pan/zoom, multi-floor
  drawing with elevation offsets, night/lighting preview.
- **RME-style palettes** — Terrain / Doodad / Item / RAW / Creature / House
  plus custom palettes; tilesets imported from RME `tilesets.xml` into JSON.
- **Ground brushes with automatic borders** — a faithful port of RME's
  `GroundBrush::doBorders`, with data converted from `grounds.xml`/`borders.xml`.
- **Doodad brushes** with multi-tile composite stamps and deterministic
  variant rotation (`R`).
- Editing: paint (with shift-drag rectangles and drag-line interpolation),
  select (single / multi-floor modes), move, delete, cut/copy/paste,
  undo/redo, stackable counts.
- **Spawns & creatures**, **houses** (assigned to towns), **towns editor**,
  Go To Position, map properties/statistics.
- Multiple maps open at once (tabs), classic-Tibia themed UI with a
  configurable colour theme and icon size.

---

## Technology stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| UI framework | Qt 6 (Core, Gui, **Qml**, **Quick**, **OpenGL**) |
| UI markup | QML (declarative) |
| Rendering | Raw **OpenGL** via `QQuickFramebufferObject`, sprite **instancing** |
| Build system | CMake ≥ 3.16 (Ninja) |
| Toolchain | MinGW 13 (developed on Windows 11); portable to other Qt 6 platforms |
| Data | JSON (brushes/tilesets), XML (creatures/towns, RME-compatible) |

**Why a custom GL renderer?** Tibia maps are dense, multi-layered sprite grids.
Instead of a scene graph, the map is drawn as a small set of instanced quads
sampling a CPU-built sprite atlas — this keeps large maps at high frame rates
with cheap pan/zoom. The QML layer is purely the editor shell (menus, palettes,
dialogs); the map surface is a single GL item.

---

## Project structure

```
DewralMapEditor/
├── CMakeLists.txt          root build: OTFormats lib + DME executable
│
├── libs/otformats/         OTFormats — static library, pure file-format parsers
│   ├── sprreader.*           .spr  sprite pixels
│   ├── datreader.*           .dat  item metadata (size, layers, light, flags)
│   ├── otbreader.*           .otb  server-id ↔ client-id mapping
│   ├── otbmreader.*          .otbm map (tiles, items, houses, spawns, towns)
│   ├── otfireader.*          .otfi format-autodetection override
│   ├── nodefilereader.*      OTBM node-tree binary framing
│   └── binaryreader.*        low-level little-endian reader
│                           No UI deps — only Qt6::Core / Gui (QImage) / Qml.
│
├── editor/                 the application
│   ├── main.cpp              entry point; registers QML types + context props
│   ├── core/                editor domain logic (no UI):
│   │   ├── brushstore.*        ground-brush engine + auto-border algorithm
│   │   ├── tilesetstore.*      palette tilesets (data/<ver>/tilesets.json)
│   │   ├── palettefilter.*     proxy model filtering the item palette
│   │   ├── creaturestore.*     creature/NPC palette (RME creatures.xml)
│   │   ├── documentmanager.*   open-map tabs (one OtbmReader per map)
│   │   ├── filetools.*         small file/clipboard helpers for QML
│   │   └── uitheme.*           UI colour theme (multiply tint over textures)
│   │
│   ├── map/                 map view — one controller class, split by concern:
│   │   ├── mapview.h/.cpp      MapView (QQuickItem): setup, load, view centering
│   │   ├── mapview_edit.cpp      painting, brushes, borders, undo/redo
│   │   ├── mapview_atlas.cpp     incremental CPU sprite atlas
│   │   ├── mapview_chunks.cpp    floor/chunk index, quad cache, worker
│   │   ├── mapview_gl.cpp        data API consumed by the GL renderer
│   │   ├── mapview_input.cpp     mouse / keyboard / zoom / selection
│   │   ├── mapview_p.h           shared private helpers
│   │   └── mapglview.*         MapGLView (QQuickFramebufferObject, GL renderer)
│   │
│   ├── qml/                 UI (declarative):
│   │   ├── Main.qml            window, menus, toolbar, map area, tabs
│   │   ├── PalettePanel.qml    left palette (categories, tilesets, search)
│   │   ├── StartupWindow.qml   startup loader (recent maps, client folders)
│   │   ├── style/             reusable classic-UI controls (buttons, combos…)
│   │   └── dialogs/           Towns, Go To, Map Properties/Stats, Item props…
│   │
│   ├── ui/                  classic-Tibia UI textures (9-slice borders, icons)
│   └── resources.qrc        Qt resource bundle (QML + textures)
│
└── data/<version>/         per-client-version data, converted from RME:
    ├── brushes.json           ground brushes + border definitions
    ├── tilesets.json          palette tileset contents
    ├── creatures.xml          creature list (RME format)
    └── creature_palette.xml   creature palette layout
```

**Data flow (map → pixels):**

```
OtbmTile.items[].server_id
    → OtbReader   (server-id → client-id)
    → DatReader   (sprite ids, size, layers, light, offsets)
    → SprReader   (pixels)
    → CPU sprite atlas  (mapview_atlas)
    → GL instances      (mapview_gl → MapGLView)
```

---

## Building

**Requirements:** Qt 6 (Core, Gui, Qml, Quick, OpenGL), CMake 3.16+, a C++17
compiler (developed with MinGW 13 on Windows).

```sh
cmake -B build -S . -G Ninja
cmake --build build
```

The `data/` directory (per-client-version brush and tileset definitions) is
copied next to the executable after every build. The resulting binary is
`build/DME.exe`.

## Running

DME needs a Tibia client version's asset files, which are **not** shipped:

- `Tibia.spr`, `Tibia.dat` — the client sprites and item metadata
- `items.otb` — the server↔client item-id map

On first launch the startup window asks you to point at the folder containing
these files for the detected client version. Paths are remembered per version.

---

## License / Credits

- Brush, border, tileset and creature **data** derive from
  **Remere's Map Editor** (GPL). RME's `GroundBrush::doBorders` and its border
  lookup tables were ported to `editor/core/brushstore.cpp`.
- The lighting/night preview is modelled on the
  [TIME](https://github.com/Open-Tibia-Tools/tibia-imgui-map-editor) editor.
- **Tibia**, the `.spr`/`.dat` client formats and all client artwork are
  © CipSoft GmbH. This project ships none of that content.
