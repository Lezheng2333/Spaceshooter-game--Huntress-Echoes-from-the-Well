/* ============================================================
   Asset Studio 素材画廊 — 0.2 交互层
   · 数据源：/api/assets（C++ 渲染器 --list）
   · 渲染真值：游戏自己的 C++ 绘制代码 / audio.h 合成函数
   ============================================================ */
"use strict";

const $ = (s, root = document) => root.querySelector(s);
const $$ = (s, root = document) => Array.from(root.querySelectorAll(s));
const LSKEY = (k) => "as.v02." + k;

const CHAPTER_META = {
  ch1: { label: "第一章", color: "#6fd3ff" },
  ch2: { label: "第二章", color: "#a78bfa" },
  ch3: { label: "第三章", color: "#ffbf69" },
  shared: { label: "共享", color: "#7ee0a0" },
  common: { label: "共享", color: "#7ee0a0" },
  draft: { label: "草稿", color: "#ff8fc7" },
};
const CHAPTER_ORDER = ["ch1", "ch2", "ch3", "shared", "common", "draft"];
const CATEGORY_META = {
  player: { label: "玩家战机", color: "#6fd3ff", icon: "✈" },
  enemy: { label: "普通敌人", color: "#ff8f8f", icon: "◈" },
  boss: { label: "Boss", color: "#ffbf69", icon: "♛" },
  bullet: { label: "子弹弹幕", color: "#ffe28a", icon: "➤" },
  fx: { label: "特效 / 技能", color: "#7ee0a0", icon: "✦" },
  hud: { label: "HUD / UI", color: "#9fb3cc", icon: "▤" },
  bg: { label: "背景 / 场景", color: "#7aa7ff", icon: "⌁" },
  audio: { label: "音频 / BGM", color: "#4fe0c8", icon: "♫" },
  misc: { label: "其他", color: "#c9a7ff", icon: "◆" },
};
const CATEGORY_ORDER = ["player", "enemy", "boss", "bullet", "fx", "hud", "bg", "audio", "misc"];

const PALETTES = [
  { value: "none", label: "原色 · 白", color: "#ffffff" },
  { value: "friendly", label: "己方 · 白", color: "#f2f6ff" },
  { value: "enemy", label: "敌方 · 红", color: "#ff5f5f" },
  { value: "skill", label: "技能 · 绿", color: "#5cff9d" },
  { value: "neutral", label: "中立 · 青", color: "#4fe8ff" },
  { value: "gold", label: "金色", color: "#ffd166" },
  { value: "violet", label: "紫色", color: "#b388ff" },
  { value: "255,60,60,tint", label: "红 · 保亮度", color: "#ff5f5f" },
];
const BGS = [
  { value: "0A0C18", label: "深蓝黑", color: "#0a0c18" },
  { value: "000000", label: "纯黑", color: "#000000" },
  { value: "141821", label: "浅灰蓝", color: "#141821" },
  { value: "FFFFFF", label: "白底", color: "#ffffff" },
];

const LS = {
  get(key, def) {
    try { const v = localStorage.getItem(LSKEY(key)); return v == null ? def : JSON.parse(v); }
    catch (e) { return def; }
  },
  set(key, val) {
    try { localStorage.setItem(LSKEY(key), JSON.stringify(val)); } catch (e) { /* 忽略 */ }
  },
};

const S = {
  gifAvailable: true,
  renderBuild: "",
  assets: [],
  drafts: [],
  visibleIds: [],
  health: null,

  filters: { chapter: null, category: null, tag: null, q: "", fav: false, anim: false, scene: false },
  sort: LS.get("sort", "default"),
  view: LS.get("view", "grid"),
  density: LS.get("density", "md"),
  favorites: new Set(LS.get("favorites", [])),
  recents: LS.get("recents", []),
  palette: LS.get("palette", "none"),
  bg: LS.get("bg", "0A0C18"),
  animThumbs: LS.get("animThumbs", true),

  detail: null,
  detailState: "",
  detailParams: {},
  detailTab: "overview",
  detailFrame: null,
  compare: null,
  zoom: 1,
  exportZoom: 1,
  wavePeaks: null,
  analysisLoaded: false,
  sourceLoaded: false,
  analysisRequest: 0,
  analysisTimer: null,

  overlays: { scene: false, grid: false, axes: false, symmetry: false, bbox: false, anim: true },

  cardAudio: null,
  cardAudioId: null,
  cardAudioBtn: null,
  cardAudioCard: null,

  waveCache: new Map(),
  cardObserver: null,

  cmdItems: [],
  cmdIndex: 0,
  searchTimer: null,
  lastFocus: null,
  compareDrag: false,
  panDrag: null,
  audioRAF: 0,
  hashSilent: false,
};

/* ============================================================
   基础工具
   ============================================================ */
function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"']/g, (c) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}
function chapterMeta(k) { return CHAPTER_META[k] || { label: k || "未分类", color: "#8b97ad" }; }
function categoryMeta(k) { return CATEGORY_META[k] || { label: k || "其他", color: "#8b97ad", icon: "◆" }; }
function isAudio(a) { return !!a && a.media === "audio"; }
function audioUrl(a) { return "/api/audio?id=" + encodeURIComponent(a.id); }
function fmtDuration(sec) {
  const d = Number(sec) || 0;
  if (d >= 60) return Math.floor(d / 60) + ":" + String(Math.round(d % 60)).padStart(2, "0");
  return d >= 10 ? Math.round(d) + "s" : d.toFixed(2).replace(/0+$/, "").replace(/\.$/, "") + "s";
}
function audioTypeLabel(a) { return a.audioType === "bgm" ? "BGM" : "音效"; }
function fmtClock(sec) {
  const d = Math.max(0, Number(sec) || 0);
  return Math.floor(d / 60) + ":" + String(Math.floor(d % 60)).padStart(2, "0");
}
function fmtMB(bytes) { return (Number(bytes || 0) / 1048576).toFixed(1) + " MB"; }
function stateLabel(a, key) {
  if (!a || !a.states) return "";
  const s = a.states.find((x) => x.key === key);
  return s ? s.label : (key || "");
}
function scrubFrames(a) { return Math.max(2, Math.min(Number(a && a.frames) || 12, 16)); }
function chapterOrderIndex(k) { const i = CHAPTER_ORDER.indexOf(k); return i === -1 ? 99 : i; }
function categoryOrderIndex(k) { const i = CATEGORY_ORDER.indexOf(k); return i === -1 ? 99 : i; }

function seedBars(seedText, count) {
  let seed = 0;
  for (const ch of String(seedText)) seed = (seed * 31 + ch.charCodeAt(0)) >>> 0;
  const out = [];
  for (let i = 0; i < count; i++) {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    out.push(0.16 + (seed % 68) / 100);
  }
  return out;
}
function barsFromPeaks(peaks, count) {
  if (!peaks || !peaks.length) return null;
  const out = [];
  for (let i = 0; i < count; i++) {
    const from = Math.floor((i / count) * peaks.length);
    const to = Math.max(from + 1, Math.floor(((i + 1) / count) * peaks.length));
    let m = 0;
    for (let j = from; j < to && j < peaks.length; j++) m = Math.max(m, peaks[j]);
    out.push(Math.max(0.06, Math.min(1, m)));
  }
  return out;
}
function barsHTML(values) {
  return values.map((v) => `<span style="height:${Math.round(10 + v * 84)}%"></span>`).join("");
}

/* ---------------- 请求 ---------------- */
async function jget(url) {
  const r = await fetch(url, { cache: "no-store" });
  const d = await r.json().catch(() => ({}));
  if (!r.ok || d.error) throw new Error(d.error || ("HTTP " + r.status));
  return d;
}
async function jpost(url, body) {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  const d = await r.json().catch(() => ({}));
  if (!r.ok || d.error) throw new Error(d.error || ("HTTP " + r.status));
  return d;
}

/* ---------------- Toast ---------------- */
function toast(message, kind = "info", ms = 2800) {
  const box = $("#toasts");
  if (!box) return;
  const el = document.createElement("div");
  el.className = "toast " + kind;
  el.dataset.test = "toast";
  const icons = { info: "◆", ok: "✓", bad: "✕", warn: "!" };
  el.innerHTML = `<span class="toast-ico">${icons[kind] || "◆"}</span><span class="toast-body">${message}</span>`;
  box.appendChild(el);
  setTimeout(() => {
    el.classList.add("leaving");
    setTimeout(() => el.remove(), 220);
  }, ms);
}

/* ============================================================
   查询参数
   ============================================================ */
function buildQuery(asset, opts = {}) {
  const q = new URLSearchParams();
  q.set("id", asset.id);
  const st = opts.state !== undefined ? opts.state : ((asset.states && asset.states[0]) ? asset.states[0].key : "");
  if (st) q.set("state", st);
  if (opts.params) for (const k in opts.params) q.set("param." + k, opts.params[k]);
  const ov = opts.overlays || {};
  for (const f of ["grid", "axes", "symmetry", "bbox"]) if (ov[f]) q.set(f, "1");
  if (ov.scene) { q.set("scene", "1"); q.set("scenedim", "130"); }
  if (opts.zoom && opts.zoom > 1) q.set("zoom", opts.zoom);
  q.set("palette", S.palette);
  q.set("bg", S.bg);
  if (S.renderBuild) q.set("v", S.renderBuild);
  if (opts.frames) q.set("frames", opts.frames);
  if (opts.frame != null) q.set("frame", opts.frame);
  return q.toString();
}

function thumbUrl(a) {
  const animated = (a.frames || 1) > 1 && S.gifAvailable && S.animThumbs;
  const kind = animated ? "gif" : "png";
  let q = buildQuery(a, { overlays: {} });
  if (animated) q += "&frames=" + Math.min(a.frames, 16);
  return "/api/" + kind + "?" + q;
}

function detailPreviewUrl(opts = {}) {
  const a = S.detail;
  if (!a) return "";
  const frames = Number(a.frames) || 1;
  const animated = frames > 1 && S.gifAvailable && S.overlays.anim && S.detailFrame == null;
  const useFrame = frames > 1 && S.detailFrame != null;
  const kind = animated ? "gif" : (useFrame ? "frame" : "png");
  let q = buildQuery(a, {
    state: S.detailState,
    params: S.detailParams,
    overlays: S.overlays,
    zoom: opts.exportZoom || 0,
  });
  if (animated || useFrame) q += "&frames=" + scrubFrames(a);
  if (useFrame) q += "&frame=" + S.detailFrame;
  return "/api/" + kind + "?" + q;
}

/* ============================================================
   载入素材注册表
   ============================================================ */
function showSkeleton(n) {
  const box = $("#skeleton");
  box.innerHTML = Array.from({ length: n }, () => '<div class="skeleton-card"></div>').join("");
  box.hidden = false;
}
function hideSkeleton() { $("#skeleton").hidden = true; }

async function loadAssets(force = false) {
  showSkeleton(15);
  $("#status").innerHTML = "正在读取素材注册表…";
  try {
    const data = await jget("/api/assets" + (force ? "?force=1" : ""));
    S.gifAvailable = data.gifAvailable !== false;
    S.renderBuild = data.build || S.renderBuild || "";
    S.assets = data.assets || [];
    // 先拿健康信息里的构建时间，再渲染网格：图片 URL 带 v=构建时间，
    // 渲染器重建后浏览器不会继续显示 24h 缓存的旧图（换座舱/改配色时尤其重要）。
    await loadHealth();
    hideSkeleton();
    renderFacets();
    renderGrid();
    updateHero();
    syncDisplayControls();
    await Promise.allSettled([loadDrafts(), checkScanBadge()]);
    routeFromHash(true);
  } catch (e) {
    hideSkeleton();
    $("#status").innerHTML = `<span class="hint">读取失败：${esc(e.message)} · 检查终端里的工坊服务</span>`;
    $("#empty").hidden = false;
    $("#empty").querySelector("h3").textContent = "素材注册表读取失败";
    $("#empty").querySelector("p").textContent = "确认 tools/asset_studio 服务仍在运行，然后点右上角 ⟳ 重试。";
    toast("素材读取失败：" + esc(e.message), "bad", 5000);
  }
}

async function loadDrafts() {
  try {
    const d = await jget("/api/drafts");
    S.drafts = d.drafts || [];
    const sec = $("#drafts-section");
    sec.hidden = S.drafts.length === 0;
    $("#f-drafts").innerHTML = S.drafts.map((x) => {
      const promoted = x.status === "promoted";
      return `<div class="facet-item" data-draft="${esc(x.id)}" title="${esc(x.dir)}">
        <span class="dot" style="--c:${promoted ? "#7ee0a0" : "#ff8fc7"}"></span>
        <span class="facet-name">${esc(x.title || x.id)}</span>
        <span class="cnt">${promoted ? "已转正" : "草稿"}</span></div>`;
    }).join("");
    $$("#f-drafts [data-draft]").forEach((el) => {
      el.onclick = () => {
        if (S.assets.some((a) => a.id === el.dataset.draft)) openDetail(el.dataset.draft);
        else toast("草稿代码尚未编译：重新构建工坊后再刷新素材。", "warn");
      };
    });
  } catch (e) { /* 草稿目录为空时静默 */ }
}

