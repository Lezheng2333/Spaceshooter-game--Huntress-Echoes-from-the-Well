# Building HUNTRESS on Windows

> **There is no official Windows binary.** The maintainer develops on macOS and only ships a signed macOS `.app`; Windows users must build from source themselves — or hand this document to a Windows AI agent.

This guide targets **MSYS2 / MinGW-w64 UCRT64**. The source is C++11 + SDL2 and uses POSIX file APIs (`dirent.h`, `unistd.h`, `mkdir`, `access`) in the save/file-browser layer; MinGW-w64 provides those. MSVC is possible but needs a small portability patch (see §7).

---

## 1. Install MSYS2 + SDL2

1. Download and install [MSYS2](https://www.msys2.org/).
2. Open **MSYS2 UCRT64** (not the plain MSYS shell, not PowerShell for the build).
3. Update and install:

```bash
pacman -Syu
pacman -S --needed base-devel \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-SDL2
```

Verify:

```bash
g++ --version
pkg-config --modversion sdl2
```

---

## 2. Build the game

Run these commands in the MSYS2 UCRT64 shell from the repository root:

```bash
cd "v2.0.0/multiple code files"

# windowed release build (UCRT64 paths)
g++ -std=c++11 -O2 -Wall -Wextra main.cpp -o Huntress.exe \
  -I/ucrt64/include/SDL2 -L/ucrt64/lib \
  -lmingw32 -lSDL2main -lSDL2 -mwindows

# console build (needed to see --selftest output)
g++ -std=c++11 -O2 -Wall -Wextra main.cpp -o Huntress-console.exe \
  -I/ucrt64/include/SDL2 -L/ucrt64/lib \
  -lmingw32 -lSDL2main -lSDL2
```

If Windows reports missing `libstdc++-6.dll` / `libgcc_s_seh-1.dll`, add `-static-libgcc -static-libstdc++` to the link line, or copy those DLLs from `C:\msys64\ucrt64\bin`.

If your MSYS2 environment exposes SDL2 through `pkg-config`, this also works:

```bash
g++ -std=c++11 -O2 -Wall -Wextra main.cpp -o Huntress.exe \
  $(pkg-config --cflags sdl2) -lmingw32 -lSDL2main -lSDL2 -mwindows
```

> `-mwindows` hides the console window. Keep a separate `Huntress-console.exe` for `--selftest` and troubleshooting.

---

## 3. Optional: embed the Windows icon

The repository ships `art/icon/huntress/Huntress.ico`. From `v2.0.0/multiple code files`:

```bash
cat > huntress.rc <<'RC'
IDI_ICON1 ICON "../../art/icon/huntress/Huntress.ico"
RC

windres huntress.rc -O coff -o huntress.res

g++ -std=c++11 -O2 -Wall -Wextra main.cpp huntress.res -o Huntress.exe \
  -I/ucrt64/include/SDL2 -L/ucrt64/lib \
  -lmingw32 -lSDL2main -lSDL2 -mwindows
```

---

## 4. Package a portable folder

```bash
cd "v2.0.0/multiple code files"
mkdir -p dist/Huntress

cp Huntress.exe Huntress-console.exe dist/Huntress/
cp /ucrt64/bin/SDL2.dll dist/Huntress/
cp ../../art/icon/huntress/Huntress.ico dist/Huntress/

# voice-over WAVs are not tracked in git (34 MB).
# If you have the macOS release, copy its Resources/voice here.
# If not, the game silently falls back to synthesized voices.
if [ -d voice ]; then
  cp -R voice dist/Huntress/voice
fi
```

Create `dist/Huntress/run_huntress.bat`:

```bat
@echo off
setlocal
cd /d "%~dp0"
if not defined HUNTRESS_SAVE_DIR set "HUNTRESS_SAVE_DIR=%~dp0saves"
start "" Huntress.exe
```

Zip the **contents of** `dist/Huntress` (not `dist` itself) and share it.

---

## 5. Verify before sharing

```bash
cd dist/Huntress
HUNTRESS_SAVE_DIR="$PWD/saves" ./Huntress-console.exe --selftest
```

Expected: `[selftest] RESULT: PASS` and exit code 0. Also run `Huntress.exe` from the same folder so `voice/`, saves and assets resolve.

> **Windows QA-path note:** `--selftest` writes its QA frames to hard-coded `/tmp/...` paths (inherited from the macOS development flow). On Windows `/tmp` means `\tmp` on the current drive. If it is not writable, either create `C:\tmp`, run from a drive whose root allows `\tmp`, or ask an AI agent to replace those few selftest paths with `%TEMP%` (the normal game/save path already respects `HUNTRESS_SAVE_DIR`).

---

## 6. Voice assets

- `v2.0.0/multiple code files/voice/` is excluded from git because it is ~34 MB of generated WAVs.
- Without it the game still runs; dialogue/narration fall back to synthesized queue sounds.
- To include voice-over:
  - copy the 34 MB `voice/` folder from the macOS release `.app` (`Contents/Resources/voice`), **or**
  - regenerate it with `tools/voice_pipeline.py` (requires Python + an edge-tts/piper backend).
- Keep the resulting folder named `voice/` next to the `.exe`.

---

## 7. If you use MSVC instead of MinGW

Out of the box the save/file-browser layer expects POSIX APIs. A Windows AI agent should make a **minimal portability patch**:

- `#include <dirent.h>` / `opendir` / `readdir` → use `FindFirstFileA` / `FindNextFileA`, or build with MinGW.
- `mkdir(p, 0755)` → `_mkdir(p)` with `#include <direct.h>`.
- `access(p, W_OK)` → `_access(p, 2)` with `#include <io.h>`, or use `GetFileAttributes`.
- `getenv("HOME")` fallback (`~/Library/Application Support/...`) → `%APPDATA%` / `%USERPROFILE%`, or simply always set `HUNTRESS_SAVE_DIR` from the `.bat`.
- Directory separators: forward slashes work on Windows, but normalize where needed.

Keep the patch isolated (e.g. a `platform_win.h`) so the macOS build remains unchanged.

---

## 8. AI-agent checklist

1. Use **MSYS2 UCRT64** + `mingw-w64-ucrt-x86_64-gcc` + SDL2; avoid plain MSYS shell.
2. Build both windowed and console executables.
3. Copy `SDL2.dll`, `Huntress.ico`, and (if available) `voice/` into the dist folder.
4. Put a `.bat` that sets `HUNTRESS_SAVE_DIR` to the portable folder before launch.
5. Run `Huntress-console.exe --selftest` from the dist folder; require PASS.
6. Do not publish a local-only build as an official Huntress release; this project’s official binary is macOS-only.
7. If you fix a portability bug, send it upstream as a PR so the macOS branch is not broken.

---

*The macOS maintainer cannot test Windows builds. Corrections/PRs are welcome — please include the exact compiler/SDL versions and the `--selftest` output.*
