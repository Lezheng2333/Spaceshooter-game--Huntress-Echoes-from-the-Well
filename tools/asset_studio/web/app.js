/* ============================================================
   Asset Studio 素材工坊 — 前端
   ============================================================ */

const $  = (s) => document.querySelector(s);
const $$ = (s) => Array.from(document.querySelectorAll(s));

const S = {
  gifAvailable: true,
  analysisRequest: 0,
  analysisTimer: null,
  assets: [],
  drafts: [],
  filters: { chapter: null, category: null, tag: null, q: "" },
  detail: null,          // 当前打开的素材
  detailState: "",
  detailParams: {},
  overlays: { scene: false, grid: false, axes: false, symmetry: false, bbox: false, anim: true, zoom: 1 },
};

const CHAPTER_LABEL = { ch1: "第一章", ch2: "第二章", ch3: "第三章", shared: "共享", common: "共享" };
const CATEGORY_LABEL = {
  player: "玩家战机", enemy: "普通敌人", boss: "Boss", bullet: "子弹弹幕",
  fx: "特效 / 技能", hud: "HUD / UI", bg: "背景 / 场景", misc: "其他",
};

/* ---------------- 工具 ---------------- */
function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"']/g, c =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function globalOpts() {
  return { palette: $("#palette").value, bg: $("#bg").value };
}

function buildQuery(asset, { state, params, overlays, gif } = {}) {
  const q = new URLSearchParams();
  q.set("id", asset.id);
  const st = state != null ? state : (asset.states[0] ? asset.states[0].key : "");
  if (st) q.set("state", st);
  if (params) for (const k in params) q.set("param." + k, params[k]);
  const ov = overlays || {};
  for (const f of ["grid", "axes", "symmetry", "bbox"]) if (ov[f]) q.set(f, "1");
  if (ov.scene) { q.set("scene", "1"); q.set("scenedim", "130"); }
  if (ov.zoom && ov.zoom > 1) q.set("zoom", ov.zoom);
  const g = globalOpts();
  q.set("palette", g.palette);
  q.set("bg", g.bg);
  return q.toString();
}

function cardImageUrl(asset) {
  const animated = (asset.frames || 1) > 1 && S.gifAvailable;
  const kind = animated ? "gif" : "png";
  let q = buildQuery(asset, { overlays: {} });
  if (animated) q = q2set(q, "frames", Math.min(asset.frames, 16));
  return `/api/${kind}?${q}`;
}
function q2set(qs, k, v) { return qs + "&" + k + "=" + encodeURIComponent(v); }

/* ---------------- 加载 ---------------- */
async function loadAssets(force) {
  $("#status").innerHTML = "正在读取素材注册表…";
  try {
    const r = await fetch("/api/assets" + (force ? "?force=1" : ""));
    const data = await r.json();
    if (data.error) throw new Error(data.error);
    S.gifAvailable = data.gifAvailable !== false;
    S.assets = data.assets || [];
    renderFacets();
    renderGrid();
    await loadDrafts();
    checkScanBadge();
  } catch (e) {
    $("#status").innerHTML = `<span class="hint">读取失败：${esc(e.message)}</span>`;
  }
}

async function loadDrafts() {
  try {
    const r = await fetch("/api/drafts");
    const d = await r.json();
    S.drafts = d.drafts || [];
    const box = $("#f-drafts");
    $("#drafts-section").hidden = S.drafts.length === 0;
    box.innerHTML = S.drafts.map(x =>
      `<div class="facet-item" data-draft="${esc(x.id)}" title="${esc(x.dir)}">${esc(x.title || x.id)}
         <span class="cnt">${x.status === "promoted" ? "已转正" : "草稿"}</span></div>`).join("");
    $$("[data-draft]").forEach(el => el.onclick = () => {
      if (S.assets.some(a => a.id === el.dataset.draft)) openDetail(el.dataset.draft);
      else msg("草稿代码尚未编译，请重新构建工坊后刷新素材。", true);
    });
  } catch (e) { /* 草稿目录为空时静默 */ }
}