function scanFileKind(file) {
  const f = String(file || "");
  if (f.includes("game.h")) return "屏幕级 UI · 开始/暂停/结算画面";
  if (f.includes("save_menu.h")) return "存读档界面";
  if (f.includes("renderer.h")) return "Renderer 内部包装 · 不是素材";
  if (f.includes("demo_stroke_font.h") || f.includes("font.h")) return "字体内部方法";
  if (f.includes("floating_text.h")) return "空实现 · 真正画字在 game.h";
  if (f.includes("player.h")) return "抽象基类空实现 · 不是素材";
  return "私有零件方法 / 子绘制";
}

async function checkScanBadge() {
  try {
    const d = await jget("/api/scan");
    const n = (d.uncovered || []).length + (d.audioUncovered || []).length;
    const btn = $("#btn-scan");
    btn.classList.toggle("ok", n === 0);
    btn.title = n > 0
      ? `有 ${n} 个绘制 / 音频入口还没有登记进画廊`
      : "所有绘制与音频入口都已登记";
    $(".scan-label", btn).textContent = n > 0 ? `未登记 ${n}` : "未登记检查 ✓";
    return d;
  } catch (e) { return null; }
}

async function loadHealth() {
  try {
    const h = await jget("/api/health");
    S.health = h;
    if (h && h.builtAt) S.renderBuild = h.builtAt;
    $("#engine-dot").className = "engine-dot ok";
    const cache = h.cache ? `${h.cache.files} 个缓存文件 · ${fmtMB(h.cache.bytes)}` : "渲染引擎正常";
    $("#engine-text").textContent = cache;
    const dot = $("#hero-engine-dot");
    const state = $("#hero-engine-state");
    const sub = $("#hero-engine-sub");
    if (dot) dot.className = "engine-dot ok";
    if (state) state.textContent = "在线 · 渲染器已编译";
    if (sub) {
      const built = String(h.builtAt || "").replace(/^\d{4}-/, "");
      sub.textContent = (built ? `构建 ${built} · ` : "") +
        (h.cache ? `缓存 ${h.cache.files} 文件 / ${fmtMB(h.cache.bytes)}` : "C++ · SDL2 无头渲染");
    }
    return h;
  } catch (e) {
    S.health = null;
    $("#engine-dot").className = "engine-dot bad";
    $("#engine-text").textContent = "渲染引擎离线";
    return null;
  }
}

/* ============================================================
   Hero / 侧栏 / 筛选
   ============================================================ */
function updateHero() {
  const total = S.assets.length;
  const audio = S.assets.filter(isAudio).length;
  $("#stat-total").textContent = total;
  $("#stat-image").textContent = total - audio;
  $("#stat-audio").textContent = audio;
  $("#stat-fav").textContent = S.favorites.size;
  const chap = new Set(S.assets.map((a) => a.chapter)).size;
  const cat = new Set(S.assets.map((a) => a.category)).size;
  const states = S.assets.reduce((n, a) => n + (a.states || []).length, 0);
  const params = S.assets.reduce((n, a) => n + (a.params || []).length, 0);
  $("#hero-meta").innerHTML =
    `已登记 <b>${total}</b> 个素材 · <b>${chap}</b> 个章节 · <b>${cat}</b> 类 ` +
    `· <b>${states}</b> 个可切换状态 · <b>${params}</b> 个可调参数`;
  const qf = $("#q-fav");
  if (qf) qf.textContent = S.favorites.size;
}

function countBy(key) {
  const m = {};
  S.assets.forEach((a) => { const v = a[key] || "?"; m[v] = (m[v] || 0) + 1; });
  return m;
}

function facetItem(label, val, count, on, facet, color) {
  return `<div class="facet-item ${on ? "on" : ""}" data-facet="${facet}" data-val="${esc(val == null ? "" : val)}">
    ${color ? `<span class="dot" style="--c:${color}"></span>` : ""}
    <span class="facet-name">${esc(label)}</span><span class="cnt">${count}</span></div>`;
}

function renderFacets() {
  const chap = countBy("chapter");
  const chapKeys = Object.keys(chap).sort((a, b) => chapterOrderIndex(a) - chapterOrderIndex(b) || a.localeCompare(b));
  $("#f-chapter").innerHTML =
    facetItem("全部章节", null, S.assets.length, S.filters.chapter === null, "chapter") +
    chapKeys.map((k) => facetItem(chapterMeta(k).label, k, chap[k], S.filters.chapter === k, "chapter", chapterMeta(k).color)).join("");

  const cat = countBy("category");
  const catKeys = Object.keys(cat).sort((a, b) => categoryOrderIndex(a) - categoryOrderIndex(b) || a.localeCompare(b));
  $("#f-category").innerHTML =
    facetItem("全部类型", null, S.assets.length, S.filters.category === null, "category") +
    catKeys.map((k) => facetItem(categoryMeta(k).label, k, cat[k], S.filters.category === k, "category", categoryMeta(k).color)).join("");

  const tagCount = {};
  S.assets.forEach((a) => (a.tags || []).forEach((t) => { tagCount[t] = (tagCount[t] || 0) + 1; }));
  const tags = Object.keys(tagCount).sort((a, b) => tagCount[b] - tagCount[a] || a.localeCompare(b));
  $("#f-tags").innerHTML = tags.slice(0, 40).map((t) =>
    `<div class="facet-item ${S.filters.tag === t ? "on" : ""}" data-facet="tag" data-val="${esc(t)}" title="${t} · ${tagCount[t]} 个素材">${esc(t)}</div>`
  ).join("") || '<span class="muted" style="font-size:12px">暂无标签</span>';

  $$("[data-facet]").forEach((el) => {
    el.onclick = () => {
      const f = el.dataset.facet;
      const v = el.dataset.val;
      S.filters[f] = (v === "" || S.filters[f] === v) ? null : v;
      renderFacets();
      renderGrid();
    };
  });

  $$("#quick-filters .quick").forEach((el) => {
    el.classList.toggle("on", !!S.filters[el.dataset.quick]);
    el.onclick = () => { S.filters[el.dataset.quick] = !S.filters[el.dataset.quick]; renderFacets(); renderGrid(); };
  });

  renderRecents();
}

function renderRecents() {
  const sec = $("#recent-section");
  const list = S.recents.map((id) => S.assets.find((a) => a.id === id)).filter(Boolean).slice(0, 7);
  sec.hidden = list.length === 0;
  $("#f-recent").innerHTML = list.map((a) =>
    `<div class="facet-item" data-recent="${esc(a.id)}" title="${esc(a.id)}">
      <span class="dot" style="--c:${chapterMeta(a.chapter).color}"></span>
      <span class="facet-name">${esc(a.name)}</span></div>`).join("");
  $$("#f-recent [data-recent]").forEach((el) => {
    el.onclick = () => openDetail(el.dataset.recent);
  });
}

function activeFilterCount() {
  const f = S.filters;
  return [f.chapter, f.category, f.tag, f.q, f.fav ? "fav" : null, f.anim ? "anim" : null, f.scene ? "scene" : null]
    .filter(Boolean).length;
}

function renderActiveChips() {
  const f = S.filters;
  const chips = [];
  if (f.chapter) chips.push(["章节 · " + chapterMeta(f.chapter).label, "chapter"]);
  if (f.category) chips.push(["类型 · " + categoryMeta(f.category).label, "category"]);
  if (f.tag) chips.push(["标签 · " + f.tag, "tag"]);
  if (f.q) chips.push(["搜索 · " + f.q, "q"]);
  if (f.fav) chips.push(["只看收藏", "fav"]);
  if (f.anim) chips.push(["有动画", "anim"]);
  if (f.scene) chips.push(["可叠场景", "scene"]);
  $("#active-chips").innerHTML = chips.map(([label, key]) =>
    `<span class="chip">${esc(label)}<button data-chip="${key}" title="移除筛选" aria-label="移除">✕</button></span>`).join("");
  $$("[data-chip]").forEach((btn) => {
    btn.onclick = () => {
      const k = btn.dataset.chip;
      const map = { fav: "fav", anim: "anim", scene: "scene" };
      if (map[k]) S.filters[k] = false;
      else if (k === "q") { S.filters.q = ""; $("#search").value = ""; }
      else S.filters[k] = null;
      renderFacets(); renderGrid();
    };
  });
}

function clearFilters() {
  S.filters = { chapter: null, category: null, tag: null, q: "", fav: false, anim: false, scene: false };
  $("#search").value = "";
  renderFacets();
  renderGrid();
}

