# HANDOVER — DeusEdix native port (macOS / ARM64)

Written for a Claude Code session picking this up cold. It states where things are, what was
done, what is verified, what is not, and what to do next.

---

## 1. The project in one paragraph

**DeusEdix** (`DeusEdix/`) is a third-party, browser-based map editor for *Deus Ex* (GOTY 1112fm)
and *Deus Ex: Revision*. It is a web page (three.js) driven by a local Python server, with a native
"headless host" that does the engine work. Its problem: that host links the game's own
`Editor.dll` — **32-bit x86 Windows PE** — so the editor cannot run on Apple Silicon.

**OpenDE** (`/Volumes/OWC/Games/OpenDE`) is a portable C++23 reimplementation of the Deus Ex
engine (same project family, MIT licensed). It already does the engine work natively.

**The goal of this work:** run DeusEdix natively on macOS/ARM64 by replacing DeusEdix's
Windows-only engine bridge with one backed by OpenDE.

**Progress: the port works.** The editor builds, stages, starts, builds its own UnrealScript
package and prepares a map's geometry on macOS ARM64. What is *not* done is the interactive
3D-view check in a browser and the "Play" (launch-the-game) feature.

---

## 2. The two repositories

| | OpenDE | DeusEdix |
|---|---|---|
| Path | `/Volumes/OWC/Games/OpenDE` | `/Volumes/OWC/Games/OpenDE/DeusEdix` (a **sibling checkout**, gitignored by OpenDE) |
| Upstream `origin` | `github.com/jarmitage99/OpenDE` | `github.com/Gabby16Bitbox/DeusEdix` |
| Your fork `fork` | `github.com/jmaccs/OpenDE` | `github.com/jmaccs/DeusEdix` |
| Branch | `feature/native-arm64-bridge` (also on `master`) | `feature/native-arm64-bridge` (`main` untouched) |
| Licence | MIT | **GPLv3** |
| Language | C++23, CMake | Python 3.10+, TypeScript/JS (three.js), C++17 (host/bridge) |

Both forks are pushed and in sync with their branches. `origin` is preserved for pulling upstream.

### Licence boundary — do not break this

DeusEdix is GPLv3, OpenDE is MIT. They **must not be merged into one binary** — that would force
OpenDE's MIT code to be GPLv3 and contradict OpenDE's grant.

They are kept separate and communicate over:
* the **plain C ABI** (`DeusEdix/mapkit/host/bridge.h`, ~50 function pointers), and
* the **file protocol** (`DeusEdix/mapkit/host/PROTOCOL.md`).