async function checkScanBadge() {
  try {
    const r = await fetch("/api/scan");
    const d = await r.json();
    const n = (d.uncovered || []).length;
    const btn = $("#btn-scan");
    btn.textContent = n > 0 ? `未登记检查 (${n})` : "未登记检查 ✓";
    btn.classList.toggle("btn-warn", n > 0);
    btn.classList.toggle("btn-bad", n > 0);
  } catch (e) { /* 忽略 */ }
}

/* ---------------- 侧栏筛选 ---------------- */
function countBy(key) {
  const m = {};
  S.assets.forEach(a => { const v = a[key] || "?"; m[v] = (m[v] || 0) + 1; });
  return m;
}

function renderFacets() {
  const chap = countBy("chapter"), cat = countBy("category");
  $("#f-chapter").innerHTML =
    facetItem("全部章节", null, S.assets.length, S.filters.chapter === null, "chapter") +
    Object.keys(chap).sort().map(k =>
      facetItem(CHAPTER_LABEL[k] || k, k, chap[k], S.filters.chapter === k, "chapter")).join("");

  $("#f-category").innerHTML =
    facetItem("全部类型", null, S.assets.length, S.filters.category === null, "category") +
    Object.keys(cat).sort().map(k =>
      facetItem(CATEGORY_LABEL[k] || k, k, cat[k], S.filters.category === k, "category")).join("");

  const tags = {};
  S.assets.forEach(a => (a.tags || []).forEach(t => tags[t] = (tags[t] || 0) + 1));
  $("#f-tags").innerHTML = Object.keys(tags).sort().map(t =>
    `<div class="facet-item ${S.filters.tag === t ? "on" : ""}" data-facet="tag" data-val="${esc(t)}">${esc(t)}</div>`).join("")
    || '<span class="dim" style="font-size:12px">暂无标签</span>';

  $$("[data-facet]").forEach(el => el.onclick = () => {
    const f = el.dataset.facet, v = el.dataset.val;
    S.filters[f] = (v === "" || S.filters[f] === v) ? null : v;
    renderFacets(); renderGrid();
  });
}

function facetItem(label, val, count, on, facet) {
  return `<div class="facet-item ${on ? "on" : ""}" data-facet="${facet}" data-val="${esc(val == null ? "" : val)}">
      <span>${esc(label)}</span><span class="cnt">${count}</span></div>`;
}

/* ---------------- 缩略图墙 ---------------- */
function visibleAssets() {
  const f = S.filters, q = f.q.trim().toLowerCase();
  return S.assets.filter(a => {
    if (f.chapter && a.chapter !== f.chapter) return false;
    if (f.category && a.category !== f.category) return false;
    if (f.tag && !(a.tags || []).includes(f.tag)) return false;
    if (q) {
      const hay = [a.name, a.id, a.desc, (a.tags || []).join(" "), a.category, a.chapter]
        .join(" ").toLowerCase();
      if (!hay.includes(q)) return false;
    }
    return true;
  });
}

function renderGrid() {
  const list = visibleAssets();
  const total = S.assets.length;
  $("#status").innerHTML = total === 0
    ? '<span class="hint">注册表是空的</span>'
    : `共 <b>${total}</b> 个已登记素材${list.length !== total ? `，筛选中 <b>${list.length}</b> 个` : ""}`;

  $("#empty").hidden = list.length > 0;
  $("#grid").innerHTML = list.map(a => {
    const animated = (a.frames || 1) > 1;
    return `<div class="card" data-id="${esc(a.id)}">
      <div class="card-thumb">
        <img loading="lazy" src="${esc(cardImageUrl(a))}" alt="${esc(a.name)}">
        ${animated ? `<span class="badge">▶ ${a.frames}帧</span>` : ""}
        ${a.scene ? `<span class="badge" style="right:auto;left:7px">可叠场景</span>` : ""}
      </div>
      <div class="card-body">
        <div class="card-name">${esc(a.name)}</div>
        <div class="card-id">${esc(a.id)}</div>
        <div class="card-tags">${(a.tags || []).slice(0, 4).map(t => `<span class="tag">${esc(t)}</span>`).join("")}</div>
      </div>
    </div>`;
  }).join("");

  $$(".card").forEach(el => el.onclick = () => openDetail(el.dataset.id));
}