function visibleAssets() {
  const f = S.filters;
  const q = f.q.trim().toLowerCase();
  let list = S.assets.filter((a) => {
    if (f.chapter && a.chapter !== f.chapter) return false;
    if (f.category && a.category !== f.category) return false;
    if (f.tag && !(a.tags || []).includes(f.tag)) return false;
    if (f.fav && !S.favorites.has(a.id)) return false;
    if (f.anim && !(a.media !== "audio" && (a.frames || 1) > 1)) return false;
    if (f.scene && !a.scene) return false;
    if (q) {
      const hay = [a.name, a.id, a.desc, (a.tags || []).join(" "), a.category, a.chapter,
        categoryMeta(a.category).label, chapterMeta(a.chapter).label, a.audioType || "", a.media || ""]
        .join(" ").toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  });
  const q2 = f.q.trim().toLowerCase();
  if (q2) {
    // 搜索时先按相关度排：名称/ID 命中优先于标签/描述命中
    list.sort((x, y) => searchScore(y, q2) - searchScore(x, q2) || x.name.localeCompare(y.name, "zh-Hans-CN"));
  } else {
    list = sortAssets(list);
  }
  return list;
}

function searchScore(a, q) {
  const name = String(a.name || "").toLowerCase();
  const id = String(a.id || "").toLowerCase();
  const tags = (a.tags || []).join(" ").toLowerCase();
  const desc = String(a.desc || "").toLowerCase();
  let score = 0;
  if (id === q || id.endsWith("." + q)) score += 120;
  else if (id.includes("." + q + ".")) score += 60;
  if (name === q) score += 100;
  if (name.includes(q)) score += 60;
  if (id.includes(q)) score += 40;
  if (tags.includes(q)) score += 25;
  if (desc.includes(q)) score += 10;
  return score;
}

function sortAssets(list) {
  const arr = list.slice();
  switch (S.sort) {
    case "name": arr.sort((a, b) => a.name.localeCompare(b.name, "zh-Hans-CN")); break;
    case "chapter": arr.sort((a, b) => chapterOrderIndex(a.chapter) - chapterOrderIndex(b.chapter) || a.name.localeCompare(b.name, "zh-Hans-CN")); break;
    case "category": arr.sort((a, b) => categoryOrderIndex(a.category) - categoryOrderIndex(b.category) || a.name.localeCompare(b.name, "zh-Hans-CN")); break;
    case "frames": arr.sort((a, b) => (b.frames || 1) - (a.frames || 1) || a.name.localeCompare(b.name, "zh-Hans-CN")); break;
    case "fav": arr.sort((a, b) => (S.favorites.has(b.id) ? 1 : 0) - (S.favorites.has(a.id) ? 1 : 0)); break;
    default: break;
  }
  return arr;
}

/* ============================================================
   缩略图墙
   ============================================================ */
function cardHTML(a, index) {
  const cat = categoryMeta(a.category);
  const ch = chapterMeta(a.chapter);
  const fav = S.favorites.has(a.id);
  const audio = isAudio(a);
  const animated = !audio && (a.frames || 1) > 1;
  const delay = index < 20 ? ` style="animation-delay:${index * 14}ms"` : "";
  const favBtn = `<button class="fav-btn ${fav ? "on" : ""}" data-fav="${esc(a.id)}" title="${fav ? "取消收藏" : "收藏"}" aria-pressed="${fav}" aria-label="收藏">${fav ? "★" : "☆"}</button>`;

  if (audio) {
    const bars = seedBars(a.id, 34);
    return `<article class="card audio-card" data-id="${esc(a.id)}" tabindex="0" role="button" aria-label="${esc(a.name)}，${audioTypeLabel(a)}"${delay}>
      <div class="card-thumb audio-thumb">
        <span class="card-cat"><span class="cat-dot" style="--c:${cat.color}"></span>${esc(cat.label)}</span>
        ${favBtn}
        <div class="audio-wave">${barsHTML(bars)}</div>
        <button class="audio-play-btn" data-audio-id="${esc(a.id)}" title="试听 ${esc(a.name)}" aria-label="试听">▶</button>
        <span class="audio-kind">${esc(audioTypeLabel(a))}</span>
        <span class="audio-duration">${esc(fmtDuration(a.duration))}</span>
        <div class="audio-progress-line" data-progress="${esc(a.id)}"></div>
      </div>
      <div class="card-body">
        <div class="card-name-row"><h3 class="card-name">${esc(a.name)}</h3></div>
        <div class="card-id">${esc(a.id)}</div>
        <div class="card-tags">${(a.tags || []).slice(0, 4).map((t) => `<span class="tag">${esc(t)}</span>`).join("")}</div>
        <div class="card-foot">
          <span class="chip-cat ch" style="--ch:${ch.color}">${esc(ch.label)}</span>
          <span class="chip-cat">${esc(audioTypeLabel(a))} 试听</span>
        </div>
      </div>
    </article>`;
  }

  const badges = [];
  if (animated) badges.push(`<span class="thumb-badge anim">▶ ${a.frames} 帧</span>`);
  if (a.scene) badges.push('<span class="thumb-badge scene">可叠场景</span>');
  return `<article class="card" data-id="${esc(a.id)}" tabindex="0" role="button" aria-label="${esc(a.name)}"${delay}>
    <div class="card-thumb">
      <span class="card-cat"><span class="cat-dot" style="--c:${cat.color}"></span>${esc(cat.label)}</span>
      ${favBtn}
      <img loading="lazy" decoding="async" src="${esc(thumbUrl(a))}" alt="${esc(a.name)}" title="${esc(a.name)} · ${esc(a.id)}" draggable="false">
      <span class="thumb-hint">打开详情 →</span>
      <div class="thumb-badges">${badges.join("")}</div>
    </div>
    <div class="card-body">
      <div class="card-name-row"><h3 class="card-name">${esc(a.name)}</h3></div>
      <div class="card-id">${esc(a.id)}</div>
      <div class="card-tags">${(a.tags || []).slice(0, 4).map((t) => `<span class="tag">${esc(t)}</span>`).join("")}</div>
      <div class="card-foot">
        <span class="chip-cat ch" style="--ch:${ch.color}">${esc(ch.label)}</span>
        <span class="chip-cat">${esc(cat.label)}</span>
      </div>
    </div>
  </article>`;
}

function renderGrid(opts = {}) {
  stopCardAudio();
  const list = visibleAssets();
  S.visibleIds = list.map((a) => a.id);
  const total = S.assets.length;

  const audioCount = S.assets.filter(isAudio).length;
  $("#status").innerHTML = total === 0
    ? '<span class="hint">注册表是空的</span>'
    : (list.length === total
      ? `共 <b>${total}</b> 个素材 · <b>${total - audioCount}</b> 绘制 / <b>${audioCount}</b> 声音`
      : `筛选中 <b>${list.length}</b> / ${total} 个素材`);

  renderActiveChips();

  const grid = $("#grid");
  grid.className = "grid density-" + S.density + (S.view === "list" ? " list" : "");
  grid.innerHTML = list.map(cardHTML).join("");
  $("#empty").hidden = list.length > 0;
  if (list.length === 0 && total > 0) {
    $("#empty").querySelector("h3").textContent = "没有符合条件的素材";
    $("#empty").querySelector("p").textContent = "换个关键词，或者把筛选器清空看看全部素材。";
  }

  bindGrid();
  observeAudioCards();
  if (opts.scrollTop !== false) window.scrollTo({ top: 0, behavior: "smooth" });
}

/* 把缩略图按内容自适应放大到卡片可视区（最近邻，保持像素锐利） */
function fitCardImage(img) {
  const thumb = img.closest(".card-thumb");
  if (!thumb || !img.naturalWidth) return;
  const boxW = Math.max(40, thumb.clientWidth * 0.84);
  const boxH = Math.max(30, thumb.clientHeight * 0.76);
  const s = Math.min(boxW / img.naturalWidth, boxH / img.naturalHeight);
  img.style.width = Math.max(1, Math.round(img.naturalWidth * s)) + "px";
  img.style.height = Math.max(1, Math.round(img.naturalHeight * s)) + "px";
}
function refitCards() {
  $$("#grid .card-thumb img").forEach((img) => { if (img.naturalWidth) fitCardImage(img); });
}

function bindGrid() {
  const grid = $("#grid");

  $$("#grid .card-thumb img").forEach((img) => {
    if (img.complete && img.naturalWidth) { img.classList.add("loaded"); fitCardImage(img); }
    img.addEventListener("load", () => { img.classList.add("loaded"); fitCardImage(img); });
    img.addEventListener("error", () => {
      const thumb = img.closest(".card-thumb");
      img.remove();
      if (thumb && !thumb.querySelector(".thumb-fallback")) {
        const s = document.createElement("span");
        s.className = "thumb-fallback";
        s.textContent = "◌";
        thumb.appendChild(s);
      }
    });
  });

  grid.onclick = (e) => {
    const fav = e.target.closest("[data-fav]");
    if (fav) { e.stopPropagation(); toggleFavorite(fav.dataset.fav); return; }
    const play = e.target.closest("[data-audio-id]");
    if (play) {
      e.stopPropagation();
      const card = play.closest(".card");
      const a = S.assets.find((x) => x.id === play.dataset.audioId);
      if (a) playCardAudio(a, play, card);
      return;
    }
    const card = e.target.closest(".card");
    if (card) openDetail(card.dataset.id);
  };

  grid.onkeydown = (e) => {
    const card = e.target.closest(".card");
    if (!card) return;
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      if (e.target.closest("[data-fav]") || e.target.closest("[data-audio-id]")) return;
      openDetail(card.dataset.id);
    }
  };
}

function observeAudioCards() {
  if (S.cardObserver) S.cardObserver.disconnect();
  S.cardObserver = new IntersectionObserver((entries) => {
    entries.forEach((entry) => {
      if (!entry.isIntersecting) return;
      const card = entry.target;
      S.cardObserver.unobserve(card);
      hydrateAudioCard(card);
    });
  }, { rootMargin: "240px 0px" });
  $$("#grid .audio-card").forEach((card) => S.cardObserver.observe(card));
}

async function fetchWaveform(asset) {
  if (S.waveCache.has(asset.id)) return S.waveCache.get(asset.id);
  const p = jget("/api/waveform?id=" + encodeURIComponent(asset.id))
    .then((d) => (d && d.peaks && d.peaks.length ? d : null))
    .catch(() => null);
  S.waveCache.set(asset.id, p);
  return p;
}

async function hydrateAudioCard(card) {
  const a = S.assets.find((x) => x.id === card.dataset.id);
  if (!a) return;
  const wave = card.querySelector(".audio-wave");
  const deco = seedBars(a.id, 34);
  const d = await fetchWaveform(a);
  if (!d) {
    if (wave && !wave.querySelector(".real-bar")) wave.innerHTML = barsHTML(deco);
    return;
  }
  const bars = barsFromPeaks(d.peaks, 34);
  if (wave && bars) {
    wave.classList.add("real");
    wave.innerHTML = bars.map((v) => `<span class="real-bar" style="height:${Math.round(10 + v * 84)}%"></span>`).join("");
  }
}

/* ============================================================
   收藏
   ============================================================ */
function saveFavorites() {
  LS.set("favorites", Array.from(S.favorites));
  updateHero();
  const qf = $("#q-fav");
  if (qf) qf.textContent = S.favorites.size;
  $$("#quick-filters [data-quick='fav']").forEach((el) => el.classList.toggle("on", !!S.filters.fav));
}
function toggleFavorite(id) {
  if (S.favorites.has(id)) S.favorites.delete(id);
  else S.favorites.add(id);
  saveFavorites();
  $$(`[data-fav="${CSS.escape(id)}"]`).forEach((btn) => {
    const on = S.favorites.has(id);
    btn.classList.toggle("on", on);
    btn.textContent = on ? "★" : "☆";
    btn.title = on ? "取消收藏" : "收藏";
    btn.setAttribute("aria-pressed", String(on));
  });
  const a = S.assets.find((x) => x.id === id);
  const added = S.favorites.has(id);
  toast((added ? "已收藏 " : "已取消收藏 ") + "「" + esc(a ? a.name : id) + "」", added ? "ok" : "info", 1600);
  if (S.detail && S.detail.id === id) updateDetailFavorite();
  if (S.filters.fav) renderGrid();
}
function updateDetailFavorite() {
  const on = S.detail && S.favorites.has(S.detail.id);
  const btn = $("#btn-fav");
  if (!btn) return;
  btn.classList.toggle("on", !!on);
  btn.textContent = on ? "★" : "☆";
  btn.setAttribute("aria-pressed", String(!!on));
}

function addRecent(id) {
  S.recents = [id].concat(S.recents.filter((x) => x !== id)).slice(0, 12);
  LS.set("recents", S.recents);
  renderRecents();
}

/* ============================================================
   音频缩略图试听
   ============================================================ */
function stopCardAudio() {
  if (S.cardAudio) {
    try { S.cardAudio.pause(); S.cardAudio.src = ""; } catch (e) { /* 忽略 */ }
    S.cardAudio = null;
  }
  if (S.cardAudioBtn) {
    S.cardAudioBtn.textContent = "▶";
    S.cardAudioBtn.classList.remove("playing", "loading");
  }
  if (S.cardAudioCard) {
    const wave = S.cardAudioCard.querySelector(".audio-wave");
    if (wave) wave.classList.remove("playing");
    const line = S.cardAudioCard.querySelector(".audio-progress-line");
    if (line) line.style.transform = "scaleX(0)";
  }
  S.cardAudioId = null;
  S.cardAudioBtn = null;
  S.cardAudioCard = null;
}

function playCardAudio(asset, btn, card) {
  if (S.cardAudioId === asset.id && S.cardAudio && !S.cardAudio.paused) { stopCardAudio(); return; }
  stopCardAudio();
  S.cardAudioId = asset.id;
  S.cardAudioBtn = btn;
  S.cardAudioCard = card;
  btn.textContent = "…";
  btn.classList.add("loading");
  const player = new Audio(audioUrl(asset));
  S.cardAudio = player;
  player.onplaying = () => {
    btn.textContent = "❚❚";
    btn.classList.remove("loading");
    btn.classList.add("playing");
    const wave = card.querySelector(".audio-wave");
    if (wave) wave.classList.add("playing");
  };
  player.ontimeupdate = () => {
    const line = card.querySelector(".audio-progress-line");
    if (line) {
      const dur = player.duration || asset.duration || 1;
      line.style.transform = "scaleX(" + Math.min(1, player.currentTime / dur).toFixed(3) + ")";
    }
  };
  player.onended = () => { if (S.cardAudio === player) stopCardAudio(); };
  player.onerror = () => { if (S.cardAudio === player) { stopCardAudio(); toast("试听失败：音频生成中或引擎未编译", "bad"); } };
  player.play().catch(() => { if (S.cardAudio === player) stopCardAudio(); });
}

/* ============================================================
   详情面板
   ============================================================ */
function draftFor(a) {
  return S.drafts.find((d) => a && (d.id === a.id || d.dir === a.id)) || null;
}
function defaultParamsFor(a) {
  const out = {};
  const draft = draftFor(a);
  (a.params || []).forEach((p) => {
    let v = p.def;
    if (draft && draft.tuned && Number.isFinite(Number(draft.tuned[p.key]))) v = Number(draft.tuned[p.key]);
    out[p.key] = Math.max(p.min, Math.min(p.max, v));
  });
  return out;
}
function defaultStateFor(a) {
  const draft = draftFor(a);
  const saved = draft && draft.state;
  if ((a.states || []).some((s) => s.key === saved)) return saved;
  return (a.states && a.states[0]) ? a.states[0].key : "";
}

function openDetail(id, opts = {}) {
  const a = S.assets.find((x) => x.id === id);
  if (!a) { toast("找不到素材：" + esc(id), "bad"); return; }
  const prevId = S.detail ? S.detail.id : null;
  S.detail = a;
  stopCardAudio();

  if (!opts.keepView && (!opts.silent || prevId !== id)) {
    S.detailState = opts.stateFromHash && (a.states || []).some((s) => s.key === opts.stateFromHash)
      ? opts.stateFromHash : defaultStateFor(a);
    S.detailParams = defaultParamsFor(a);
    S.overlays = {
      scene: false, grid: false, axes: false, symmetry: false, bbox: false,
      anim: (a.frames || 1) > 1 && S.gifAvailable,
    };
    S.zoom = 1;
    S.exportZoom = 1;
    S.detailFrame = null;
    S.compare = null;
    S.detailTab = "overview";
    S.wavePeaks = null;
    S.analysisLoaded = false;
    S.sourceLoaded = false;
  }

  S.lastFocus = document.activeElement;

  renderDetailShell(a);
  showTab("overview", true);
  $("#detail").hidden = false;
  document.body.style.overflow = "hidden";

  if (a.media === "audio") setupAudioDetail(a);
  else setupImageDetail(a);

  addRecent(id);
  if (!opts.silent) pushDetailHash(a);
  setTimeout(() => { try { $("#detail-close").focus({ preventScroll: true }); } catch (e) { /* 忽略 */ } }, 30);
}