**One exception already made, and it needs the owner's decision:** DeusEdix's
`mapkit/native/NavRoute.h` (GPLv3, the author's own code) was transcribed into OpenDE as
`src/engine/nav_route.cc/.h`. Same author, so a dual-licence statement would clean it up, but
**neither licence file was edited** — that is the owner's call, not the agent's.

---

## 3. Architecture: what the port looks like

```
DeusEdix's page (three.js, in a browser)
   │  HTTP /api/* to 127.0.0.1:8790
   ▼
DeusEdix's Python server  (mapkit/editor/server.py)
   │  writes a command file, starts the host as a subprocess
   ▼
UCMapKitHost  (mapkit/host/UCMapKitHost.cpp — NOW PORTED TO POSIX)
   │  dlopen("UCEngineBridge.dylib" | ".so" | ".dll")
   │  speaks UCMapKit|<what>|<fields> on stdout, writes the files
   ▼
UCOpenBridge.dylib  (mapkit/host/openbridge/ — NEW, ~4100 lines)
   │  implements bridge.h against OpenDE
   ▼
OpenDE's engine  (libdx-engine.a, libdx-compiler.a)
```

The protocol and the ABI are **unchanged** — the editor's Python needed no protocol work.

### The Windows-only bridge still exists

`DeusEdix/mapkit/host/UCEngineBridge.cpp` (2634 lines) is the original, still Windows-only, still
the release build for Windows. It was **not** modified. The new bridge is a *second* bridge; the
host picks whichever it is given (`--bridge <path>`).

---

## 4. What was done, in order

### Phase 1 — OpenDE's UnrealScript compiler (two real bugs, both fixed)

| Commit | What |
|---|---|
| `26899e6` | `ucc make` loads the game's packages before compiling (cross-package types like `ConSys.Conversation` were invisible) |
| `9ddbd5d` | a class named before its own script is parsed no longer leaves a broken placeholder (it **segfaulted** the compiler) |
| `65cf256` | regression test for the above |
| `dad5232` | `ucc make` loads **every** package the ini lists, not a fixed handful (the `DeusExUI` fonts were missing) |
| `87c2493` | a file with no directory of its own could not be written (`create_directories("")` threw) |

### Phase 1b — OpenDE fixes from earlier in the session

| Commit | What |
|---|---|
| `4b8c915` | the macOS ARM64 build: `-static-libgcc` is a hard error on Apple clang; deployment target 11.0; libc++ hardening for Debug assertions |
| `f0e8bc8` | **the travel/spawn bug**: options were appended after the `#portal` fragment, so `parseUrl` swallowed them and the player spawned at the level's default PlayerStart (the subway). Now `Map?Opt1?Opt2#Portal`. |

### Phase 2 — the host ported to POSIX

`DeusEdix/mapkit/host/platform.h` (new, 337 lines) holds every OS call. The Windows side is
verbatim behind `#ifdef _WIN32`. On POSIX: `dlopen`, a `SIGSEGV`/`sigaltstack` handler that reports
`UCMapKit|av|<module>+0x<offset>|read 0x...` (same shape as Windows) and lets the session
continue, `setsid` + rlimits instead of a job object.

### Phase 3 — the OpenDE-backed bridge

`DeusEdix/mapkit/host/openbridge/UCOpenBridge.cpp` (new, ~4100 lines) implements **all 47**
`UCBridgeApi` functions against OpenDE. Verified against OpenDE's own editor on
`02_NYC_BatteryPark`:

| | OpenDE's editor | the bridge |
|---|---|---|
| actors | 1994 | `actors\|1994` |
| rebuild | 846 brushes, 12766 nodes, 5477 surfaces, 15 zones | identical |
| lighting | 376 lights, 5800 lightmaps | identical |

And a map → rebuild → dump → save → load → dump gives **byte-identical** geometry.

### Phase 4 — the Python layer (four lanes + integration)

| Lane | Files |
|---|---|
| A | `mapkit/kit.py`, `mapkit/build_cpu.py`, `mapkit/resident_build.py` |
| B | `mapkit/settings.py`, `mapkit/workspace.py`, `mapkit/mods.py`, `sheet/texcat/texgen` |
| C | `launcher/deusedix.py`, `launcher/native_menu.py`, `launcher/DeusEdix.spec`, its tests |
| D | `mapkit/host/CMakeLists.txt` (new), `mapkit/native/CMakeLists.txt` (new), `dev/*.py` |
| E | `mapkit/native/NavRoute.h` → OpenDE's `src/engine/nav_route.cc`, `nav_native.cc` |
| F (me) | the integration fixes the lanes left behind — see below |

### The integration fixes (lane F) — worth knowing, they are subtle

1. `settings.PROJECT` and `settings.build_bin()` are **layout-aware**: the install layout has the
   code in `tools/mapkit`, a checkout has `mapkit/` at the top. `kit.py` and `workspace.py` now use
   them, so they cannot disagree about where the editor's files are. (Before, `kit.PROJECT` pointed
   at the *OpenDE repo* — the wrong repo entirely.)
2. The host reads `--bridge` **before** its command. `workspace.build()` had it the other way
   round, so the host never found its bridge.
