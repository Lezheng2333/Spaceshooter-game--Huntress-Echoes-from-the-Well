#!/usr/bin/env bash
# run_selftest.sh — 一键跑通 _pipeline 的 2–6 号交付（render / preview / sheet / dock / icns）
#
#   用法: ./_selftest/run_selftest.sh
#
# 产物写到 _pipeline/_selftest/out/，完整日志写到 _pipeline/_selftest/last_run.log。
# 只读测试输入，最后会用哈希确认输入文件没有被修改。
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
PIPE=$(cd "$HERE/.." && pwd)
cd "$PIPE"

DEFAULT_PY="/Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3"
PY="${DSH_PY:-$DEFAULT_PY}"
if [[ ! -x "$PY" ]]; then
  PY=$(command -v python3 2>/dev/null || true)
fi
[[ -n "$PY" && -x "$PY" ]] || { echo "error: 找不到 Python（可用 DSH_PY 指定）" >&2; exit 2; }
"$PY" -c 'import PIL' >/dev/null 2>&1 || {
  echo "error: Python ($PY) 缺少 Pillow；请设置 DSH_PY 指向 DSH 自带 Python" >&2
  exit 2
}

OUT="$HERE/out"
INPUTS="$HERE/inputs"
mkdir -p "$OUT"
LOG="$HERE/last_run.log"

main() {
set -euo pipefail
echo "=============================================="
echo " macOS 图标管线自测 · $(date '+%Y-%m-%d %H:%M:%S')"
echo " pipeline: $PIPE"
echo "=============================================="

echo
echo "== [1/8] 环境 =="
echo "Python : $PY"
"$PY" -c "import sys, PIL; print('        ', sys.version.split()[0], '· Pillow', PIL.__version__)"
EDGE="${EDGE_BIN:-/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge}"
if [[ -x "$EDGE" ]]; then
  printf 'Edge   : %s\n' "$("$EDGE" --version 2>/dev/null | head -1)"
else
  echo "Edge   : 缺失（render.sh 会报错）"
fi
echo "iconutil: $(command -v iconutil || echo 缺失)"

echo
echo "== [2/8] 生成 5 张测试输入（Pillow 绘制） =="
"$PY" "$HERE/gen_test_inputs.py"

BEFORE=$(cat "$INPUTS"/*.png "$HERE/render_test.html" 2>/dev/null | shasum -a 256 | awk '{print $1}')

echo
echo "== [3/8] render.sh（Edge 2x 截图 + Pillow 降采样；另测 1x） =="
./render.sh "$HERE/render_test.html" "$OUT/render_2x.png" 1024 2
./render.sh "$HERE/render_test.html" "$OUT/render_1x.png" 1024 1

echo
echo "== [4/8] mac_preview.py（plain / shadow+inner / squircle） =="
"$PY" mac_preview.py "$INPUTS/test_A.png" "$OUT/preview_A_plain.png"
"$PY" mac_preview.py "$INPUTS/test_A.png" "$OUT/preview_A_shadow_inner.png" --shadow --inner
"$PY" mac_preview.py "$INPUTS/test_A.png" "$OUT/preview_A_squircle.png" \
  --shape squircle --inner

echo
echo "== [5/8] contact_sheet.py（5 张图三行展示板） =="
"$PY" contact_sheet.py \
  --inputs "$INPUTS/test_A.png" "$INPUTS/test_B.png" "$INPUTS/test_C.png" \
           "$INPUTS/test_D.png" "$INPUTS/test_E.png" \
  --labels "A 红楔" "B 蓝环" "C 绿棱" "D 紫涡" "E 金冠" \
  --out "$OUT/contact_sheet.png"

echo
echo "== [6/8] dock_preview.py（仿 macOS Dock，2000×520） =="
"$PY" dock_preview.py \
  "$INPUTS/test_A.png" "$INPUTS/test_B.png" "$INPUTS/test_C.png" \
  "$INPUTS/test_D.png" "$INPUTS/test_E.png" \
  --out "$OUT/dock_preview.png"

echo
echo "== [7/8] make_icns.sh（iconset + iconutil） =="
./make_icns.sh "$OUT/preview_A_shadow_inner.png" "$OUT/test_A.icns"

echo
echo "== [8/8] 错误路径 + 输入只读检查 =="
expect_fail() {
  local desc="$1"
  shift
  local log
  if log=$("$@" 2>&1); then
    printf '  ❌ %s（本应失败但退出码为 0）\n' "$desc"
    return 1
  fi
  if printf '%s' "$log" | grep -qi 'error'; then
    printf '  ✅ %s（按预期报错）\n' "$desc"
  else
    printf '  ❌ %s（失败但没有 error: 提示）\n' "$desc"
    printf '%s\n' "$log" | sed 's/^/       /'
    return 1
  fi
}
expect_fail "mac_preview 拒绝非方形输入" \
  "$PY" mac_preview.py "$INPUTS/non_square.png" "$OUT/_should_not_exist.png"
expect_fail "make_icns 拒绝 512 源图" \
  ./make_icns.sh "$INPUTS/test_A_512.png" "$OUT/_should_not_exist.icns"
expect_fail "contact_sheet 拒绝标签数量不匹配" \
  "$PY" contact_sheet.py --inputs "$INPUTS/test_A.png" "$INPUTS/test_B.png" \
  --labels "only-one" --out "$OUT/_should_not_exist.png"
expect_fail "render.sh 拒绝非 HTML 输入" \
  ./render.sh "$INPUTS/test_A.png" "$OUT/_should_not_exist.png"
rm -f "$OUT/_should_not_exist.png" "$OUT/_should_not_exist.icns"

AFTER=$(cat "$INPUTS"/*.png "$HERE/render_test.html" 2>/dev/null | shasum -a 256 | awk '{print $1}')
if [[ "$BEFORE" == "$AFTER" ]]; then
  echo "  ✅ 测试输入与 HTML 均未被修改（只读）"
else
  echo "  ❌ 测试输入/HTML 被修改了！"
  exit 1
fi

echo
echo "== 机器校验 =="
"$PY" "$HERE/verify_outputs.py" "$OUT"

echo
echo "=============================================="
echo " ✅ ALL SELFTEST PASS · 产物在 $OUT"
echo "=============================================="
}

# 输出同时到终端与日志（bash 3.2 下不用 process substitution，避免 /dev/fd 限制）
set +e
main "$@" 2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}
set -e
exit "$RC"