function closeDetail(opts = {}) {
  if (!S.detail) return;
  S.analysisRequest++;
  clearTimeout(S.analysisTimer);
  const player = $("#detail-audio");
  if (player) { try { player.pause(); } catch (e) { /* 忽略 */ } }
  S.compare = null;
  S.detailFrame = null;
  $("#detail").hidden = true;
  document.body.style.overflow = "";
  if (!opts.silent) clearHash();
  if (S.lastFocus && document.contains(S.lastFocus)) {
    try { S.lastFocus.focus({ preventScroll: true }); } catch (e) { /* 忽略 */ }
  }
  S.detail = null;
}

function renderDetailShell(a) {
  const ch = chapterMeta(a.chapter);
  const cat = categoryMeta(a.category);
  const audio = isAudio(a);

  const kickerCat = audio ? "声音素材" : cat.label;
  $("#detail-kicker").innerHTML =
    `<span style="color:${ch.color}">${esc(ch.label)}</span> · <span style="color:${cat.color}">${esc(kickerCat)}</span>` +
    (audio ? ` · <span style="color:#4fe0c8">${esc(audioTypeLabel(a))}</span>` : ` · <span class="muted">绘制素材</span>`);
  $("#detail-name").textContent = a.name;
  $("#detail-id").textContent = a.id;
  $("#detail-desc").textContent = a.desc || "（暂无描述）";
  $("#detail-tags").innerHTML = (a.tags || []).map((t) => `<span class="tag">${esc(t)}</span>`).join("");

  // 元信息
  const meta = [];
  meta.push(["素材 ID", a.id, true]);
  meta.push(["章节", ch.label]);
  meta.push(["类型", cat.label]);
  if (audio) {
    meta.push(["媒体", audioTypeLabel(a)]);
    meta.push(["时长", fmtDuration(a.duration)]);
    meta.push(["合成方式", "audio.h 实时合成"]);
  } else {
    meta.push(["默认帧数", String(a.frames || 1)]);
    meta.push(["可切换状态", String((a.states || []).length)]);
    meta.push(["可调参数", String((a.params || []).length)]);
    meta.push(["场景上下文", a.scene ? "可叠加" : "无"]);
  }
  const src = a.source || "";
  const m = src.match(/^(.*):(\d+)/);
  meta.push(["源码位置", m ? `${m[1].split("/").pop()}:${m[2]}` : (src || "未记录"), true, !!src]);
  $("#detail-meta").innerHTML = meta.map(([k, v, mono, wide]) =>
    `<div class="meta-item ${wide || mono ? "wide" : ""}">
      <span class="meta-k">${esc(k)}</span>
      <span class="meta-v ${mono ? "mono" : ""}" title="${esc(v)}">${esc(v)}</span></div>`).join("");

  // 零件关系（门禁 → 真空门 / 扫描光束 / 脉冲光环；场景 → 子零件）
  renderParts(a);

  // 相关素材
  renderRelated(a);

  // 场景说明
  $("#scene-hint-wrap").hidden = audio || !a.scene;
  if (a.scene) {
    const sceneAsset = S.assets.find((x) => x.id === a.scene);
    $("#scene-hint").innerHTML = `预览时可叠加 <b>${esc(sceneAsset ? sceneAsset.name : a.scene)}</b> 作为真实场景上下文。` +
      `打开「真实场景」开关可以看到它在游戏里的实际观感。`;
  }

  // 状态
  const states = a.states || [];
  $("#detail-states-wrap").hidden = audio || states.length === 0;
  $("#state-hint").textContent = states.length ? "当前：" + stateLabel(a, S.detailState) : "";
  $("#detail-states").innerHTML = states.map((s) =>
    `<button class="state-btn ${s.key === S.detailState ? "on" : ""}" data-state="${esc(s.key)}">${esc(s.label)}</button>`).join("");
  $$("#detail-states .state-btn").forEach((b) => {
    b.onclick = () => { setDetailState(b.dataset.state); };
  });

  // 参数
  const params = a.params || [];
  $("#detail-params-wrap").hidden = audio || params.length === 0;
  $("#params-empty").hidden = audio || params.length > 0;
  $("#detail-params").innerHTML = params.map((p) => `
    <div class="param-row">
      <label title="${esc(p.key)}">${esc(p.label)}</label>
      <input type="range" data-pk="${esc(p.key)}" min="${p.min}" max="${p.max}" step="${p.step}" value="${S.detailParams[p.key]}" aria-label="${esc(p.label)}">
      <span class="val" data-pv="${esc(p.key)}">${fmtParamVal(S.detailParams[p.key], p.step)}</span>
    </div>`).join("");
  $$("#detail-params input[type=range]").forEach((r) => {
    r.oninput = () => {
      const key = r.dataset.pk;
      S.detailParams[key] = parseFloat(r.value);
      const spec = params.find((p) => p.key === key);
      const val = $(`[data-pv="${CSS.escape(key)}"]`);
      if (val) val.textContent = fmtParamVal(S.detailParams[key], spec ? spec.step : 0.1);
      clearTimeout(r._t);
      r._t = setTimeout(() => { refreshPreview(); scheduleAnalysis(); }, 55);
    };
  });

  const paramCount = (a.params || []).length;
  const stateCount = (a.states || []).length;
  $("#tab-param-count").textContent = audio ? "" : String(paramCount + stateCount) || "";
  $("#btn-reset-params").hidden = !params.length;
  $("#btn-save-draft").hidden = audio || !a.id.startsWith("draft.");
  $("#btn-copy-code").hidden = audio;
  $("#btn-clone").hidden = audio;
  $("#btn-download").textContent = audio ? "下载 WAV" : (S.detailFrame == null && (a.frames || 1) > 1 && S.overlays.anim && S.gifAvailable ? "下载 GIF" : "下载 PNG");
  $("#action-msg").textContent = "";
  $("#action-msg").className = "action-msg";
  $("#source-loc").textContent = "";
  $("#detail-source").innerHTML = '<span class="muted">切到「源码」页自动加载，或点下方「查看源码」。</span>';
  $("#detail-analysis").innerHTML = '<span class="muted">加载中…</span>';

  updateDetailFavorite();
  updateDetailNav();

  // 音频素材隐藏「参数」页
  const paramTab = $('.tab[data-tab="params"]');
  if (paramTab) paramTab.hidden = audio;
}

function fmtParamVal(v, step) {
  const n = Number(v);
  if (!Number.isFinite(n)) return "0";
  if (step && step < 0.1) return n.toFixed(1);
  return String(Math.round(n * 1000) / 1000);
}

function renderParts(a) {
  const wrap = $("#parts-wrap");
  if (!wrap) return;
  const children = S.assets.filter((x) => x.id !== a.id && x.id.startsWith(a.id + "."));
  let parent = null;
  const segs = a.id.split(".");
  for (let i = segs.length - 1; i > 0; i--) {
    const cand = segs.slice(0, i).join(".");
    const p = S.assets.find((x) => x.id === cand);
    if (p) { parent = p; break; }
  }
  const items = [];
  if (parent) items.push({ a: parent, role: "所属场景" });
  children.forEach((c) => items.push({ a: c, role: "零件" }));
  wrap.hidden = items.length === 0;
  if (!items.length) return;
  $("#parts-list").innerHTML = items.map(({ a: x, role }) => `
    <button class="related-item" data-part="${esc(x.id)}" title="${esc(x.id)}">
      ${isAudio(x) ? '<span class="related-thumb" style="display:grid;place-items:center;color:#4fe0c8;font-size:13px">♫</span>'
        : `<span class="related-thumb" style="background-image:url('${esc(thumbUrl(x))}')"></span>`}
      <span>${role === "所属场景" ? "↖ " : ""}${esc(x.name)}</span>
    </button>`).join("");
  $$("#parts-list [data-part]").forEach((el) => {
    el.onclick = () => openDetail(el.dataset.part);
  });
}

function partIdsFor(a) {
  const ids = new Set();
  S.assets.forEach((x) => { if (x.id !== a.id && x.id.startsWith(a.id + ".")) ids.add(x.id); });
  const segs = a.id.split(".");
  for (let i = segs.length - 1; i > 0; i--) {
    const cand = segs.slice(0, i).join(".");
    const p = S.assets.find((x) => x.id === cand);
    if (p) { ids.add(p.id); break; }
  }
  return ids;
}

function renderRelated(a) {
  const mine = new Set(a.tags || []);
  const partIds = partIdsFor(a);
  let rel = S.assets.filter((x) => x.id !== a.id && !partIds.has(x.id)).map((x) => {
    let score = 0;
    if (x.category === a.category) score += 2;
    if (x.chapter === a.chapter) score += 1;
    (x.tags || []).forEach((t) => { if (mine.has(t)) score += 1; });
    return { a: x, score };
  }).filter((x) => x.score >= 3).sort((x, y) => y.score - x.score).slice(0, 6).map((x) => x.a);
  if (rel.length < 4) {
    rel = rel.concat(S.assets.filter((x) => x.id !== a.id && !partIds.has(x.id) &&
      (x.category === a.category || x.chapter === a.chapter) &&
      !rel.some((r) => r.id === x.id)).slice(0, 6 - rel.length));
  }
  $("#related-wrap").hidden = rel.length === 0;
  $("#related-list").innerHTML = rel.map((x) => `
    <button class="related-item" data-related="${esc(x.id)}" title="${esc(x.id)}">
      ${isAudio(x) ? '<span class="related-thumb" style="display:grid;place-items:center;color:#4fe0c8;font-size:13px">♫</span>'
        : `<span class="related-thumb" style="background-image:url('${esc(thumbUrl(x))}')"></span>`}
      <span>${esc(x.name)}</span>
    </button>`).join("");
  $$("#related-list [data-related]").forEach((el) => {
    el.onclick = () => openDetail(el.dataset.related);
  });
}

function updateDetailNav() {
  const list = S.visibleIds.length ? S.visibleIds : S.assets.map((a) => a.id);
  let i = list.indexOf(S.detail.id);
  if (i === -1) { const all = S.assets.map((a) => a.id); i = all.indexOf(S.detail.id); }
  const total = (list.includes(S.detail.id) ? list : S.assets.map((a) => a.id)).length;
  $("#detail-prev").disabled = total <= 1;
  $("#detail-next").disabled = total <= 1;
}

function detailStep(dir) {
  if (!S.detail) return;
  let list = S.visibleIds.length ? S.visibleIds : S.assets.map((a) => a.id);
  let i = list.indexOf(S.detail.id);
  if (i === -1) { list = S.assets.map((a) => a.id); i = list.indexOf(S.detail.id); }
  if (!list.length || i === -1) return;
  const next = list[(i + dir + list.length) % list.length];
  openDetail(next);
}

function setDetailState(key) {
  const a = S.detail;
  if (!a || !(a.states || []).some((s) => s.key === key)) return;
  S.detailState = key;
  $$("#detail-states .state-btn").forEach((b) => b.classList.toggle("on", b.dataset.state === key));
  $("#state-hint").textContent = "当前：" + stateLabel(a, key);
  refreshPreview();
  scheduleAnalysis();
  updateDetailHash();
  const tab = $(".tab.on");
  if (tab && tab.dataset.tab === "source") { S.sourceLoaded = false; loadSource(); }
}

/* ---------------- 图片 / 音频初始化 ---------------- */
function setupImageDetail(a) {
  applyStageBg();
  $("#audio-stage").hidden = true;
  $(".stage-bar").hidden = false;
  $("#stage-viewport").hidden = false;
  $(".stage-foot").hidden = false;
  updateCompareUI();

  const img = $("#detail-img");
  img.classList.remove("loaded");
  $("#stage-empty").hidden = true;

  // 场景开关
  const sceneChk = $("#t-scene");
  const sceneWrap = $("#chk-scene-wrap");
  sceneChk.disabled = !a.scene;
  sceneWrap.style.opacity = "";
  sceneWrap.title = a.scene ? "叠加真实场景：" + a.scene : "该素材没有声明场景上下文";

  // 动画开关
  const animChk = $("#t-anim");
  const animWrap = $("#chk-anim-wrap");
  const animated = (a.frames || 1) > 1;
  $(".stage-foot .select-wrap #t-zoom").value = "1";
  S.exportZoom = 1;
  animChk.disabled = !animated || !S.gifAvailable;
  animWrap.style.opacity = "";
  animChk.title = !S.gifAvailable ? "当前 Python 未安装 Pillow，动画播放不可用（仍可逐帧查看）" : (animated ? "" : "单帧素材");
  animChk.checked = animated && S.gifAvailable && S.overlays.anim;

  syncToolControls();
  updateTimeline();
  updateDownloadLabel();
  refreshPreview();
  loadAnalysis();
}