3. The original game's runtime classes **overlay** the editor's own (which are Revision's) in the
   workspace — the same thing a project does (`projects.py`).
4. `dev/stage.py --clean` refuses to clean a folder with user data and **exits**, so staging
   silently did nothing. Use `python3 dev/stage.py --game <folder>` (no `--clean`).

### Phase 5 — the navmesh routing moved into OpenDE

`DeusEdixNav.dll` was a 32-bit PE DLL the *game* loads, linked against the game's `Core.dll`.
The algorithm (`NavRoute.h`) was plain portable C++, so it is now OpenDE's
`src/engine/nav_route.cc` with a native bound by name (`nav_native.cc`), compiled with
`-ffp-contract=off` so the float arithmetic stays bit-identical to UnrealScript's.

Verified: OpenDE's corners match the original `NavRoute.h` and the UnrealScript `ScriptRouter`
on **2400 routes / 14152 corners**.

### The last two fixes (committed)

`b81155d` — the editor's command lines are written in the engine's Windows form:
`FILE="..\Input\map.dx"`. On POSIX the backslash is a file-name character, so the bridge never
found the file — **MAP LOAD and MAP EXPORT failed with the map the editor showed**.
`resolveFile()` in the bridge now treats `\` as a separator and resolves relative paths from the
host's working directory (the run's `System` folder), exactly as on Windows.

`f54f68f` — the workspace keeps the original game's classes. The editor's own sources are
Revision's, and two of their classes use what only Revision has; the versions for the original
game are laid over them. It is done **before** the workspace is made and when it is refreshed —
a workspace made while the editor's sources were Revision's kept them, and the editor's own
package did not compile.

---

## 5. Verified working (with the commands)

```
# OpenDE builds and its tests pass
cmake --build /Volumes/OWC/Games/OpenDE/build --parallel 10     # exit 0, no warnings
/Volumes/OWC/Games/OpenDE/build/bin/unit-tests "~[data]"        # 62 cases, 60 passed, 2 failed
                                                             # (the 2 are pre-existing audio rounding)
```

```
# the host and the bridge build on macOS ARM64
cd /Volumes/OWC/Games/OpenDE/DeusEdix
cmake -S mapkit/host -B build/host -DCMAKE_BUILD_TYPE=Release \
      -DOPENDE_ROOT=/Volumes/OWC/Games/OpenDE
cmake --build build/host --parallel 10
# -> build/host/bin/UCMapKitHost, build/host/bin/UCOpenBridge.dylib (arm64)

# stage the program folder
python3 dev/stage.py --game /Volumes/OWC/Games/OpenDE/game-data
# -> build/app (the editor's code, the host, the bridge, the page)

# the editor targets the ORIGINAL game (the user has no Revision)
python3 - <<'EOF'
import json; p='build/app/deusedix.json'
d=json.load(open(p)); d['target']='vanilla'; json.dump(d, open(p,'w'), indent=1)
EOF

# the editor builds its own UnrealScript package with the native host
cd build/app && python3 -c "
import sys; sys.path.insert(0,'tools/mapkit'); import workspace
ok, lines = workspace.build(log=print); print('build ->', ok)"
# -> 'Success - 0 error(s), 0 warnings', OK: dist/DeusEdix.u

# the editor's page and API answer
python3 ../../launcher/deusedix.py --browser      # then http://127.0.0.1:8790/
# -> page 200 (81 KB), /api/project/status 200, /api/map_browser 200, /api/projects 200

# a map's geometry is prepared for the 3D view (the whole data pipeline)
cd build/app && python3 -c "
import sys; sys.path.insert(0,'tools/mapkit'); sys.path.insert(0,'tools/mapkit/editor')
import dxmaps, data as datamod
datamod.prepare('02_NYC_Bar', src=dxmaps.shown('02_NYC_Bar', False), log=lambda *a: None)
print('prepare -> ok')"

# a real map loads through the native host
cd build/app && python3 -c "
import sys; sys.path.insert(0,'tools/mapkit'); import kit
run = kit.new_run(label='e2e')
kit.execute(run, ['MAP LOAD FILE=\"/Volumes/OWC/Games/OpenDE/game-data/Maps/02_NYC_Bar.dx\"'])"
# -> UCMapKit|actors|492, complete 27 ms
```

**The editor's own Python suite:** 38 problems against a baseline of 51 (**13 fewer**). The six that
differ from the baseline are **Revision-only test assertions** (the editor targets the original
game here) plus one fixture that needs the built package in `dist/`. No real regression.

> Re-measure the baseline with: `git checkout f83ff07 -- . && cd mapkit && python3 -m unittest
> discover -s tests -p "test_*.py"` (then `git checkout HEAD -- .`).

---

## 6. What is NOT done — the MVP gaps, ranked

1. **The 3D view has never been opened in a browser against the native bridge.** The data
   pipeline (`/api/scene` → the bridge → `scene.t3d` + the dumps) is verified up to the file
   being written. The page itself is three.js/WebGL and should work in any modern browser, but
   *nobody has looked at it*. **This is the first thing to do.**
   * Try: `python3 launcher/deusedix.py --browser`, open `http://127.0.0.1:8790/`, open a map, and
     look at the 3D view. Expect the geometry to be there and some panels to be empty.
2. **"Play" (the editor launching the game) is Windows only.** `launcher/native_play.py` drives the
   game's window with Win32 (`ctypes.wintypes`). The game itself runs natively (OpenDE's
   `deusex`); the editor cannot yet drive its window. `mapkit/game_process.py` (412 lines) is also
   Win32-only and was **not** ported (no lane owned it).
   * For an MVP, "Play" could launch `build/bin/deusex <map>` without any window control.
