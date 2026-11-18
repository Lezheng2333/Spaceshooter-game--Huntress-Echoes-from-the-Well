#!/usr/bin/env bash
# make_icns.sh — 把一张 1024×1024 方形 PNG 打包成 macOS .icns
#
#   用法: ./make_icns.sh <1024.png> <out.icns>
#
# 流程：临时 .iconset（16/32/128/256/512 的 1x 与 2x 共 10 个 PNG）
#        → iconutil -c icns → 校验 magic / 回读 iconset。
# 只依赖 macOS 自带 sips + iconutil，不联网。
#
# 说明：源图建议先用 mac_preview.py 生成（透明背景 + 824 内容区 + 185.4 圆角），
#       直接把 1024 概念图（不透明底）打成 icns 会显示成白底方块（脚本会警告）。
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
用法: ./make_icns.sh <1024.png> <out.icns>

  1024.png   1024×1024 方形 PNG（更大也会先缩到 1024；更小会报错）
  out.icns   输出 .icns（父目录不存在会自动创建）

示例:
  ./make_icns.sh _selftest/out/preview_A_shadow_inner.png icon_A.icns
EOF
  exit "${1:-0}"
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 2
}

[[ "${1:-}" == "-h" || "${1:-}" == "--help" ]] && usage 0
[[ $# -eq 2 ]] || usage 2

IN="$1"
OUT="$2"

[[ -f "$IN" ]] || die "输入文件不存在: $IN"
[[ -x /usr/bin/iconutil ]] || command -v iconutil >/dev/null 2>&1 \
  || die "找不到 iconutil（macOS 自带，应在 /usr/bin/iconutil）"
command -v sips >/dev/null 2>&1 || die "找不到 sips（macOS 自带）"

EXT=$(printf '%s' "${IN##*.}" | tr '[:upper:]' '[:lower:]')
[[ "$EXT" == "png" ]] || die "输入必须是 .png（当前 .$EXT）"

abspath() {
  local p="$1" dir
  if [[ "$p" == /* ]]; then
    printf '%s' "$p"
  else
    dir=$(cd "$(dirname "$p")" && pwd) || return 1
    printf '%s/%s' "$dir" "$(basename "$p")"
  fi
}
IN_ABS=$(abspath "$IN") || die "无法解析输入绝对路径: $IN"
mkdir -p "$(dirname "$OUT")" || die "无法创建输出目录: $(dirname "$OUT")"
OUT_ABS=$(abspath "$OUT") || die "无法解析输出绝对路径: $OUT"
[[ "$IN_ABS" == "$OUT_ABS" ]] && die "输出路径与输入相同，拒绝覆盖源文件"
case "$(basename "$OUT_ABS")" in
  *.icns) ;;
  *) die "输出文件名应以 .icns 结尾（收到: $(basename "$OUT_ABS")）" ;;
esac

# ---------- 读源图尺寸 ----------
dim() {
  sips -g "$2" "$1" 2>/dev/null | awk -v k="$2" '$1 == k ":" {print $2}'
}
W=$(dim "$IN_ABS" pixelWidth)
H=$(dim "$IN_ABS" pixelHeight)
[[ "$W" =~ ^[0-9]+$ && "$H" =~ ^[0-9]+$ ]] || die "无法读取 PNG 尺寸（是否真是 PNG？）: $IN_ABS"
[[ "$W" == "$H" ]] || die "图标源必须是正方形（当前 ${W}×${H}）"
(( W >= 1024 )) || die "需要至少 1024×1024 源图（当前 ${W}×${H}）：
       512@2x 这一档必须有 1024 像素；请先用 mac_preview.py 生成 1024 预览。"
if (( W > 1024 )); then
  printf 'warning: 源图 %s×%s 大于 1024，将先缩到 1024×1024\n' "$W" "$H" >&2
fi
HAS_ALPHA=$(sips -g hasAlpha "$IN_ABS" 2>/dev/null | awk '$1 == "hasAlpha:" {print $2}')
if [[ "$HAS_ALPHA" == "no" ]]; then
  printf 'warning: 输入没有 alpha 通道，Dock/Finder 里可能显示成白底方块；\n' >&2
  printf '         建议用 mac_preview.py 生成透明底 squircle 预览后再打包。\n' >&2
fi

# ---------- 临时 iconset ----------
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/dsh-icns.XXXXXX") || die "mktemp 失败"
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT INT TERM

ICONSET="$TMP_DIR/AppIcon.iconset"
mkdir -p "$ICONSET"
SRC1024="$TMP_DIR/source_1024.png"
if (( W == 1024 )); then
  cp "$IN_ABS" "$SRC1024" || die "复制源图失败"
else
  sips -z 1024 1024 "$IN_ABS" --out "$SRC1024" >/dev/null 2>&1 \
    || die "sips 把源图缩到 1024 失败"
fi

SPECS=(
  "icon_16x16.png:16"
  "icon_16x16@2x.png:32"
  "icon_32x32.png:32"
  "icon_32x32@2x.png:64"
  "icon_128x128.png:128"
  "icon_128x128@2x.png:256"
  "icon_256x256.png:256"
  "icon_256x256@2x.png:512"
  "icon_512x512.png:512"
  "icon_512x512@2x.png:1024"
)

printf '▸ 生成 iconset: %d 个尺寸档\n' "${#SPECS[@]}"
for spec in "${SPECS[@]}"; do
  NAME="${spec%%:*}"
  PX="${spec##*:}"
  DEST="$ICONSET/$NAME"
  if (( PX == 1024 )); then
    cp "$SRC1024" "$DEST" || die "写入 $NAME 失败"
  else
    sips -z "$PX" "$PX" "$SRC1024" --out "$DEST" >/dev/null 2>&1 \
      || die "sips 缩放 $NAME（${PX}px）失败"
  fi
  [[ -s "$DEST" ]] || die "生成 $NAME 后文件为空"
  GOT=$(dim "$DEST" pixelWidth)
  [[ "$GOT" == "$PX" ]] || die "尺寸校验失败：$NAME 期望 ${PX}px，实际 ${GOT:-读取失败}px"
done

# ---------- 打包 ----------
printf '▸ iconutil 打包 → %s\n' "$OUT_ABS"
ICNS_LOG="$TMP_DIR/iconutil.log"
if ! iconutil -c icns -o "$OUT_ABS" "$ICONSET" 2>"$ICNS_LOG"; then
  printf 'error: iconutil 转换失败\n' >&2
  sed 's/^/       /' "$ICNS_LOG" >&2 || true
  exit 4
fi
[[ -s "$OUT_ABS" ]] || die "iconutil 返回成功但没有产出 icns 文件"

MAGIC=$(head -c 4 "$OUT_ABS" | od -An -c | tr -d ' \n')
[[ "$MAGIC" == "icns" ]] || die "输出不是合法 icns（magic=${MAGIC:-空}）"

# ---------- 回读验证 ----------
VERIFY="$TMP_DIR/verify.iconset"
if iconutil -c iconset -o "$VERIFY" "$OUT_ABS" >/dev/null 2>&1; then
  REPS=$(find "$VERIFY" -name '*.png' | wc -l | tr -d ' ')
else
  REPS="?"
  printf 'warning: 回读 icns 失败（文件仍已写出，请用 Finder 预览确认）\n' >&2
fi
SIZE_KB=$(( $(wc -c < "$OUT_ABS") / 1024 ))

printf '✅ make_icns: %s — %s KB · %s 个尺寸 · 回读 %s 个 rep\n' \
  "$OUT_ABS" "$SIZE_KB" "${#SPECS[@]}" "$REPS"
printf '   16/32/64/128/256/512/1024 px（512@2x 用 1024 源图）\n'