function setupAudioDetail(a) {
  $("#audio-stage").hidden = false;
  $(".stage-bar").hidden = true;
  $("#stage-viewport").hidden = true;
  $(".stage-foot").hidden = true;
  $("#timeline").hidden = true;

  const player = $("#detail-audio");
  player.pause();
  player.src = audioUrl(a);
  player.loop = a.audioType === "bgm";
  $("#audio-loop").checked = player.loop;
  $("#audio-detail-kind").textContent =
    `${audioTypeLabel(a)} · ${fmtDuration(a.duration)} · audio.h 实时合成`;
  $("#audio-hint").textContent = a.audioType === "bgm"
    ? "BGM 试听会循环播放；点击波形任意位置可跳转，拖动也可以。"
    : "音效为游戏内真实合成结果，点击播放即可试听。";
  $("#audio-total").textContent = fmtClock(a.duration);
  $("#audio-current").textContent = "0:00";
  $("#audio-progress").style.width = "0%";
  $("#audio-cursor").style.left = "0%";
  setAudioPlaying(false);
  S.wavePeaks = null;
  drawAudioWave(null);

  fetchWaveform(a).then((d) => {
    if (!S.detail || S.detail.id !== a.id) return;
    S.wavePeaks = d ? d.peaks : null;
    drawAudioWave(S.wavePeaks);
    if (d && d.duration) $("#audio-total").textContent = fmtClock(d.duration);
  });

  const audioText =
    `${audioTypeLabel(a)} 试听素材\n` +
    `时长: ${fmtDuration(a.duration)}\n` +
    `采样率: 44100 Hz · 单声道 · 16-bit\n` +
    `来源: v2.0.0/multiple code files/audio.h（实时合成）\n` +
    `章节: ${chapterMeta(a.chapter).label}\n` +
    `标签: ${(a.tags || []).join(" / ")}\n` +
    `波形: 由 audiopreview 导出的真实 WAV 计算 RMS 峰值`;
  S.analysisText = audioText;
  $("#detail-analysis").innerHTML = analysisHTMLFromText(audioText);
  $("#ascii-wrap").hidden = true;
  S.analysisLoaded = true;
  $("#btn-download").textContent = "下载 WAV";
}

function updateDownloadLabel() {
  const a = S.detail;
  if (!a) return;
  if (isAudio(a)) { $("#btn-download").textContent = "下载 WAV"; return; }
  const animated = (a.frames || 1) > 1 && S.overlays.anim && S.detailFrame == null && S.gifAvailable;
  $("#btn-download").textContent = animated ? "下载 GIF" : "下载 PNG";
}

function syncToolControls() {
  for (const k of ["scene", "grid", "axes", "symmetry", "bbox"]) {
    const el = $("#t-" + k);
    if (el) el.checked = !!S.overlays[k];
  }
  const anim = $("#t-anim");
  if (anim) anim.checked = !!(S.overlays.anim && S.detail && (S.detail.frames || 1) > 1 && S.gifAvailable);
  const zoom = $("#t-zoom");
  if (zoom) zoom.value = String(S.exportZoom);
}

/* ---------------- 预览刷新 ---------------- */
function refreshPreview() {
  const a = S.detail;
  if (!a || isAudio(a)) return;
  const img = $("#detail-img");
  $("#stage-loading").hidden = false;
  img.src = detailPreviewUrl();
  if (img.complete && img.naturalWidth) {
    img.classList.add("loaded");
  }
  updateTimeline();
  updateDownloadLabel();
}

function detailImageError() {
  $("#stage-loading").hidden = true;
  toast("这一帧渲染失败，试试切回静态预览或恢复默认参数。", "bad");
}

/* ---------------- 时间轴 ---------------- */
function updateTimeline() {
  const a = S.detail;
  const frames = a ? Number(a.frames) || 1 : 1;
  const show = !!a && !isAudio(a) && frames > 1;
  $("#timeline").hidden = !show;
  if (!show) return;
  const total = scrubFrames(a);
  const range = $("#frame-range");
  const scrubbing = S.detailFrame != null;
  range.max = String(total - 1);
  range.value = String(scrubbing ? S.detailFrame : 0);
  $("#btn-play").textContent = scrubbing ? "▶" : "❚❚";
  $("#btn-play").disabled = !S.gifAvailable;
  $("#frame-badge").textContent = scrubbing
    ? `逐帧 ${S.detailFrame + 1} / ${total}`
    : (S.overlays.anim ? `循环 · ${total} 帧` : `静态 · ${total} 帧`);
}

function togglePlayback() {
  const a = S.detail;
  if (!a || (a.frames || 1) <= 1) return;
  if (S.detailFrame == null) S.detailFrame = 0;
  else S.detailFrame = null;
  refreshPreview();
}

/* ---------------- 对比 ---------------- */
function updateCompareUI() {
  const on = !!S.compare;
  $("#compare-wrap").hidden = !on;
  $("#compare-divider").hidden = !on;
  $("#compare-tags").hidden = !on;
  $("#btn-compare").classList.toggle("on", on);
  $("#btn-compare").textContent = on ? "⇄ 取消对比" : "⇄ 钉住对比";
  const canvas = $("#stage-canvas");
  if (canvas) canvas.style.setProperty("--split", "50%");
  if (on && $("#compare-img").src !== S.compare.src) $("#compare-img").src = S.compare.src;
  if (on) $("#compare-tag-pin").textContent = "钉住 · " + S.compare.label;
  if (on) fitStage();
}

function compareLabel() {
  const a = S.detail;
  const st = stateLabel(a, S.detailState);
  const bits = [];
  if (st) bits.push(st);
  if (S.overlays.scene) bits.push("场景");
  return bits.length ? bits.join(" · ") : "当前状态";
}

function toggleCompare() {
  const a = S.detail;
  if (!a || isAudio(a)) return;
  if (S.compare) {
    S.compare = null;
    toast("已取消对比", "info", 1400);
  } else {
    S.compare = { src: detailPreviewUrl(), label: compareLabel() };
    toast("已钉住当前画面：改动状态 / 参数后拖动分割线对比", "ok", 3200);
  }
  updateCompareUI();
}

/* ---------------- 舞台缩放 / 平移 ---------------- */
function fitStage() {
  const img = $("#detail-img");
  const vp = $("#stage-viewport");
  const canvas = $("#stage-canvas");
  if (!img || !vp || !canvas || !img.naturalWidth || $("#stage-viewport").hidden) return;
  const pad = 34;
  const availW = Math.max(60, vp.clientWidth - pad);
  const availH = Math.max(60, vp.clientHeight - pad);
  // 小素材（如 31x17 的战机）在 800x600 里极小，允许最多 10 倍整数放大；
  // 大背景则按可视区缩小。S.zoom 是用户额外的放大倍数。
  const fit = Math.min(availW / img.naturalWidth, availH / img.naturalHeight, 10);
  const scale = fit * S.zoom;
  const w = Math.max(1, Math.round(img.naturalWidth * scale));
  const h = Math.max(1, Math.round(img.naturalHeight * scale));
  canvas.style.width = w + "px";
  canvas.style.height = h + "px";
  img.style.width = w + "px";
  img.style.height = h + "px";
  const cmp = $("#compare-img");
  if (cmp) { cmp.style.width = w + "px"; cmp.style.height = h + "px"; }
  vp.classList.toggle("pannable", S.zoom > 1 && (w > vp.clientWidth || h > vp.clientHeight));
  const zv = $("#zoom-value");
  if (zv) zv.textContent = S.zoom <= 1 ? "适配" : "额外 " + S.zoom + "×";
  $("#zoom-out").disabled = S.zoom <= 1;
  $("#zoom-in").disabled = S.zoom >= 8;
}

function setZoom(z) {
  S.zoom = Math.max(1, Math.min(8, z));
  fitStage();
}

/* ============================================================
   分析 / 源码
   ============================================================ */
function scheduleAnalysis() {
  S.analysisRequest++;
  clearTimeout(S.analysisTimer);
  S.analysisTimer = setTimeout(loadAnalysis, 200);
}

async function loadAnalysis() {
  const request = ++S.analysisRequest;
  const a = S.detail;
  if (!a || isAudio(a)) return;
  const box = $("#detail-analysis");
  try {
    const q = buildQuery(a, { state: S.detailState, params: S.detailParams, overlays: {} });
    const d = await jget("/api/analysis?" + q);
    if (request !== S.analysisRequest || !S.detail || S.detail.id !== a.id) return;
    renderAnalysis(d.text || "");
    S.analysisLoaded = true;
  } catch (e) {
    if (request === S.analysisRequest) {
      box.innerHTML = `<span class="scan-bad">分析失败：${esc(e.message)}</span>`;
    }
  }
}

function analysisHTMLFromText(text) {
  return String(text || "").split("\n").map((line) => {
    if (!line.trim()) return "";
    const i = line.indexOf(": ");
    if (i > 0) {
      return `<div class="an-row"><span class="an-k">${esc(line.slice(0, i + 1))}</span><span class="an-v">${esc(line.slice(i + 1))}</span></div>`;
    }
    return `<div class="an-row"><span class="an-v">${esc(line)}</span></div>`;
  }).join("");
}

function renderAnalysis(text) {
  const lines = String(text || "").split("\n");
  const asciiStart = lines.findIndex((l) => /\|\s*\d+\s*$/.test(l));
  const metaLines = asciiStart === -1 ? lines : lines.slice(0, asciiStart);
  const ascii = asciiStart === -1 ? "" : lines.slice(asciiStart).join("\n");
  const box = $("#detail-analysis");
  box.innerHTML = analysisHTMLFromText(metaLines.join("\n"));
  const wrap = $("#ascii-wrap");
  wrap.hidden = !ascii.trim();
  $("#detail-ascii").textContent = ascii;
  S.analysisText = text;
}

async function loadSource() {
  const a = S.detail;
  if (!a) return;
  $("#source-loc").textContent = "";
  const pre = $("#detail-source");
  pre.innerHTML = '<span class="muted">定位源码中…</span>';
  try {
    const d = await jget("/api/source?id=" + encodeURIComponent(a.id));
    if (!S.detail || S.detail.id !== a.id) return;
    pre.textContent = `// ${d.file}:${d.line}\n` + d.code;
    $("#source-loc").textContent = `${d.file.split("/").pop()}:${d.line}`;
    S.sourceLoaded = true;
    S.sourceText = `// ${d.file}:${d.line}\n` + d.code;
  } catch (e) {
    pre.innerHTML = `<span class="scan-bad">源码定位失败：${esc(e.message)}</span>`;
  }
}

function showTab(name, silent) {
  const a = S.detail;
  if (!a) return;
  if (name === "params" && isAudio(a)) name = "overview";
  S.detailTab = name;
  $$(".tab").forEach((t) => t.classList.toggle("on", t.dataset.tab === name));
  $$(".tab-pane").forEach((p) => { p.hidden = p.dataset.pane !== name; });
  if (name === "analysis" && !isAudio(a) && !S.analysisLoaded) loadAnalysis();
  if (name === "source" && !S.sourceLoaded) loadSource();
  if (!silent && name === "params") $("#tab-panes").scrollTop = 0;
}

/* ============================================================
   详情操作
   ============================================================ */
async function saveDraft() {
  const a = S.detail;
  if (!a) return;
  try {
    const d = await jpost("/api/draft/save", { draftId: a.id, params: S.detailParams, state: S.detailState });
    if (!d.ok) throw new Error("保存失败");
    await loadDrafts();
    msg("草稿参数与状态已保存，下次打开会自动恢复。", false, "good");
    toast("草稿参数已保存", "ok");
  } catch (e) { msg("保存失败：" + e.message, true); }
}

async function copyCode() {
  const a = S.detail;
  if (!a || isAudio(a)) { msg("音频素材没有可导出的绘制代码。", true); return; }
  try {
    const q = buildQuery(a, { state: S.detailState, params: S.detailParams });
    const d = await jget("/api/authored?" + q);
    try {
      await navigator.clipboard.writeText(d.code);
      msg("调好的参数已导出并复制到剪贴板。", false, "good");
      toast("导出代码已复制", "ok", 1800);
    } catch (e) {
      showTab("source");
      $("#detail-source").textContent = d.code;
      $("#source-loc").textContent = "导出的参数清单";
      msg("剪贴板不可用，已在「源码」页显示导出内容。", true);
    }
  } catch (e) { msg("导出失败：" + e.message, true); }
}

