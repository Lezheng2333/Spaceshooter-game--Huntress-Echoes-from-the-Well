#!/bin/bash
# === STAR FOX SPACE SHOOTER — Release Script ===
# Usage: .claude/skills/release.sh <version> <title>
# Example: .claude/skills/release.sh 1.2.9 "Ver 1.2.9: description"
#
# Prerequisites (already done by Claude before running this):
#   1. DEVELOPMENT_LOG.md updated with new version entry
#   2. README.md updated (download links, What's New, project structure)
#   3. Source code version string updated
#   4. Code compiles cleanly
# This script handles: compile → package .app → sign → zip → git commit/push → gh release

set -e

VERSION="$1"
TITLE="$2"
NOTES="$3"

if [ -z "$VERSION" ] || [ -z "$TITLE" ]; then
    echo "Usage: release.sh <version> <title> [notes]"
    echo "Example: release.sh 1.2.9 'Ver 1.2.9: new features'"
    exit 1
fi

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC_DIR="$PROJECT_ROOT/v2.0.0"
RELEASE_DIR="$SRC_DIR/Release Version"
APP_NAME="Shooter ver${VERSION}.app"
ZIP_NAME="Shooter ver${VERSION}.zip"
SRC_FILE="main.cpp"
SRC_SUBDIR="multiple code files"

# 从已跟踪的稳定归档提取真正的 SDL2，避免 Homebrew sdl2-compat 引入 SDL3/新系统依赖。
SDL_TEMP=$(mktemp -d)
trap 'rm -rf "$SDL_TEMP"' EXIT
SDL_LIB="$SDL_TEMP/libSDL2-2.0.0.dylib"
unzip -p "$PROJECT_ROOT/v1.0.0/Shooter ver1.0.0.zip" \
    'Shooter ver1.0.0.app/Contents/Frameworks/libSDL2-2.0.0.dylib' > "$SDL_LIB"
echo "=== Step 1: Compile ==="
cd "$SRC_DIR/$SRC_SUBDIR"
clang++ -std=c++11 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -D_THREAD_SAFE -mmacosx-version-min=11.0 "$SRC_FILE" -o shooter \
    "$SDL_LIB"
echo "Compile OK"

echo "=== Step 2: Package .app ==="
APP_DIR="$RELEASE_DIR/$APP_NAME"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Frameworks"
mkdir -p "$APP_DIR/Contents/Resources"

cp shooter "$APP_DIR/Contents/MacOS/shooter"
# Finder 启动时 cwd 不固定：启动器进入 Resources，语音相对路径保持有效；存档放用户目录。
cat > "$APP_DIR/Contents/MacOS/launch" <<'LAUNCH'
#!/bin/bash
set -e
if [ -z "${SFSS_SAVE_DIR:-}" ]; then
    export SFSS_SAVE_DIR="$HOME/Library/Application Support/StarFoxSpaceShooter/saves"
fi
mkdir -p "$SFSS_SAVE_DIR"
cd "$(dirname "$0")/../Resources"
exec ../MacOS/shooter "$@"
LAUNCH
chmod +x "$APP_DIR/Contents/MacOS/launch"
if [ -d voice ]; then
    cp -R voice "$APP_DIR/Contents/Resources/voice"
fi
cp "$SDL_LIB" "$APP_DIR/Contents/Frameworks/"
install_name_tool -id @executable_path/../Frameworks/libSDL2-2.0.0.dylib \
    "$APP_DIR/Contents/Frameworks/libSDL2-2.0.0.dylib"

cat > "$APP_DIR/Contents/Info.plist" << PLEOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>launch</string>
    <key>CFBundleIdentifier</key>
    <string>com.starfox.spaceshooter</string>
    <key>CFBundleName</key>
    <string>Shooter</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
PLEOF

echo "=== Step 3: Code Sign ==="
codesign --force --deep --sign - "$APP_DIR"
echo "Sign OK"

echo "=== Step 4: Create .zip ==="
cd "$RELEASE_DIR"
ditto -c -k --keepParent "$APP_NAME" "$ZIP_NAME"
ls -la "$ZIP_NAME"
echo "Zip OK"

# 允许先打包并验证，再执行提交/推送/发布。
if [ "${4:-}" = "--package-only" ]; then exit 0; fi

echo "=== Step 5: Git commit & push ==="
cd "$PROJECT_ROOT"
git add .gitignore DEVELOPMENT_LOG.md README.md CLAUDE.md tools/ \
    "$SRC_DIR/$SRC_SUBDIR/" "$RELEASE_DIR/$ZIP_NAME" \
    "$SRC_DIR/space_shooting ver1.2.20 single-file archive.cpp"
# .claude/ 在 .gitignore 里（IDE 目录），但本脚本本身是已跟踪文件，需 -f 强制暂存
git add -f .claude/skills/release.sh
git commit -m "$TITLE"
git push origin main
echo "Push OK"

echo "=== Step 6: GitHub Release ==="
if [ -n "$NOTES" ]; then
    if [ -f "$NOTES" ]; then
        gh release create "v${VERSION}" --title "$TITLE" --notes-file "$NOTES" "$RELEASE_DIR/$ZIP_NAME" --target "$(git rev-parse HEAD)"
    else
        gh release create "v${VERSION}" --title "$TITLE" --notes "$NOTES" "$RELEASE_DIR/$ZIP_NAME" --target "$(git rev-parse HEAD)"
    fi
else
    gh release create "v${VERSION}" --title "$TITLE" "$RELEASE_DIR/$ZIP_NAME"
fi
echo "=== Release v${VERSION} complete! ==="
echo "URL: https://github.com/Lezheng2333/starfox_spaceshooter/releases/tag/v${VERSION}"
