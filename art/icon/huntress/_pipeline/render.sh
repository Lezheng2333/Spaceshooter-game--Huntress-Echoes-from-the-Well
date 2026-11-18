#!/usr/bin/env bash
# render.sh — 用 Microsoft Edge 无头模式把 HTML / SVG 渲染成 PNG（macOS 图标管线）
#
#   用法: ./render.sh <input.html> <output.png> [size] [scale]
#
#   size    视口边长，默认 1024
#   scale   设备像素比，默认 2：先按 size×scale 截图，再用 Pillow LANCZOS 缩回 size×size；
#           Edge 不支持 --force-device-scale-factor 时会明确警告并退回 1x。
#
# 特点：自动转 file:// 绝对路径、独立临时 profile、退出自动清理；
#       无网 / 无 GUI 可跑，不依赖 npm，只用 Edge + DSH 自带 Python(+Pillow)。
#
# 环境变量：
#   EDGE_BIN          覆盖 Edge 可执行文件路径
#   DSH_PY            覆盖用于缩放的 Python（默认 DSH 自带 Python）
#   RENDER_TIMEOUT    单次渲染看门狗秒数（默认 90）
#   RENDER_TRANSPARENT=1   透明背景截图（--default-background-color=00000000）
#   RENDER_KEEP_TMP=1      保留临时目录便于排查（会打印路径）
#
# 示例：
#   ./render.sh ../../my_draft.html out/icon_A.png
#   ./render.sh draft.svg out/icon.svg.png 1024 1
set -euo pipefail

DEFAULT_EDGE="/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
DEFAULT_PY="/Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3"
EDGE="${EDGE_BIN:-$DEFAULT_EDGE}"
TIMEOUT="${RENDER_TIMEOUT:-90}"

usage() {
  cat >&2 <<'EOF'
用法: ./render.sh <input.html> <output.png> [size] [scale]

  input.html   HTML 或 SVG 文件（自动转成 file:// 绝对路径）
  output.png   输出 PNG（父目录不存在会自动创建）
  size         视口边长，默认 1024
  scale        设备像素比，默认 2：按 size×scale 截图，再用 Pillow LANCZOS 缩回
               size×size；Edge 不支持 --force-device-scale-factor 时警告并退回 1x

环境变量:
  EDGE_BIN=path            覆盖 Edge 可执行文件路径
  DSH_PY=path              覆盖用于缩放的 Python（默认 DSH 自带 Python）
  RENDER_TIMEOUT=90        单次渲染看门狗秒数
  RENDER_TRANSPARENT=1     透明背景截图（--default-background-color=00000000）
  RENDER_KEEP_TMP=1        保留临时 profile 并打印路径（排查用）

示例:
  ./render.sh draft_A.html out/icon_A.png
  ./render.sh draft.svg out/icon.svg.png 1024 1
EOF
  exit "${1:-0}"
}