async function cloneDraft() {
  const a = S.detail;
  if (!a || isAudio(a)) { msg("音频素材不能克隆为绘制草稿。", true); return; }
  msg("正在派生草稿…");
  try {
    const d = await jpost("/api/clone", {
      id: a.id, name: a.name, state: S.detailState, params: S.detailParams,
      source: a.source || "", desc: a.desc || "",
    });
    if (d.error) throw new Error(d.error);
    await loadDrafts();
    msg(`已创建草稿：drafts/${d.dir}/ —— 改完后让 AI 写回主游戏。`, false, "good");
    toast("草稿已派生：" + d.dir, "ok", 3600);
  } catch (e) { msg("派生失败：" + e.message, true); }
}

async function downloadCurrent() {
  const a = S.detail;
  if (!a) return;
  if (isAudio(a)) {
    const link = document.createElement("a");
    link.href = audioUrl(a);
    link.download = safeName(a.name || a.id) + ".wav";
    document.body.appendChild(link);
    link.click();
    link.remove();
    toast("正在下载 WAV…", "info");
    return;
  }
  const frames = Number(a.frames) || 1;
  const animated = frames > 1 && S.overlays.anim && S.detailFrame == null && S.gifAvailable;
  const kind = animated ? "gif" : "png";
  let q = buildQuery(a, {
    state: S.detailState, params: S.detailParams, overlays: S.overlays, zoom: S.exportZoom,
  });
  const outFrames = animated ? Math.min(frames, 32) : 1;
  if (animated) q += "&frames=" + outFrames;
  window.open("/api/" + kind + "?" + q, "_blank");
  toast(animated ? "已打开 GIF（可另存）" : "已打开 PNG（可另存）", "info", 1800);
}

function safeName(s) { return String(s || "asset").replace(/[\\/:*?"<>|]/g, "_"); }

function copyText(text, okMsg) {
  if (!text) return;
  const done = () => toast(okMsg || "已复制到剪贴板", "ok", 1800);
  const legacy = () => {
    // 无 Clipboard API 权限时（http / 无安全上下文 / 自动化环境）退回 execCommand
    try {
      const ta = document.createElement("textarea");
      ta.value = text;
      ta.setAttribute("readonly", "");
      ta.style.cssText = "position:fixed;top:-200px;left:0;opacity:0";
      document.body.appendChild(ta);
      ta.select();
      const ok = document.execCommand("copy");
      ta.remove();
      if (ok) done();
      else toast("复制失败：浏览器拦截了剪贴板，请手动选择文本", "bad", 3200);
    } catch (e) {
      toast("复制失败：请手动选择文本", "bad", 3200);
    }
  };
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).then(done, legacy);
  } else legacy();
}

function msg(text, bad, kind) {
  const el = $("#action-msg");
  if (!el) return;
  el.textContent = text;
  el.className = "action-msg" + (bad ? " bad" : (kind === "good" ? " good" : ""));
}

/* ============================================================
   深链 / 历史
   ============================================================ */
function pushDetailHash(a) {
  const state = S.detailState ? "&s=" + encodeURIComponent(S.detailState) : "";
  try { history.pushState(null, "", "#a=" + encodeURIComponent(a.id) + state); } catch (e) { /* 忽略 */ }
}
function updateDetailHash() {
  if (!S.detail) return;
  const state = S.detailState ? "&s=" + encodeURIComponent(S.detailState) : "";
  try { history.replaceState(null, "", "#a=" + encodeURIComponent(S.detail.id) + state); } catch (e) { /* 忽略 */ }
}
function clearHash() {
  try { history.replaceState(null, "", location.pathname + location.search); } catch (e) { /* 忽略 */ }
}
function setHash(hash) {
  try { history.pushState(null, "", hash); } catch (e) { /* 忽略 */ }
}

function routeFromHash(initial) {
  if (S.hashSilent) return;
  const h = location.hash || "";
  if (h === "#/scan" || h === "#scan") {
    if (!S.detail) closeAllOverlays();
    showScan({ fromRoute: true });
    return;
  }
  if (h === "#/help" || h === "#help") { openHelp({ fromRoute: true }); return; }
  const m = h.match(/^#a=(.+?)(?:&s=(.*))?$/);
  if (m) {
    const id = decodeURIComponent(m[1]);
    const st = m[2] ? decodeURIComponent(m[2]) : null;
    const a = S.assets.find((x) => x.id === id);
    if (!a) { if (initial) toast("深链里的素材不存在：" + esc(id), "warn", 4000); return; }
    if (!S.detail || S.detail.id !== id) openDetail(id, { silent: true, stateFromHash: st });
    else if (st && st !== S.detailState) setDetailState(st);
    return;
  }
  // 默认态：关掉所有覆盖层
  if (S.detail) closeDetail({ silent: true });
  closeAllOverlays();
}

function closeAllOverlays() {
  $("#display-pop").hidden = true;
  if (!$("#cmd-modal").hidden) closeCmd(true);
  if (!$("#help-modal").hidden) $("#help-modal").hidden = true;
  if (!$("#scan-modal").hidden) $("#scan-modal").hidden = true;
}

/* ============================================================
   扫描弹窗 / 缓存
   ============================================================ */
async function showScan(opts = {}) {
  $("#scan-modal").hidden = false;
  if (!opts.fromRoute) setHash("#/scan");
  const body = $("#scan-body");
  body.innerHTML = '<span class="muted">正在扫描源码…</span>';
  loadCacheInfo();
  let d;
  try {
    d = await jget("/api/scan");
    checkScanBadge();
  } catch (e) {
    body.innerHTML = `<p class="scan-bad">扫描失败：${esc(e.message)}</p>
      <p class="muted">先确认工坊服务仍在运行（终端窗口），然后重试。</p>`;
    return;
  }
  const unc = d.uncovered || [];
  const byFile = {};
  unc.forEach((u) => { (byFile[u.file] = byFile[u.file] || []).push(u); });
  let html = `<div class="scan-summary">
    <div class="scan-stat"><span class="v">${d.gameDrawCount || 0}</span><span class="k">游戏绘制入口</span></div>
    <div class="scan-stat"><span class="v">${d.coveredCount || 0}</span><span class="k">已登记覆盖</span></div>
    <div class="scan-stat ${unc.length ? "warn" : "ok"}"><span class="v">${unc.length}</span><span class="k">未登记绘制</span></div>
    <div class="scan-stat ${(d.audioUncovered || []).length ? "warn" : "ok"}"><span class="v">${(d.audioUncovered || []).length}</span><span class="k">未登记音效</span></div>
  </div>`;
  if (unc.length === 0) {
    html += '<p class="scan-ok">✓ 所有绘制入口都已登记</p>';
  } else {
    html += "<p class='muted' style='font-size:12px'>下面这些入口存在于游戏里，但画廊看不到。其中不少是私有零件方法 / 屏幕级绘制，可按需登记；<b>注意未登记数 ≠ 缺少的素材数</b>。</p>";
    for (const f of Object.keys(byFile).sort()) {
      const list = byFile[f].sort((a, b) => a.line - b.line);
      html += `<div class="scan-group"><div class="scan-file">${esc(f)} <span class="file-count">${list.length} 项</span>` +
        `<span class="scan-kind">${esc(scanFileKind(f))}</span></div>`;
      list.forEach((u) => { html += `<div class="scan-item"><span class="ln">第 ${u.line} 行</span><span>${esc(u.token)}</span></div>`; });
      html += "</div>";
    }
  }
  const stale = d.stale || [];
  if (stale.length) {
    html += `<p class="scan-bad">⚠ 注册表指向了 ${stale.length} 个已不存在的入口（源码重构后需更新登记）：</p>`;
    stale.forEach((s) => { html += `<div class="scan-item"><span>${esc(s.id)} → ${esc(s.token)}</span></div>`; });
  }

  const audioUnc = d.audioUncovered || [];
  const audioStale = d.audioStale || [];
  html += `<div class="scan-section"><h3>音频覆盖</h3>`;
  html += `<p class="muted" style="font-size:12px">audio.h 中可试听的 snd* 音效 <b>${d.audioGameSfxCount || 0}</b> 个，
    已登记 <b>${d.audioRegisteredSfxCount || 0}</b> 个；BGM 试听 <b>${d.audioBgmCount || 0}</b> 条，
    <span class="${audioUnc.length ? "scan-bad" : "scan-ok"}">未登记音效 ${audioUnc.length} 个</span>。</p>`;
  if (audioUnc.length === 0) {
    html += '<p class="scan-ok">✓ 所有 snd* 音效都已能在素材画廊试听</p>';
  } else {
    html += "<p class='muted' style='font-size:12px'>下面这些音效存在于游戏里，但画廊听不到。请在 registry/audio_manifest.h 补一条：</p>";
    audioUnc.forEach((u) => { html += `<div class="scan-item"><span class="ln">第 ${u.line} 行</span><span>${esc(u.token)}</span></div>`; });
  }
  if (audioStale.length) {
    html += `<p class="scan-bad">⚠ 音频清单指向了 ${audioStale.length} 个已不存在的方法：</p>`;
    audioStale.forEach((s) => { html += `<div class="scan-item"><span>${esc(s.id)} → ${esc(s.token)}</span></div>`; });
  }
  html += "</div>";
  body.innerHTML = html;
}

async function loadCacheInfo() {
  try {
    const c = await jget("/api/cache");
    $("#cache-text").textContent = `缓存 ${c.files} 个文件 · ${fmtMB(c.bytes)} · 渲染结果按参数内容寻址`;
  } catch (e) { $("#cache-text").textContent = "缓存信息不可用"; }
}

async function clearCache() {
  if (!confirm("清理所有渲染 / 音频缓存？\n（不会删除素材与草稿，下次预览会重新渲染）")) return;
  const btn = $("#btn-cache-clear");
  btn.disabled = true;
  try {
    const d = await jpost("/api/cache/clear", {});
    toast(`已清理 ${d.removed || 0} 个缓存文件（${fmtMB(d.bytes)}）`, "ok", 3200);
    await Promise.allSettled([loadCacheInfo(), loadHealth()]);
  } catch (e) {
    toast("清理失败：" + esc(e.message), "bad");
  } finally { btn.disabled = false; }
}

/* ============================================================
   命令面板
   ============================================================ */
const CMD_ACTIONS = [
  { id: "scan", icon: "⌖", label: "未登记检查", hint: "扫描源码里漏登记的绘制 / 音频入口", run: () => showScan() },
  { id: "refresh", icon: "⟳", label: "重新扫描素材", hint: "让 C++ 渲染器重新输出素材清单", run: () => { loadAssets(true); toast("正在重新扫描素材注册表…", "info"); } },
  { id: "cache", icon: "⌫", label: "清理渲染缓存", hint: "释放磁盘空间；下次预览重新渲染", run: () => { closeCmd(true); clearCache(); } },
  { id: "export", icon: "⤓", label: "导出当前清单", hint: "把筛选结果导出为 JSON", run: () => { closeCmd(true); exportList(); } },
  { id: "fav", icon: "★", label: "只看收藏", hint: "切换收藏筛选", run: () => { closeCmd(true); S.filters.fav = !S.filters.fav; renderFacets(); renderGrid(); } },
  { id: "clear", icon: "✕", label: "清空筛选", hint: "回到全部素材", run: () => { closeCmd(true); clearFilters(); } },
  { id: "grid", icon: "▦", label: "网格视图", hint: "G", run: () => { closeCmd(true); setView("grid"); } },
  { id: "list", icon: "☰", label: "列表视图", hint: "L", run: () => { closeCmd(true); setView("list"); } },
  { id: "random", icon: "◇", label: "随机打开一个素材", hint: "骰子模式", run: () => { closeCmd(true); const a = S.assets[Math.floor(Math.random() * S.assets.length)]; if (a) openDetail(a.id); } },
  { id: "help", icon: "?", label: "快捷键帮助", hint: "?", run: () => { closeCmd(true); openHelp(); } },
  { id: "animthumb", icon: "▶", label: (S.animThumbs ? "关闭" : "开启") + "缩略图动画", hint: "关闭可让首屏更快", run: () => { closeCmd(true); toggleAnimThumbs(!S.animThumbs); } },
];

function openCmd() {
  const modal = $("#cmd-modal");
  if (!modal.hidden) return;
  closeAllOverlays();
  S.lastFocus = document.activeElement;
  $("#cmd-input").value = "";
  modal.hidden = false;
  renderCmdList("");
  setTimeout(() => $("#cmd-input").focus(), 20);
}
function closeCmd(restoreFocus) {
  $("#cmd-modal").hidden = true;
  if (restoreFocus && S.lastFocus && document.contains(S.lastFocus)) {
    try { S.lastFocus.focus({ preventScroll: true }); } catch (e) { /* 忽略 */ }
  }
}

function cmdMatches(query) {
  const q = query.trim().toLowerCase();
  const items = [];
  const actions = CMD_ACTIONS.filter((c) => !q || (c.label + " " + c.hint).toLowerCase().includes(q));
  if (actions.length) items.push({ group: "命令", actions: actions.map((c) => ({ ...c, type: "action" })) });
  if (!q) {
    if (S.recents.length) {
      const recent = S.recents.map((id) => S.assets.find((a) => a.id === id)).filter(Boolean).slice(0, 5);
      if (recent.length) items.push({ group: "最近浏览", actions: recent.map((a) => ({ type: "asset", asset: a, label: a.name, hint: a.id, icon: categoryMeta(a.category).icon })) });
    }
    const favs = Array.from(S.favorites).map((id) => S.assets.find((a) => a.id === id)).filter(Boolean).slice(0, 5);
    if (favs.length) items.push({ group: "收藏", actions: favs.map((a) => ({ type: "asset", asset: a, label: a.name, hint: a.id, icon: "★" })) });
    return items;
  }
  const hits = [];
  S.assets.forEach((a) => {
    const hay = [a.name, a.id, (a.tags || []).join(" "), a.desc].join(" ").toLowerCase();
    if (!hay.includes(q)) return;
    let score = a.name.toLowerCase().includes(q) ? 0 : 1;
    if (a.name.toLowerCase().startsWith(q)) score -= 0.5;
    hits.push({ score, a });
  });
  hits.sort((x, y) => x.score - y.score || x.a.name.localeCompare(y.a.name, "zh-Hans-CN"));
  if (hits.length) {
    items.push({ group: "素材", actions: hits.slice(0, 24).map((h) => ({
      type: "asset", asset: h.a, label: h.a.name, hint: h.a.id + " · " + categoryMeta(h.a.category).label,
      icon: isAudio(h.a) ? "♫" : categoryMeta(h.a.category).icon,
    })) });
  }
  return items;
}

function highlightMatch(text, q) {
  if (!q) return esc(text);
  const i = text.toLowerCase().indexOf(q.toLowerCase());
  if (i === -1) return esc(text);
  return esc(text.slice(0, i)) + "<mark>" + esc(text.slice(i, i + q.length)) + "</mark>" + esc(text.slice(i + q.length));
}

function renderCmdList(query) {
  const groups = cmdMatches(query);
  const flat = [];
  const html = groups.map((g) => {
    const rows = g.actions.map((it) => {
      const idx = flat.length;
      flat.push(it);
      return `<div class="cmd-item ${idx === S.cmdIndex ? "sel" : ""}" data-cmd-index="${idx}" role="option">
        <span class="cmd-ico">${it.icon || "•"}</span>
        <span class="cmd-main"><span class="cmd-name">${highlightMatch(it.label, query.trim())}</span>
        <span class="cmd-hint">${esc(it.hint || "")}</span></span>
      </div>`;
    }).join("");
    return `<div class="cmd-group">${esc(g.group)}</div>${rows}`;
  }).join("");
  S.cmdItems = flat;
  S.cmdIndex = Math.max(0, Math.min(S.cmdIndex, flat.length - 1));
  $("#cmd-list").innerHTML = html || '<div class="cmd-empty">没有匹配的素材或命令</div>';
  $("#cmd-count").textContent = flat.length ? `${flat.length} 项` : "";
  $$("#cmd-list .cmd-item").forEach((el) => {
    el.onclick = () => runCmdItem(Number(el.dataset.cmdIndex));
    el.onmouseenter = () => { S.cmdIndex = Number(el.dataset.cmdIndex); paintCmdSelection(); };
  });
  paintCmdSelection();
}

function paintCmdSelection() {
  $$("#cmd-list .cmd-item").forEach((el) => el.classList.toggle("sel", Number(el.dataset.cmdIndex) === S.cmdIndex));
  const sel = $("#cmd-list .cmd-item.sel");
  if (sel) sel.scrollIntoView({ block: "nearest" });
}

function runCmdItem(i) {
  const it = S.cmdItems[i];
  if (!it) return;
  if (it.type === "asset") { closeCmd(true); openDetail(it.asset.id); return; }
  it.run();
}

function cmdKeydown(e) {
  if (e.key === "ArrowDown" || e.key === "ArrowUp") {
    e.preventDefault();
    if (!S.cmdItems.length) return;
    S.cmdIndex = (S.cmdIndex + (e.key === "ArrowDown" ? 1 : -1) + S.cmdItems.length) % S.cmdItems.length;
    paintCmdSelection();
  } else if (e.key === "Enter") {
    e.preventDefault();
    runCmdItem(S.cmdIndex);
  }
}

/* ============================================================
   帮助 / 显示设置
   ============================================================ */
const HELP_ROWS = [
  [["⌘", "K"], "命令面板（搜索素材 / 执行命令）"],
  [["/"], "聚焦搜索框"],
  [["G"], "网格视图"],
  [["L"], "列表视图"],
  [["←"], "详情里看上一个素材"],
  [["→"], "详情里看下一个素材"],
  [["F"], "收藏 / 取消收藏当前素材"],
  [["A"], "动画开关"],
  [["S"], "真实场景开关"],
  [["C"], "钉住对比"],
  [["1", "…", "9"], "切换素材状态"],
  [["Esc"], "关闭最上层弹层"],
  [["?"], "这张快捷键表"],
];
function renderHelp() {
  $("#help-grid").innerHTML = HELP_ROWS.map(([keys, desc]) =>
    `<div class="help-row"><span>${esc(desc)}</span><span class="help-keys">${keys.map((k) => `<kbd>${esc(k)}</kbd>`).join("")}</span></div>`).join("");
}
function openHelp(opts = {}) {
  const modal = $("#help-modal");
  if (!modal.hidden) return;
  S.lastFocus = document.activeElement;
  modal.hidden = false;
  if (!opts.fromRoute) setHash("#/help");
  setTimeout(() => $("#help-close").focus(), 20);
}
function closeHelp() {
  $("#help-modal").hidden = true;
  if (location.hash === "#/help") clearHash();
}

function renderDisplayControls() {
  $("#palette-swatches").innerHTML = PALETTES.map((p) =>
    `<button class="swatch ${S.palette === p.value ? "on" : ""}" data-palette="${esc(p.value)}" role="radio" aria-checked="${S.palette === p.value}" title="${esc(p.value)}">
      <i style="background:${p.color}"></i>${esc(p.label)}</button>`).join("");
  $("#bg-swatches").innerHTML = BGS.map((b) =>
    `<button class="swatch ${S.bg === b.value ? "on" : ""}" data-bg="${esc(b.value)}" role="radio" aria-checked="${S.bg === b.value}" title="背景 #${b.value}">
      <i style="background:#${b.color.replace("#", "")}"></i>${esc(b.label)}</button>`).join("");
  $("#opt-anim-thumbs").checked = !!S.animThumbs;
  $$("#palette-swatches [data-palette]").forEach((el) => {
    el.onclick = () => { S.palette = el.dataset.palette; LS.set("palette", S.palette); renderDisplayControls(); renderGrid({ scrollTop: false }); if (S.detail) refreshPreview(); };
  });
  $$("#bg-swatches [data-bg]").forEach((el) => {
    el.onclick = () => {
      S.bg = el.dataset.bg; LS.set("bg", S.bg);
      renderDisplayControls(); applyStageBg();
      renderGrid({ scrollTop: false });
      if (S.detail) refreshPreview();
    };
  });
}
function syncDisplayControls() {
  renderDisplayControls();
  $("#opt-anim-thumbs").checked = !!S.animThumbs;
  applyStageBg();
}

function applyStageBg() {
  const stage = $("#stage-viewport");
  if (stage) stage.style.setProperty("--stage-bg", "#" + S.bg);
}

function toggleAnimThumbs(on) {
  S.animThumbs = !!on;
  LS.set("animThumbs", S.animThumbs);
  $("#opt-anim-thumbs").checked = S.animThumbs;
  renderGrid({ scrollTop: false });
  toast(S.animThumbs ? "缩略图动画已开启" : "缩略图动画已关闭（首屏更快）", "info", 1800);
}

/* ============================================================
   视图 / 排序 / 导出
   ============================================================ */
function setView(view) {
  S.view = view === "list" ? "list" : "grid";
  LS.set("view", S.view);
  $$("#view-toggle .seg-btn").forEach((b) => b.classList.toggle("on", b.dataset.view === S.view));
  renderGrid({ scrollTop: false });
}

function exportList() {
  const list = visibleAssets();
  const data = {
    generatedAt: new Date().toISOString(),
    studio: "Asset Studio 0.2.4",
    filters: { ...S.filters },
    count: list.length,
    assets: list.map((a) => ({
      id: a.id, name: a.name, chapter: a.chapter, category: a.category,
      media: a.media || "image", audioType: a.audioType || "", duration: a.duration || 0,
      frames: a.frames || 1, scene: a.scene || "",
      states: (a.states || []).map((s) => s.key),
      params: (a.params || []).map((p) => p.key),
      tags: a.tags || [], source: a.source || "",
    })),
  };
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  const stamp = new Date().toISOString().slice(0, 10).replace(/-/g, "");
  link.href = url;
  link.download = `asset-list-${stamp}.json`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
  toast(`已导出 ${list.length} 个素材的清单`, "ok", 2400);
}

/* ============================================================
   音频波形画布
   ============================================================ */
function drawAudioWave(peaks) {
  const canvas = $("#wave-canvas");
  const wrap = $("#audio-wave-wrap");
  if (!canvas || !wrap) return;
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const w = Math.max(120, wrap.clientWidth);
  const h = 132;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);
  canvas.style.width = w + "px";
  canvas.style.height = h + "px";
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);

  // 中线
  ctx.strokeStyle = "rgba(111,211,255,.12)";
  ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();

  const data = peaks && peaks.length ? peaks : seedBars("ambient", 96);
  const n = Math.max(48, Math.min(160, Math.floor(w / 4)));
  const vals = barsFromPeaks(data, n) || seedBars("ambient", n);
  const barW = Math.max(1.4, w / n * 0.62);
  const gap = w / n;
  const grad = ctx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0, "rgba(142,231,255,.95)");
  grad.addColorStop(0.5, "rgba(111,211,255,.75)");
  grad.addColorStop(1, "rgba(43,125,160,.5)");
  for (let i = 0; i < n; i++) {
    const v = vals[i];
    const bh = Math.max(2, v * (h - 14));
    const x = i * gap + (gap - barW) / 2;
    const y = (h - bh) / 2;
    ctx.fillStyle = grad;
    ctx.globalAlpha = 0.55 + v * 0.45;
    if (ctx.roundRect) { ctx.beginPath(); ctx.roundRect(x, y, barW, bh, 2); ctx.fill(); }
    else ctx.fillRect(x, y, barW, bh);
  }
  ctx.globalAlpha = 1;
}