3. **The bridge's `meshImport` ignores the old vertex format argument**, and OpenDE's picture
   importer does not turn a picture the right way up. Both are in **OpenDE**
   (`src/compiler/asset_texture.cc`), not the bridge.
4. **The bridge's `texFlags` derives the flags** from a texture's booleans, as OpenDE's renderer
   does — Deus Ex textures have no flags of their own.
5. **The editor's Python suite still has Windows-only fixtures** (`ctypes.WinDLL`, `FakeHost.bat`).
   Not failures of the port, but they make the suite noisy on macOS.
6. **`DeusEdixNav.dll` is still built for Windows** (the *game* loads it). Elsewhere OpenDE
   answers the route. Both paths exist; that is deliberate.
7. **The nav package's licence** (GPLv3 code in the MIT tree) needs the owner's explicit
   dual-licence statement. See §2.
8. **The editor's page has no native window on macOS** unless pywebview's Cocoa backend is
   installed (needs `pyobjc`); the browser is the fallback and works.
9. **Two Revision-only base classes** (`RevRootWindow`, `RevMenuScreenNewGame`) are in DeusEdix's
   Revision runtime. `runtime/vanilla/Classes` already has the original-game versions and they
   are used — no action needed unless Revision support is wanted.

---

## 7. Things that will bite you

* **The staged app is `build/app`, the repo is `DeusEdix/`.** Two layouts:
  * install: `<app>/tools/mapkit/…`, `settings.PROJECT = <app>`
  * checkout: `<repo>/mapkit/…`, `settings.PROJECT = <repo>`
  `settings.build_bin()` tells them apart **by where the editor's code is** — do not test for a
  folder's existence (`tools/mapkit/` can exist with only `runs/` in it).
* **`dev/stage.py --clean` refuses and exits** when the folder has user data. Use no `--clean`.
* **`deusedix.json` needs `"target": "vanilla"`** — the default is Revision, and only the
  original game is installed. `stage.py` rewrites this file, so set it **after** staging.
* **The host's `--bridge` must come first** on the command line.
* **The user's `~/.local/share/deusex/DeusEx.ini` takes precedence** over the game's ini
  (`config::initialize` → `loadPreferred`). `EditPackages` is one package **per line** (not
  comma-separated) — `Ini::getAll` returns one value per line.