/* ---------------- 详情 ---------------- */
function openDetail(id, preserve = false) {
  const a = S.assets.find(x => x.id === id);
  if (!a) return;
  S.detail = a;
  if (!preserve) {
    const draft = S.drafts.find(d => d.id === a.id);
    const savedState = draft && draft.state;
    S.detailState = (a.states || []).some(s => s.key === savedState)
      ? savedState : (a.states[0] ? a.states[0].key : "");
    S.detailParams = {};
    (a.params || []).forEach(p => {
      const saved = draft && draft.tuned && draft.tuned[p.key];
      S.detailParams[p.key] = Number.isFinite(saved) ? Math.max(p.min, Math.min(p.max, saved)) : p.def;
    });
    S.overlays.anim = (a.frames || 1) > 1 && S.gifAvailable;
  }

  $("#detail-name").textContent = a.name;
  $("#detail-id").textContent = a.id + "   ·   " + (CHAPTER_LABEL[a.chapter] || a.chapter)
    + " / " + (CATEGORY_LABEL[a.category] || a.category);
  $("#detail-desc").textContent = a.desc || "";
  $("#detail-tags").innerHTML = (a.tags || []).map(t => `<span class="tag">${esc(t)}</span>`).join("");

  // 状态
  const hasStates = (a.states || []).length > 0;
  $("#detail-states-wrap").hidden = !hasStates;
  $("#detail-states").innerHTML = (a.states || []).map(s =>
    `<button class="state-btn ${s.key === S.detailState ? "on" : ""}" data-state="${esc(s.key)}">${esc(s.label)}</button>`).join("");
  $$("#detail-states .state-btn").forEach(b => b.onclick = () => {
    S.detailState = b.dataset.state; openDetail(id, true);
  });

  // 参数滑块
  const hasParams = (a.params || []).length > 0;
  $("#detail-params-wrap").hidden = !hasParams;
  $("#detail-params").innerHTML = (a.params || []).map(p => `
    <div class="param-row">
      <label title="${esc(p.key)}">${esc(p.label)}</label>
      <input type="range" data-pk="${esc(p.key)}" min="${p.min}" max="${p.max}" step="${p.step}" value="${S.detailParams[p.key]}">
      <span class="val" id="pv-${esc(p.key)}">${S.detailParams[p.key]}</span>
    </div>`).join("");
  $$("#detail-params input[type=range]").forEach(r => {
    r.oninput = () => {
      S.detailParams[r.dataset.pk] = parseFloat(r.value);
      $("#pv-" + r.dataset.pk).textContent = r.value;
      clearTimeout(r._t);
      r._t = setTimeout(() => { refreshPreview(); scheduleAnalysis(); }, 60);   // 节流，避免拖动时疯狂请求
    };
  });

  $("#source-wrap").hidden = true;
  $("#action-msg").textContent = "";
  $("#action-msg").className = "action-msg";

  // 场景开关：只有声明了场景的素材才可用
  const sceneChk = $("#t-scene");
  sceneChk.disabled = !a.scene;
  sceneChk.parentElement.style.opacity = a.scene ? "1" : "0.35";
  sceneChk.parentElement.title = a.scene ? ("场景上下文：" + a.scene) : "该素材没有声明场景上下文";
  if (!a.scene) { S.overlays.scene = false; sceneChk.checked = false; }

  // 动画开关：只有多帧素材才可用
  const animChk = $("#t-anim");
  const animated = (a.frames || 1) > 1 && S.gifAvailable;
  animChk.title = S.gifAvailable ? "" : "当前 Python 未安装 Pillow，已使用 PNG 预览";
  animChk.disabled = !animated;
  animChk.parentElement.style.opacity = animated ? "1" : "0.35";
  animChk.checked = animated && S.overlays.anim;

  $("#btn-save-draft").hidden = !a.id.startsWith("draft.");
  syncToolCheckboxes();
  refreshPreview();
  loadAnalysis();
  $("#detail").hidden = false;
  document.body.style.overflow = "hidden";
}

function closeDetail() {
  S.analysisRequest++;
  clearTimeout(S.analysisTimer);
  $("#detail").hidden = true;
  document.body.style.overflow = "";
}