function updateAudioProgressUI() {
  const player = $("#detail-audio");
  if (!player || !S.detail || !isAudio(S.detail)) return;
  const dur = player.duration || S.detail.duration || 0;
  const cur = player.currentTime || 0;
  const pct = dur > 0 ? Math.min(1, cur / dur) : 0;
  const wrap = $("#audio-wave-wrap");
  if (!wrap) return;
  const wpx = Math.max(120, wrap.clientWidth);
  $("#audio-progress").style.width = (pct * wpx) + "px";
  $("#audio-cursor").style.left = (pct * wpx) + "px";
  $("#audio-current").textContent = fmtClock(cur);
}

function setAudioPlaying(on) {
  const orb = $("#audio-orb");
  const btn = $("#audio-play");
  if (btn) btn.textContent = on ? "❚❚" : "▶";
  if (orb) orb.classList.toggle("playing", !!on);
}

function audioToggle() {
  const player = $("#detail-audio");
  if (!player || !isAudio(S.detail)) return;
  if (player.paused) player.play().catch(() => toast("播放失败：音频还在合成，稍等一下", "warn"));
  else player.pause();
}

function seekAudioFromEvent(e) {
  const wrap = $("#audio-wave-wrap");
  const player = $("#detail-audio");
  if (!wrap || !player) return;
  const rect = wrap.getBoundingClientRect();
  const pct = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
  const dur = player.duration || S.detail.duration || 0;
  if (dur > 0) {
    player.currentTime = pct * dur;
    updateAudioProgressUI();
  }
}

/* ============================================================
   事件绑定
   ============================================================ */