die() {
  printf 'error: %s\n' "$*" >&2
  exit 2
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then usage 0; fi
[[ $# -ge 2 && $# -le 4 ]] || usage 2

INPUT="$1"
OUTPUT="$2"
SIZE="${3:-1024}"
SCALE="${4:-2}"

# ---------- 参数校验 ----------
[[ -f "$INPUT" ]] || die "输入文件不存在: $INPUT"
EXT=$(printf '%s' "${INPUT##*.}" | tr '[:upper:]' '[:lower:]')
case "$EXT" in
  html|htm|svg) ;;
  *) die "输入应为 .html / .htm / .svg（Edge 无头导航用），收到: $INPUT" ;;
esac
[[ "$SIZE" =~ ^[0-9]+$ ]] || die "size 必须是正整数，收到: $SIZE"
(( SIZE >= 16 && SIZE <= 8192 )) || die "size 需在 16..8192 之间，收到: $SIZE"
[[ "$SCALE" =~ ^[0-9]+$ ]] || die "scale 必须是正整数，收到: $SCALE"
(( SCALE >= 1 && SCALE <= 4 )) || die "scale 需在 1..4 之间，收到: $SCALE"
[[ -x "$EDGE" ]] || die "找不到可执行的 Edge: $EDGE
       可用 EDGE_BIN=/path/to/Microsoft\\ Edge 覆盖路径。"

# ---------- 绝对路径 / 目录 ----------
abspath() {
  local p="$1" dir
  if [[ "$p" == /* ]]; then
    printf '%s' "$p"
  else
    dir=$(cd "$(dirname "$p")" && pwd) || return 1
    printf '%s/%s' "$dir" "$(basename "$p")"
  fi
}
INPUT_ABS=$(abspath "$INPUT") || die "无法解析输入绝对路径: $INPUT"
mkdir -p "$(dirname "$OUTPUT")" || die "无法创建输出目录: $(dirname "$OUTPUT")"
OUTPUT_ABS=$(abspath "$OUTPUT") || die "无法解析输出绝对路径: $OUTPUT"
[[ "$INPUT_ABS" == "$OUTPUT_ABS" ]] && die "输出路径与输入相同，拒绝覆盖源文件"

# ---------- 选一个带 Pillow 的 Python（缩放用；没有则退回 sips） ----------
pick_python() {
  local cand
  for cand in "${DSH_PY:-}" "$DEFAULT_PY" "$(command -v python3 2>/dev/null || true)"; do
    [[ -n "$cand" && -x "$cand" ]] || continue
    if "$cand" -c 'import PIL' >/dev/null 2>&1; then
      printf '%s' "$cand"
      return 0
    fi
  done
  return 1
}
PY=$(pick_python || true)
URL_PY="${PY:-$(command -v python3 2>/dev/null || true)}"

# ---------- file:// URL（正确百分号编码空格/中文） ----------
if [[ -n "$URL_PY" ]]; then
  FILE_URL=$("$URL_PY" - "$INPUT_ABS" <<'PYEOF'
import pathlib
import sys
print(pathlib.Path(sys.argv[1]).as_uri())
PYEOF
) || die "无法生成 file:// URL: $INPUT_ABS"
else
  FILE_URL="file://$INPUT_ABS"
fi

# ---------- 临时目录 ----------
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/dsh-icon-render.XXXXXX") || die "mktemp 失败"
cleanup() {
  if [[ "${RENDER_KEEP_TMP:-0}" == "1" ]]; then
    printf 'ℹ 保留临时目录: %s\n' "$TMP_DIR" >&2
  else
    rm -rf "$TMP_DIR"
  fi
}
trap cleanup EXIT INT TERM
PROFILE="$TMP_DIR/profile"
LOG="$TMP_DIR/edge.log"

# ---------- 渲染 ----------
RC=0
render_once() {
  local scale="$1" shot="$2"
  local args=(
    --headless=new
    --no-sandbox
    --disable-gpu
    --disable-software-rasterizer
    --disable-dev-shm-usage
    --no-first-run
    --no-default-browser-check
    --user-data-dir="$PROFILE"
  )
  if (( scale > 1 )); then
    args+=(--force-device-scale-factor="$scale")
  fi
  if [[ "${RENDER_TRANSPARENT:-0}" == "1" ]]; then
    args+=(--default-background-color=00000000)
  fi
  args+=(--window-size="$SIZE,$SIZE" --screenshot="$shot" "$FILE_URL")

  : > "$LOG"
  "$EDGE" "${args[@]}" >>"$LOG" 2>&1 &
  local pid=$!
  local rc=0 elapsed=0
  # 前台轮询看门狗（不用后台 sleep 子进程，避免残留进程占住调用方的 stdout 管道）
  while kill -0 "$pid" 2>/dev/null; do
    sleep 1
    elapsed=$((elapsed + 1))
    if (( elapsed >= TIMEOUT )); then
      printf 'warning: Edge 渲染超时（>%ss），已终止
' "$TIMEOUT" >&2
      kill -TERM "$pid" 2>/dev/null || true
      sleep 2
      kill -KILL "$pid" 2>/dev/null || true
      break
    fi
  done
  wait "$pid" || rc=$?
  if (( rc != 0 )); then
    printf 'error: Edge 渲染失败（exit %s）\n' "$rc" >&2
    tail -n 25 "$LOG" >&2 || true
    printf '提示: 检查 %s 是否可执行、页面是否引用网络资源；可用 RENDER_KEEP_TMP=1 保留现场。\n' "$EDGE" >&2
    exit 3
  fi
  [[ -s "$shot" ]] || {
    printf 'error: Edge 没有产出截图（%s）\n' "$shot" >&2
    tail -n 25 "$LOG" >&2 || true
    exit 3
  }
}

# 输出尺寸（PNG 头解析，不依赖 Pillow；失败返回空）
png_size() {
  local f="$1"
  local w h
  w=$(od -An -tu1 -j16 -N4 "$f" 2>/dev/null | awk 'NF>=4 {print $1*16777216+$2*65536+$3*256+$4; exit}')
  h=$(od -An -tu1 -j20 -N4 "$f" 2>/dev/null | awk 'NF>=4 {print $1*16777216+$2*65536+$3*256+$4; exit}')
  [[ -n "$w" && -n "$h" ]] && printf '%s %s' "$w" "$h"
}

WANT_HI=$(( SIZE * SCALE ))
SHOT_HI="$TMP_DIR/shot_${SIZE}x${WANT_HI}.png"
render_once "$SCALE" "$SHOT_HI"
read -r SHOT_W SHOT_H <<<"$(png_size "$SHOT_HI")" || true
[[ -n "${SHOT_W:-}" && -n "${SHOT_H:-}" ]] || die "无法读取截图尺寸: $SHOT_HI"

USED_SCALE=$SCALE
DOWNSCALE=0
NOTE=""
if (( SCALE > 1 )); then
  if [[ "$SHOT_W" == "$WANT_HI" && "$SHOT_H" == "$WANT_HI" ]]; then
    DOWNSCALE=1
  elif [[ "$SHOT_W" == "$SIZE" && "$SHOT_H" == "$SIZE" ]]; then
    USED_SCALE=1
    NOTE="⚠️  Edge 未响应 --force-device-scale-factor=$SCALE（截图 ${SHOT_W}×${SHOT_H}），已按 1x 处理"
  else
    printf 'warning: --force-device-scale-factor=%s 输出尺寸异常（%s×%s），退回 1x 重试\n' \
      "$SCALE" "$SHOT_W" "$SHOT_H" >&2
    SHOT1="$TMP_DIR/shot_1x.png"
    render_once 1 "$SHOT1"
    read -r SHOT_W SHOT_H <<<"$(png_size "$SHOT1")" || true
    [[ "$SHOT_W" == "$SIZE" && "$SHOT_H" == "$SIZE" ]] || die \
      "Edge 1x 截图尺寸仍不符（期望 ${SIZE}×${SIZE}，实际 ${SHOT_W}×${SHOT_H}）"
    SHOT_HI="$SHOT1"
    USED_SCALE=1
    DOWNSCALE=0
    NOTE="⚠️  Edge 不支持设备像素比，已退回 1x"
  fi
else
  [[ "$SHOT_W" == "$SIZE" && "$SHOT_H" == "$SIZE" ]] || die \
    "Edge 1x 截图尺寸不符（期望 ${SIZE}×${SIZE}，实际 ${SHOT_W}×${SHOT_H}）"
fi

# ---------- 高质量缩放 / 落盘 ----------
if (( DOWNSCALE )); then
  if [[ -z "$PY" ]]; then
    if command -v sips >/dev/null 2>&1; then
      NOTE="${NOTE:+$NOTE；}⚠️  未找到带 Pillow 的 Python，改用 sips 缩放"
      sips -z "$SIZE" "$SIZE" "$SHOT_HI" --out "$OUTPUT_ABS" >/dev/null 2>&1 \
        || die "sips 缩放失败"
    else
      die "需要 Pillow 或 sips 才能把 ${SHOT_W}×${SHOT_H} 缩到 ${SIZE}×${SIZE}"
    fi
  else
    "$PY" - "$SHOT_HI" "$OUTPUT_ABS" "$SIZE" <<'PYEOF'
import sys
from PIL import Image

src, dst, size = sys.argv[1], sys.argv[2], int(sys.argv[3])
with Image.open(src) as im:
    im.load()
    if im.size != (size, size):
        im = im.resize((size, size), Image.Resampling.LANCZOS)
    im.save(dst, format="PNG")
PYEOF
  fi
else
  cp "$SHOT_HI" "$OUTPUT_ABS" || die "无法写出 $OUTPUT_ABS"
fi

# ---------- 结果 ----------
FINAL_WH=$(png_size "$OUTPUT_ABS")
if [[ "$FINAL_WH" != "$SIZE $SIZE" ]]; then
  die "输出尺寸异常（期望 ${SIZE}×${SIZE}，实际 ${FINAL_WH}）"
fi
printf '✅ render: %s — %s×%s\n' "$OUTPUT_ABS" "$SIZE" "$SIZE"
if (( USED_SCALE > 1 && DOWNSCALE )); then
  printf '   Edge %sx 截图 %s×%s → Pillow LANCZOS 缩到 %s×%s\n' \
    "$USED_SCALE" "$((SIZE * USED_SCALE))" "$((SIZE * USED_SCALE))" "$SIZE" "$SIZE"
else
  printf '   Edge 1x 截图（scale=%s）\n' "$USED_SCALE"
fi
if [[ -n "$NOTE" ]]; then
  printf '   %s\n' "$NOTE"
fi
printf '   source: %s\n' "$INPUT_ABS"