function syncToolCheckboxes() {
  for (const k of ["scene", "grid", "axes", "symmetry", "bbox"]) {
    const el = $("#t-" + k);
    if (el) el.checked = !!S.overlays[k];
  }
  $("#t-zoom").value = String(S.overlays.zoom);
}

function detailImageUrl() {
  const a = S.detail;
  const animated = (a.frames || 1) > 1 && S.overlays.anim;
  const kind = animated ? "gif" : "png";
  let q = buildQuery(a, {
    state: S.detailState,
    params: S.detailParams,
    overlays: S.overlays,
  });
  if (animated) q = q2set(q, "frames", Math.min(a.frames, 32));
  return `/api/${kind}?${q}`;
}

function refreshPreview() {
  if (!S.detail) return;
  const img = $("#detail-img");
  img.src = detailImageUrl();
  $("#btn-download").textContent = (S.detail.frames > 1 && S.overlays.anim) ? "下载 GIF" : "下载 PNG";
}

function scheduleAnalysis() {
  S.analysisRequest++;
  clearTimeout(S.analysisTimer);
  S.analysisTimer = setTimeout(loadAnalysis, 180);
}

async function loadAnalysis() {
  const request = ++S.analysisRequest;
  const a = S.detail;
  const box = $("#detail-analysis");
  box.textContent = "正在分析…";
  try {
    let q = buildQuery(a, { state: S.detailState, params: S.detailParams });
    const r = await fetch("/api/analysis?" + q);
    const d = await r.json();
    if (request === S.analysisRequest) box.textContent = d.text || d.error || "（无结果）";
  } catch (e) {
    if (request === S.analysisRequest) box.textContent = "分析失败：" + e.message;
  }
}

/* ---------------- 操作 ---------------- */
async function saveDraft() {
  try {
    const r = await fetch("/api/draft/save", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ draftId: S.detail.id, params: S.detailParams, state: S.detailState }),
    });
    const d = await r.json();
    if (!r.ok || !d.ok) throw new Error(d.error || "保存失败");
    await loadDrafts();
    msg("草稿参数与状态已保存，下次打开会恢复。");
  } catch (e) { msg("保存失败：" + e.message, true); }
}

async function copyCode() {
  const a = S.detail;
  let q = buildQuery(a, { state: S.detailState, params: S.detailParams });
  const r = await fetch("/api/authored?" + q);
  const d = await r.json();
  if (d.error) return msg(d.error, true);
  try {
    await navigator.clipboard.writeText(d.code);
    msg("代码已复制到剪贴板（转正时把它发给我即可）");
  } catch (e) {
    showSource(d.code, "导出的代码");
    msg("剪贴板不可用，已在下方显示代码");
  }
}

async function showSource(code, title) {
  if (!code) {
    const a = S.detail;
    const r = await fetch("/api/source?id=" + encodeURIComponent(a.id));
    const d = await r.json();
    if (d.error) return msg(d.error, true);
    code = `// ${d.file}:${d.line}\n` + d.code;
  }
  $("#source-wrap").hidden = false;
  $("#source-wrap").querySelector("h3").childNodes[0].nodeValue = (title || "源码") + " ";
  $("#detail-source").textContent = code;
}

async function cloneDraft() {
  const a = S.detail;
  msg("正在派生草稿…");
  try {
    const r = await fetch("/api/clone", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        id: a.id, name: a.name, state: S.detailState, params: S.detailParams,
        source: a.source || "", desc: a.desc || "",
      }),
    });
    const d = await r.json();
    if (d.error) return msg(d.error, true);
    msg(`已创建草稿：drafts/${d.dir}/ —— 告诉我你想怎么改，我来写参数化的绘制代码`);
    loadDrafts();
  } catch (e) {
    msg("派生失败：" + e.message, true);
  }
}

function msg(t, bad) {
  const el = $("#action-msg");
  el.textContent = t;
  el.className = "action-msg" + (bad ? " bad" : "");
}