function bind() {
  renderHelp();
  renderDisplayControls();
  applyStageBg();
  showSkeleton(15);

  // ---- 顶栏 ----
  $("#search").addEventListener("input", (e) => {
    clearTimeout(S.searchTimer);
    const v = e.target.value;
    S.searchTimer = setTimeout(() => { S.filters.q = v; renderGrid(); }, 130);
  });
  $("#btn-refresh").onclick = () => {
    $("#btn-refresh").classList.add("spin");
    loadAssets(true).finally(() => setTimeout(() => $("#btn-refresh").classList.remove("spin"), 500));
    toast("正在重新扫描素材注册表…", "info");
  };
  $("#btn-scan").onclick = () => showScan();
  $("#btn-help").onclick = () => openHelp();
  $("#btn-display").onclick = () => { $("#display-pop").hidden = !$("#display-pop").hidden; };
  $("#display-close").onclick = () => { $("#display-pop").hidden = true; };
  $("#opt-anim-thumbs").onchange = (e) => toggleAnimThumbs(e.target.checked);
  $("#btn-filters").onclick = () => {
    const sb = $("#sidebar");
    sb.classList.toggle("open");
    $("#sidebar-scrim").hidden = !sb.classList.contains("open");
  };
  $("#sidebar-scrim").onclick = () => { $("#sidebar").classList.remove("open"); $("#sidebar-scrim").hidden = true; };
  $("#btn-clear-filters").onclick = clearFilters;
  $("#btn-empty-reset").onclick = clearFilters;
  $("#btn-clear-recent").onclick = () => { S.recents = []; LS.set("recents", []); renderRecents(); };

  // ---- 工具栏 ----
  $("#sort").value = S.sort;
  $("#sort").onchange = (e) => { S.sort = e.target.value; LS.set("sort", S.sort); renderGrid({ scrollTop: false }); };
  $("#density").value = S.density;
  $("#density").onchange = (e) => { S.density = e.target.value; LS.set("density", S.density); renderGrid({ scrollTop: false }); };
  $$("#view-toggle .seg-btn").forEach((b) => {
    b.classList.toggle("on", b.dataset.view === S.view);
    b.onclick = () => setView(b.dataset.view);
  });
  $("#btn-export-list").onclick = exportList;

  // ---- 详情 ----
  $("#detail-close").onclick = () => closeDetail();
  $("#detail").addEventListener("click", (e) => {
    if (e.target.id === "detail" || e.target.classList.contains("modal-backdrop")) closeDetail();
  });
  $("#detail-prev").onclick = () => detailStep(-1);
  $("#detail-next").onclick = () => detailStep(1);
  $("#btn-fav").onclick = () => { if (S.detail) toggleFavorite(S.detail.id); };
  $("#btn-copy-id").onclick = () => { if (S.detail) copyText(S.detail.id, "素材 ID 已复制"); };
  $("#btn-share").onclick = () => {
    if (!S.detail) return;
    updateDetailHash();
    copyText(location.href, "分享深链已复制：打开即定位到该素材与状态");
  };

  $$(".tab").forEach((t) => { t.onclick = () => showTab(t.dataset.tab); });
  $("#btn-reset-params").onclick = () => {
    if (!S.detail) return;
    S.detailParams = defaultParamsFor(S.detail);
    renderDetailShell(S.detail);
    showTab(S.detailTab, true);
    refreshPreview();
    scheduleAnalysis();
    toast("参数已恢复默认", "info", 1600);
  };
  $("#btn-download").onclick = downloadCurrent;
  $("#btn-copy-code").onclick = copyCode;
  $("#btn-clone").onclick = cloneDraft;
  $("#btn-save-draft").onclick = saveDraft;
  $("#btn-source").onclick = () => { showTab("source"); loadSource(); };
  $("#btn-source-reload").onclick = () => { S.sourceLoaded = false; loadSource(); };
  $("#btn-source-copy").onclick = () => copyText(S.sourceText || $("#detail-source").textContent, "源码已复制");
  $("#btn-copy-analysis").onclick = () => copyText(S.analysisText || $("#detail-analysis").textContent, "分析结果已复制");
  $("#btn-open-raw").onclick = () => { if (S.detail && !isAudio(S.detail)) window.open(detailPreviewUrl(), "_blank"); };

  // 舞台工具
  $("#zoom-in").onclick = () => setZoom(S.zoom + 1);
  $("#zoom-out").onclick = () => setZoom(S.zoom - 1);
  $("#zoom-fit").onclick = () => setZoom(1);
  $("#btn-compare").onclick = toggleCompare;

  for (const k of ["scene", "grid", "axes", "symmetry", "bbox"]) {
    const el = $("#t-" + k);
    if (el) el.onchange = (e) => { S.overlays[k] = e.target.checked; refreshPreview(); };
  }
  $("#t-anim").onchange = (e) => {
    S.overlays.anim = e.target.checked;
    S.detailFrame = e.target.checked ? null : 0;
    refreshPreview();
    updateTimeline();
  };
  $("#t-zoom").onchange = (e) => { S.exportZoom = parseInt(e.target.value, 10) || 1; updateDownloadLabel(); };
  $("#btn-play").onclick = togglePlayback;
  $("#frame-range").oninput = (e) => {
    S.detailFrame = parseInt(e.target.value, 10);
    refreshPreview();
    updateTimeline();
  };

  // 详情图片事件
  const img = $("#detail-img");
  img.addEventListener("load", () => {
    img.classList.add("loaded");
    $("#stage-loading").hidden = true;
    $("#stage-empty").hidden = true;
    fitStage();
  });
  img.addEventListener("error", () => {
    $("#stage-loading").hidden = true;
    $("#stage-empty").hidden = false;
  });

  // 舞台平移
  const vp = $("#stage-viewport");
  vp.addEventListener("pointerdown", (e) => {
    const divider = e.target.closest("#compare-divider");
    if (divider) {
      S.compareDrag = true;
      divider.setPointerCapture && divider.setPointerCapture(e.pointerId);
      e.preventDefault();
      return;
    }
    if (!vp.classList.contains("pannable")) return;
    S.panDrag = { x: e.clientX, y: e.clientY, sl: vp.scrollLeft, st: vp.scrollTop };
    vp.classList.add("panning");
    vp.setPointerCapture && vp.setPointerCapture(e.pointerId);
  });
  vp.addEventListener("pointermove", (e) => {
    if (S.compareDrag) {
      const canvas = $("#stage-canvas");
      const rect = canvas.getBoundingClientRect();
      const pct = Math.max(2, Math.min(98, ((e.clientX - rect.left) / rect.width) * 100));
      canvas.style.setProperty("--split", pct + "%");
      return;
    }
    if (!S.panDrag) return;
    vp.scrollLeft = S.panDrag.sl - (e.clientX - S.panDrag.x);
    vp.scrollTop = S.panDrag.st - (e.clientY - S.panDrag.y);
  });
  const endDrag = () => {
    if (S.compareDrag) S.compareDrag = false;
    if (S.panDrag) { S.panDrag = null; vp.classList.remove("panning"); }
  };
  vp.addEventListener("pointerup", endDrag);
  vp.addEventListener("pointercancel", endDrag);
  window.addEventListener("pointerup", endDrag);
  window.addEventListener("resize", () => {
    refitCards();
    fitStage();
    if (S.detail && isAudio(S.detail)) drawAudioWave(S.wavePeaks);
  });

  // 音频
  $("#audio-play").onclick = audioToggle;
  $("#audio-restart").onclick = () => {
    const p = $("#detail-audio");
    if (!p) return;
    p.currentTime = 0;
    p.play().catch(() => {});
  };
  $("#audio-loop").onchange = (e) => { const p = $("#detail-audio"); if (p) p.loop = e.target.checked; };
  const player = $("#detail-audio");
  player.addEventListener("play", () => setAudioPlaying(true));
  player.addEventListener("pause", () => setAudioPlaying(false));
  player.addEventListener("ended", () => setAudioPlaying(false));
  player.addEventListener("timeupdate", updateAudioProgressUI);
  player.addEventListener("loadedmetadata", () => {
    if (S.detail && isAudio(S.detail)) {
      $("#audio-total").textContent = fmtClock(player.duration || S.detail.duration);
      updateAudioProgressUI();
    }
  });
  const waveWrap = $("#audio-wave-wrap");
  waveWrap.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    seekAudioFromEvent(e);
    const move = (ev) => seekAudioFromEvent(ev);
    const up = () => { window.removeEventListener("pointermove", move); window.removeEventListener("pointerup", up); };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  });

  // ---- 命令面板 ----
  $("#cmd-input").addEventListener("input", (e) => {
    S.cmdIndex = 0;
    renderCmdList(e.target.value);
  });
  $("#cmd-input").addEventListener("keydown", cmdKeydown);
  $("#cmd-modal").addEventListener("click", (e) => {
    if (e.target.id === "cmd-modal" || e.target.classList.contains("modal-backdrop")) closeCmd(true);
  });

  // ---- 帮助 ----
  $("#help-close").onclick = closeHelp;
  $("#help-modal").addEventListener("click", (e) => {
    if (e.target.id === "help-modal" || e.target.classList.contains("modal-backdrop")) closeHelp();
  });

  // ---- 扫描 ----
  $("#scan-close").onclick = () => { $("#scan-modal").hidden = true; if (location.hash === "#/scan") clearHash(); };
  $("#scan-modal").addEventListener("click", (e) => {
    if (e.target.id === "scan-modal" || e.target.classList.contains("modal-backdrop")) {
      $("#scan-modal").hidden = true;
      if (location.hash === "#/scan") clearHash();
    }
  });
  $("#btn-cache-clear").onclick = clearCache;

  // ---- 显示设置：点外部关闭 ----
  document.addEventListener("click", (e) => {
    const pop = $("#display-pop");
    if (pop.hidden) return;
    if (pop.contains(e.target) || e.target.closest("#btn-display")) return;
    pop.hidden = true;
  });

  // ---- 全局键盘 ----
  document.addEventListener("keydown", onGlobalKey);

  // ---- 历史 ----
  window.addEventListener("popstate", () => routeFromHash(false));
  window.addEventListener("hashchange", () => routeFromHash(false));
}

function trapModalFocus(e) {
  if (e.key !== "Tab") return false;
  const open = ["#cmd-modal", "#help-modal", "#scan-modal", "#detail"].find((sel) => !$(sel).hidden);
  if (!open) return false;
  const root = $(open);
  const focusables = $$("button, [href], input, select, textarea, [tabindex]:not([tabindex='-1'])", root)
    .filter((el) => el.offsetParent !== null && !el.disabled);
  if (!focusables.length) return false;
  const first = focusables[0], last = focusables[focusables.length - 1];
  if (!root.contains(document.activeElement)) { e.preventDefault(); first.focus(); return true; }
  if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); return true; }
  if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); return true; }
  return false;
}

function onGlobalKey(e) {
  const inField = e.target && (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA" || e.target.tagName === "SELECT" || e.target.isContentEditable);
  const cmdOpen = !$("#cmd-modal").hidden;
  if (trapModalFocus(e)) return;

  if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === "k") {
    e.preventDefault();
    if (cmdOpen) closeCmd(true); else openCmd();
    return;
  }
  if (e.key === "Escape") {
    if (!$("#display-pop").hidden) { $("#display-pop").hidden = true; return; }
    if (cmdOpen) { closeCmd(true); return; }
    if (!$("#help-modal").hidden) { closeHelp(); return; }
    if (!$("#scan-modal").hidden) { $("#scan-modal").hidden = true; if (location.hash === "#/scan") clearHash(); return; }
    if (S.detail) { closeDetail(); return; }
    return;
  }
  if (cmdOpen || inField) return;

  if (e.key === "/") {
    e.preventDefault();
    $("#search").focus();
    return;
  }
  if (e.key === "?") { e.preventDefault(); openHelp(); return; }
  if (e.key.toLowerCase() === "g") { setView("grid"); return; }
  if (e.key.toLowerCase() === "l") { setView("list"); return; }

  if (!S.detail) return;
  if (e.key === "ArrowLeft") { e.preventDefault(); detailStep(-1); return; }
  if (e.key === "ArrowRight") { e.preventDefault(); detailStep(1); return; }
  const k = e.key.toLowerCase();
  if (k === "f") { toggleFavorite(S.detail.id); return; }
  if (k === "a" && !$("#t-anim").disabled) {
    $("#t-anim").checked = !$("#t-anim").checked;
    S.overlays.anim = $("#t-anim").checked;
    S.detailFrame = S.overlays.anim ? null : 0;
    refreshPreview(); updateTimeline();
    return;
  }
  if (k === "s" && !$("#t-scene").disabled) {
    $("#t-scene").checked = !$("#t-scene").checked;
    S.overlays.scene = $("#t-scene").checked;
    refreshPreview();
    return;
  }
  if (k === "c") { toggleCompare(); return; }
  if (/^[1-9]$/.test(e.key)) {
    const st = (S.detail.states || [])[parseInt(e.key, 10) - 1];
    if (st) setDetailState(st.key);
  }
}

/* ============================================================
   启动
   ============================================================ */
window.__assetStudio = {
  S,
  openDetail,
  closeDetail,
  showScan,
  openCmd,
  openHelp,
  toast,
  refreshPreview,
  renderGrid,
  visibleAssets,
  toggleFavorite,
  setDetailState,
};

bind();
loadAssets();