* **The original game's runtime classes** are `runtime/vanilla/Classes` (made by
  `dev/vanilla_runtime.py` from the Revision ones). They reproduce byte-identically.
* `uv` manages the Python environment (the user's choice). `requirements.txt` is unchanged:
  pillow, pywebview, pyinstaller.
* The Godot-style `DeusEdix/mapkit/editor/static/` is **not** built — it is served as-is.

---

## 8. Useful commands

```sh
# OpenDE
cmake --build /Volumes/OWC/Games/OpenDE/build --parallel 10
/Volumes/OWC/Games/OpenDE/build/bin/unit-tests "~[data]"
/Volumes/OWC/Games/OpenDE/build/bin/deusex <map> --at "120:exec actorlist Engine.Actor" --frames 125
/Volumes/OWC/Games/OpenDE/build/bin/deused <map>      # OpenDE's own (primitive) editor

# DeusEdix
cd /Volumes/OWC/Games/OpenDE/DeusEdix
cmake -S mapkit/host -B build/host -DCMAKE_BUILD_TYPE=Release -DOPENDE_ROOT=/Volumes/OWC/Games/OpenDE
cmake --build build/host --parallel 10
python3 dev/stage.py --game /Volumes/OWC/Games/OpenDE/game-data
cd mapkit && python3 -m unittest discover -s tests -p "test_*.py"
cd /Volumes/OWC/Games/OpenDE/DeusEdix && python3 dev/vanilla_runtime.py   # regenerates runtime/vanilla/Classes

# the host by hand
build/host/bin/UCMapKitHost --bridge build/host/bin/UCOpenBridge.dylib -
```

---

## 9. Reference files

| File | What it is |
|---|---|
| `DeusEdix/docs/NATIVE_PORT.md` | the design: the capability matrix, the licence boundary, what is portable and what is not |
| `DeusEdix/mapkit/host/bridge.h` | the plain C ABI (unchanged) |
| `DeusEdix/mapkit/host/PROTOCOL.md` | the command protocol and every file format (unchanged) |
| `DeusEdix/mapkit/host/README.md` | what the host and the bridge do |
| `DeusEdix/mapkit/host/platform.h` | the ported host's system calls |
| `DeusEdix/mapkit/host/openbridge/` | the OpenDE-backed bridge |
| `DeusEdix/.slim/deepwork/native-arm64-port/progress.md` | the pass plan and the known-issues list |
| `OpenDE/src/engine/nav_route.cc` | the navmesh router (from DeusEdix's `NavRoute.h`) |
| `OpenDE/src/core/url.cc` | `travelUrl()` — the travel/spawn fix |
| `OpenDE/src/ucc/main.cc` | the UnrealScript compiler's command line |
| `OpenDE/src/compiler/` | the UnrealScript compiler |

**Game data:** `/Volumes/OWC/Games/OpenDE/game-data` is a full Deus Ex GOTY 1112fm install
(System with all the `.u` packages, Maps with 88 `.dx` files, Textures, Sounds, Music).
`/Volumes/OWC/Games/OpenDE/deusexsdk1112f_8877.exe` is the Deus Ex SDK 1112f (a WinZip SFX;
`ReleaseSDK1112f/Headers/DxHeaders.zip` is inside). The SDK is **only** needed for the Windows
engine bridge — the native port needs neither its headers nor its import libraries.

---

## 10. If you pick this up cold, do this first

1. `cd /Volumes/OWC/Games/OpenDE/DeusEdix && git log --oneline -8` and
   `git show --stat` the commits above — each is small and explains itself in its message.
2. Build and stage (commands in §8), set `target: vanilla`, and confirm
   `workspace.build()` says "Success".
3. **Open the page in a browser and look at it** (gap #1). That is the one thing that has never
   been done and the one thing that decides whether this port is done.
4. Then decide about "Play" (gap #2) and the nav package's licence (gap #7).