/* ---------------- 扫描报告 ---------------- */
async function showScan() {
  $("#scan-modal").hidden = false;
  const body = $("#scan-body");
  body.textContent = "正在扫描…";
  let d;
  try {
    const r = await fetch("/api/scan");
    d = await r.json();
  } catch (e) {
    body.innerHTML = `<p class="scan-bad">扫描失败：${esc(e.message)}</p>
      <p class="dim">先确认工坊服务还在运行（终端里那个 python3 server.py 窗口），然后重试。</p>`;
    return;
  }
  if (d.error) { body.innerHTML = `<p class="scan-bad">${esc(d.error)}</p>`; return; }
  const unc = d.uncovered || [];
  const byFile = {};
  unc.forEach(u => (byFile[u.file] = byFile[u.file] || []).push(u));

  let html = `<p>游戏源码绘制入口 <b>${d.gameDrawCount}</b> 个，
    注册表已覆盖 <b>${d.coveredCount}</b> 个，
    <span class="${unc.length ? "scan-bad" : "scan-ok"}">未登记 ${unc.length} 个</span>。</p>`;
  if (unc.length === 0) {
    html += '<p class="scan-ok">✓ 所有绘制入口都已登记</p>';
  } else {
    html += "<p class='dim'>下面这些素材存在于游戏里，但画廊看不到它们。补一条注册项即可：</p>";
    for (const f of Object.keys(byFile).sort()) {
      html += `<div class="scan-group"><div class="scan-file">${esc(f)}</div>`;
      byFile[f].sort((a, b) => a.line - b.line).forEach(u => {
        html += `<div class="scan-item">第 ${u.line} 行&nbsp;&nbsp;${esc(u.token)}</div>`;
      });
      html += "</div>";
    }
  }
  if ((d.stale || []).length) {
    html += `<p class="scan-bad">⚠ 注册表指向了 ${d.stale.length} 个已不存在的入口（源码重构后需要更新登记）：</p>`;
    d.stale.forEach(s => html += `<div class="scan-item">${esc(s.id)} → ${esc(s.token)}</div>`);
  }
  body.innerHTML = html;
}

/* ---------------- 事件绑定 ---------------- */
function bind() {
  $("#search").oninput = (e) => { S.filters.q = e.target.value; renderGrid(); };
  $("#palette").onchange = () => { renderGrid(); if (S.detail) refreshPreview(); };
  $("#bg").onchange = () => { renderGrid(); if (S.detail) refreshPreview(); };
  $("#btn-refresh").onclick = () => loadAssets(true);
  $("#btn-scan").onclick = showScan;
  $("#scan-close").onclick = () => $("#scan-modal").hidden = true;
  $("#detail-close").onclick = closeDetail;

  for (const k of ["scene", "grid", "axes", "symmetry", "bbox"]) {
    $("#t-" + k).onchange = (e) => { S.overlays[k] = e.target.checked; refreshPreview(); };
  }
  $("#t-anim").onchange = (e) => { S.overlays.anim = e.target.checked; refreshPreview(); };
  $("#t-zoom").onchange = (e) => { S.overlays.zoom = parseInt(e.target.value, 10); refreshPreview(); };

  $("#btn-reset-params").onclick = () => {
    (S.detail.params || []).forEach(p => S.detailParams[p.key] = p.def);
    openDetail(S.detail.id, true);
  };
  $("#btn-save-draft").onclick = saveDraft;
  $("#btn-copy-code").onclick = copyCode;
  $("#btn-source").onclick = () => showSource(null, "源码");
  $("#btn-source-hide").onclick = () => $("#source-wrap").hidden = true;
  $("#btn-clone").onclick = cloneDraft;
  $("#btn-download").onclick = () => window.open(detailImageUrl(), "_blank");

  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") {
      if (!$("#scan-modal").hidden) $("#scan-modal").hidden = true;
      else if (!$("#detail").hidden) closeDetail();
    }
  });
  $("#detail").onclick = (e) => { if (e.target.id === "detail") closeDetail(); };
  $("#scan-modal").onclick = (e) => { if (e.target.id === "scan-modal") $("#scan-modal").hidden = true; };
}

bind();
loadAssets();

// 支持用 #scan 直接打开「未登记检查」：既方便收藏，也让无头浏览器能端到端验证弹窗
if (location.hash === "#scan") showScan();
