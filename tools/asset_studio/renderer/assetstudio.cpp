// ============================================================
// Asset Studio — 无头素材渲染器
//
// 用法：
//   assetstudio --list [--pretty]
//   assetstudio --render <素材id> [选项]
//
// 选项：
//   --state <key>          切换状态（如 single / triple）
//   --param k=v            覆盖参数（可重复）
//   --frames N             渲染帧数（动画）
//   --fps F                帧率（仅写进 json 元数据）
//   --zoom N               整数倍放大（最近邻，默认 1）
//   --crop / --no-crop     自动裁剪到内容
//   --pad N                裁剪留白（默认 6）
//   --bg RRGGBB            画布背景色（默认 0A0C18）
//   --palette <spec>       调色板（none/enemy/skill/… 或 R,G,B[,tint]）
//   --scene / --no-scene   真实场景上下文开关
//   --scene-dim N          场景压暗程度 0-255（默认 130）
//   --grid / --axes / --symmetry / --bbox   创作辅助叠加层
//   --out <path>           输出 PNG 路径（多帧时为目录）
//   --ascii                打印字符画
//   --analysis             打印像素分析
//   --json                 打印元数据 JSON
//   --seed N               固定随机种子（背景用 rand 生成星星）
// ============================================================

// ---- 与 main.cpp 相同的前导 include（游戏头文件依赖这些标准库先引入） ----
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <deque>
#include <sys/stat.h>

#include "palette.h"
#include "color_intercept.h"   // 必须在游戏绘制头文件之前生效
#include "png_writer.h"
#include "inspect.h"
#include "preview_harness.h"
#include "asset_registry.h"    // 内含游戏真实头文件

// ============================================================
// 选项
// ============================================================
struct Options {
    std::string cmd;
    std::string assetId;
    std::string outPath;
    std::string state;
    std::map<std::string, double> params;
    int    frames = 0;          // 0 = 用素材默认
    int    fps = 12;
    int    zoom = 1;
    int    cropOverride = -1;   // -1 用素材默认
    int    pad = 6;
    unsigned int bg = 0x0A0C18;
    std::string paletteSpec = "none";
    bool   sceneOn = false;     // 场景上下文默认关闭（按需求做成可开启按钮）
    int    sceneDim = 130;
    int    scenePad = 60;       // 开场景时多留的上下文边距
    bool   grid = false, axes = false, symmetry = false, bboxOverlay = false;
    bool   wantAscii = false, wantAnalysis = false, wantJson = false, pretty = false;
    int    asciiCols = 0;
    unsigned int seed = 0xA55F;
};

static bool parseHexColor(const std::string& s, unsigned int& out) {
    unsigned int v = 0;
    if (sscanf(s.c_str(), "%x", &v) != 1) return false;
    out = v & 0xFFFFFFu;
    return true;
}

static void mkdirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1) mkdir(cur.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
}

// ============================================================
// JSON 辅助
// ============================================================
static std::string jsonEscape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        char ch = s[i];
        switch (ch) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)ch < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", ch); o += b; }
                else o += ch;
        }
    }
    return o;
}
static std::string jstr(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }
static std::string jnum(double d) {
    char b[64];
    if (d == (long long)d) snprintf(b, sizeof(b), "%lld", (long long)d);
    else snprintf(b, sizeof(b), "%g", d);
    return b;
}

static std::string entryJson(const AssetEntry& e, bool pretty, const std::string& indent) {
    std::string nl = pretty ? "\n" : "";
    std::string i1 = pretty ? indent + "  " : "";
    std::string i2 = pretty ? indent + "    " : "";
    std::string i3 = pretty ? indent + "      " : "";
    std::string s = "{" + nl;
    s += i1 + jstr("id")       + ": " + jstr(e.id) + "," + nl;
    s += i1 + jstr("name")     + ": " + jstr(e.name) + "," + nl;
    s += i1 + jstr("chapter")  + ": " + jstr(e.chapter) + "," + nl;
    s += i1 + jstr("category") + ": " + jstr(e.category) + "," + nl;
    s += i1 + jstr("desc")     + ": " + jstr(e.desc) + "," + nl;
    s += i1 + jstr("source")   + ": " + jstr(e.source) + "," + nl;
    s += i1 + jstr("crop")     + ": " + (e.crop ? "true" : "false") + "," + nl;
    s += i1 + jstr("frames")   + ": " + jnum(e.frames) + "," + nl;
    s += i1 + jstr("scene")    + ": " + jstr(e.scene) + "," + nl;

    s += i1 + jstr("tags") + ": [";
    for (size_t i = 0; i < e.tags.size(); ++i) s += (i ? ", " : "") + jstr(e.tags[i]);
    s += "]," + nl;

    s += i1 + jstr("states") + ": [" + nl;
    for (size_t i = 0; i < e.states.size(); ++i) {
        s += i2 + "{" + jstr("key") + ": " + jstr(e.states[i].key) + ", "
                 + jstr("label") + ": " + jstr(e.states[i].label) + "}";
        s += (i + 1 < e.states.size() ? "," : "") + nl;
    }
    s += i1 + "]," + nl;

    s += i1 + jstr("params") + ": [" + nl;
    for (size_t i = 0; i < e.params.size(); ++i) {
        const ParamSpec& p = e.params[i];
        s += i2 + "{" + jstr("key") + ": " + jstr(p.key) + ", "
                 + jstr("label") + ": " + jstr(p.label) + ", "
                 + jstr("min") + ": " + jnum(p.mn) + ", "
                 + jstr("max") + ": " + jnum(p.mx) + ", "
                 + jstr("def") + ": " + jnum(p.def) + ", "
                 + jstr("step") + ": " + jnum(p.step) + "}";
        s += (i + 1 < e.params.size() ? "," : "") + nl;
    }
    s += i1 + "]" + nl;
    (void)i3;
    s += indent + "}";
    return s;
}

// ============================================================
// 像素处理：裁剪 + 整数倍最近邻放大
// ============================================================
static void cropZoomToRgb(const unsigned int* src, int sw, int sh,
                          int cx, int cy, int cw, int ch, int zoom,
                          std::vector<unsigned char>& out, int& ow, int& oh) {
    if (cw <= 0 || ch <= 0) { cx = 0; cy = 0; cw = sw; ch = sh; }
    ow = cw * zoom;
    oh = ch * zoom;
    out.assign((size_t)ow * oh * 3, 0);
    for (int y = 0; y < oh; ++y) {
        int sy = cy + y / zoom;
        if (sy < 0 || sy >= sh) continue;
        for (int x = 0; x < ow; ++x) {
            int sx = cx + x / zoom;
            if (sx < 0 || sx >= sw) continue;
            unsigned int p = src[(size_t)sy * sw + sx];
            size_t o = ((size_t)y * ow + x) * 3;
            out[o + 0] = (unsigned char)((p >> 16) & 0xFF);
            out[o + 1] = (unsigned char)((p >>  8) & 0xFF);
            out[o + 2] = (unsigned char)( p        & 0xFF);
        }
    }
}

// ============================================================
// 叠加层绘制（绕过调色板，避免辅助线被染色）
// ============================================================
static void drawOverlays(SDL_Renderer* r, int W, int H, const Options& o,
                         const AsAnalysis& content) {
    if (o.grid) {
        setDrawColorRaw(r, 70, 80, 110, 110);
        for (int x = 0; x <= W; x += 50) SDL_RenderDrawLine(r, x, 0, x, H);
        for (int y = 0; y <= H; y += 50) SDL_RenderDrawLine(r, 0, y, W, y);
        setDrawColorRaw(r, 110, 125, 170, 140);
        for (int x = 0; x <= W; x += 100) SDL_RenderDrawLine(r, x, 0, x, H);
        for (int y = 0; y <= H; y += 100) SDL_RenderDrawLine(r, 0, y, W, y);
    }
    if (o.axes) {
        setDrawColorRaw(r, 255, 90, 90, 150);
        SDL_RenderDrawLine(r, W / 2, 0, W / 2, H);
        setDrawColorRaw(r, 90, 200, 255, 150);
        SDL_RenderDrawLine(r, 0, H / 2, W, H / 2);
    }
    if (o.symmetry && !content.empty) {
        int axisX = content.bboxX + content.bboxW / 2;
        setDrawColorRaw(r, 255, 120, 255, 170);
        for (int y = 0; y < H; y += 8) SDL_RenderDrawLine(r, axisX, y, axisX, y + 4);
        // 镜像对照线：把包围盒左右边界标出来
        setDrawColorRaw(r, 255, 120, 255, 70);
        SDL_RenderDrawLine(r, content.bboxX, 0, content.bboxX, H);
        SDL_RenderDrawLine(r, content.bboxX + content.bboxW - 1, 0,
                              content.bboxX + content.bboxW - 1, H);
    }
    if (o.bboxOverlay && !content.empty) {
        SDL_Rect rc;
        rc.x = content.bboxX; rc.y = content.bboxY;
        rc.w = content.bboxW; rc.h = content.bboxH;
        setDrawColorRaw(r, 120, 255, 140, 200);
        SDL_RenderDrawRect(r, &rc);
        // 四角加粗，方便定位
        for (int i = 0; i < 6; ++i) {
            SDL_RenderDrawPoint(r, rc.x + i, rc.y);
            SDL_RenderDrawPoint(r, rc.x + i, rc.y + rc.h - 1);
            SDL_RenderDrawPoint(r, rc.x + rc.w - 1 - i, rc.y);
            SDL_RenderDrawPoint(r, rc.x + rc.w - 1 - i, rc.y + rc.h - 1);
        }
    }
}

// ============================================================
// 单次渲染：把素材（含可选场景）画进离屏画布并读回像素
// ============================================================
struct RenderOut {
    std::vector< std::vector<unsigned int> > frames;
    int W = 0, H = 0;
    double suggestedZoom = 1.0;
};

static bool renderAsset(const AssetEntry& e, const Options& o, DrawCtx& ctx,
                        bool useScene, RenderOut& out, std::string& err) {
    int W = 800, H = 600;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) { err = std::string("创建画布失败: ") + SDL_GetError(); return false; }
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
    if (!r) { SDL_FreeSurface(surf); err = std::string("创建软件渲染器失败: ") + SDL_GetError(); return false; }

    int frames = o.frames > 0 ? o.frames : (e.frames > 0 ? e.frames : 1);
    if (frames < 1) frames = 1;
    if (frames > 240) frames = 240;

    AssetEntry* sceneEntry = useScene ? findAsset(e.scene) : nullptr;
    if (sceneEntry && !sceneEntry->draw) sceneEntry = nullptr;

    out.W = W; out.H = H;
    out.frames.clear();

    for (int f = 0; f < frames; ++f) {
        // 清屏（绕过调色板，否则背景色也会被染色）
        setDrawColorRaw(r, (o.bg >> 16) & 0xFF, (o.bg >> 8) & 0xFF, o.bg & 0xFF, 255);
        SDL_RenderClear(r);

        if (sceneEntry) {
            DrawCtx sctx;
            sctx.r = r; sctx.W = W; sctx.H = H;
            sctx.frame = f; sctx.totalFrames = frames;
            sctx.t = frames > 1 ? (double)f / (double)frames : 0.0;
            sctx.priv = ctx.scenePriv;
            for (size_t i = 0; i < sceneEntry->params.size(); ++i)
                sctx.params[sceneEntry->params[i].key] = sceneEntry->params[i].def;
            sceneEntry->draw(sctx);
            ctx.scenePriv = sctx.priv;

            if (o.sceneDim > 0) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                setDrawColorRaw(r, 0, 0, 0, (Uint8)o.sceneDim);
                SDL_Rect full = {0, 0, W, H};
                SDL_RenderFillRect(r, &full);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            }
        }

        ctx.r = r; ctx.frame = f; ctx.totalFrames = frames;
        ctx.t = frames > 1 ? (double)f / (double)frames : 0.0;

        // 有些素材（例如 drawTextLine / Font::drawChar）不自己设置绘制颜色，
        // 而是依赖调用方先设色。这里统一种下白色作为默认值：
        //   ① 避免它们被残留的背景色"画成隐身"
        //   ② 走调色板层，于是这些素材也能被阵营配色
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);

        e.draw(ctx);

        std::vector<unsigned int> buf((size_t)W * H, 0);
        if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, &buf[0], W * 4) != 0) {
            err = std::string("读回像素失败: ") + SDL_GetError();
            SDL_DestroyRenderer(r); SDL_FreeSurface(surf); return false;
        }
        out.frames.push_back(buf);
    }

    SDL_DestroyRenderer(r);
    SDL_FreeSurface(surf);
    return true;
}

// ============================================================
// main
// ============================================================
static void usage() {
    printf("Asset Studio — 无头素材渲染器\n\n");
    printf("  assetstudio --list [--pretty]\n");
    printf("  assetstudio --render <素材id> [--state s] [--param k=v]... [--out 路径] [--ascii]\n\n");
    printf("调色板: %s\n", paletteList().c_str());
}

int main(int argc, char** argv) {
    // 软件渲染器不需要显示器或声音设备，默认使用 dummy，保留显式驱动覆盖。
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 0);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 0);
    Options o;

    if (argc < 2) { usage(); return 1; }

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { fprintf(stderr, "[ERR] %s 缺少参数\n", name); exit(2); }
            return argv[++i];
        };

        if (a == "--list")            o.cmd = "list";
        else if (a == "--verify-all") o.cmd = "verify";
        else if (a == "--render")  { o.cmd = "render"; o.assetId = next("--render"); }
        else if (a == "--state")      o.state = next("--state");
        else if (a == "--out")        o.outPath = next("--out");
        else if (a == "--frames")     o.frames = atoi(next("--frames").c_str());
        else if (a == "--fps")        o.fps = atoi(next("--fps").c_str());
        else if (a == "--zoom")       o.zoom = atoi(next("--zoom").c_str());
        else if (a == "--pad")        o.pad = atoi(next("--pad").c_str());
        else if (a == "--crop")       o.cropOverride = 1;
        else if (a == "--no-crop")    o.cropOverride = 0;
        else if (a == "--bg")       { std::string v = next("--bg"); if (!parseHexColor(v, o.bg)) { fprintf(stderr, "[ERR] 背景色格式应为 RRGGBB\n"); return 2; } }
        else if (a == "--palette")    o.paletteSpec = next("--palette");
        else if (a == "--scene")      o.sceneOn = true;
        else if (a == "--no-scene")   o.sceneOn = false;
        else if (a == "--scene-dim")  o.sceneDim = atoi(next("--scene-dim").c_str());
        else if (a == "--scene-pad")  o.scenePad = atoi(next("--scene-pad").c_str());
        else if (a == "--grid")       o.grid = true;
        else if (a == "--axes")       o.axes = true;
        else if (a == "--symmetry")   o.symmetry = true;
        else if (a == "--bbox")       o.bboxOverlay = true;
        else if (a == "--ascii")      o.wantAscii = true;
        else if (a == "--ascii-cols") o.asciiCols = atoi(next("--ascii-cols").c_str());
        else if (a == "--analysis")   o.wantAnalysis = true;
        else if (a == "--json")       o.wantJson = true;
        else if (a == "--pretty")     o.pretty = true;
        else if (a == "--seed")       o.seed = (unsigned int)atoi(next("--seed").c_str());
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a.size() > 8 && a.substr(0, 8) == "--param=") {
            std::string kv = a.substr(8);
            size_t eq = kv.find('=');
            if (eq == std::string::npos) { fprintf(stderr, "[ERR] --param 格式应为 k=v\n"); return 2; }
            o.params[kv.substr(0, eq)] = atof(kv.substr(eq + 1).c_str());
        }
        else if (a == "--param") {
            std::string kv = next("--param");
            size_t eq = kv.find('=');
            if (eq == std::string::npos) { fprintf(stderr, "[ERR] --param 格式应为 k=v\n"); return 2; }
            o.params[kv.substr(0, eq)] = atof(kv.substr(eq + 1).c_str());
        }
        else if (a == "--zoom-x") { /* 兼容占位 */ }
        else { fprintf(stderr, "[ERR] 未知参数: %s\n", a.c_str()); usage(); return 2; }
    }

    // 固定随机种子：背景用 rand() 生成星星，不固定就无法稳定复现
    srand(o.seed);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[ERR] SDL 初始化失败: %s\n", SDL_GetError());
        return 1;
    }

    buildAssetRegistry();

    Palette pal;
    if (!parsePalette(o.paletteSpec, pal)) {
        fprintf(stderr, "[ERR] 无法识别的调色板: %s\n  可用: %s\n", o.paletteSpec.c_str(), paletteList().c_str());
        return 2;
    }
    activePalette() = pal;

    // ---------------- list ----------------
    if (o.cmd == "list") {
        std::string nl = o.pretty ? "\n" : "";
        std::string ind = o.pretty ? "  " : "";
        std::string s = "{" + nl + ind + "\"assets\": [" + nl;
        std::deque<AssetEntry>& v = assetRegistry();
        for (size_t i = 0; i < v.size(); ++i) {
            s += (o.pretty ? "    " : "") + entryJson(v[i], o.pretty, o.pretty ? "    " : "");
            s += (i + 1 < v.size() ? "," : "") + nl;
        }
        s += ind + "]" + nl + "}" + nl;
        printf("%s", s.c_str());
        SDL_Quit();
        return 0;
    }

    // ---------------- verify：批量自检所有已登记素材 ----------------
    // 批量接入素材时靠这个命令一次性确认：每个素材都渲染出了东西、
    // 每个状态都有内容、声明的场景真的能叠上去。
    if (o.cmd == "verify") {
        std::deque<AssetEntry>& all = assetRegistry();
        unsigned int bgK = 0xFF000000u | o.bg;
        int nOk = 0, nEmpty = 0, nFail = 0;
        std::string json = "{\"results\":[";
        bool firstJson = true;

        for (size_t i = 0; i < all.size(); ++i) {
            AssetEntry& e = all[i];
            struct VCase { std::string label; std::string state; bool scene; };
            std::vector<VCase> cases;
            if (e.states.empty()) {
                VCase vc; vc.label = "默认"; vc.state = ""; vc.scene = false;
                cases.push_back(vc);
            } else {
                for (size_t s = 0; s < e.states.size(); ++s) {
                    VCase vc; vc.label = e.states[s].label; vc.state = e.states[s].key; vc.scene = false;
                    cases.push_back(vc);
                }
            }
            if (!e.scene.empty()) {
                VCase vc; vc.label = "真实场景";
                vc.state = e.states.empty() ? std::string() : e.states[0].key;
                vc.scene = true;
                cases.push_back(vc);
            }

            for (size_t c = 0; c < cases.size(); ++c) {
                Options oo = o;
                oo.frames = 1;
                oo.sceneOn = cases[c].scene;
                DrawCtx vctx;
                for (size_t k = 0; k < e.params.size(); ++k)
                    vctx.params[e.params[k].key] = e.params[k].def;
                vctx.state = cases[c].state;

                // 第一遍：素材本体（无场景）—— 用来判断"素材到底画出来没有"。
                // 带场景时画布会被背景涂满，"非空"就失去意义了，所以必须分开判。
                RenderOut baseRo;
                std::string verr;
                bool rendered = (e.draw != nullptr) &&
                                renderAsset(e, oo, vctx, false, baseRo, verr);
                AsAnalysis a;
                if (rendered) a = asAnalyze(&baseRo.frames[0][0], baseRo.W, baseRo.H, bgK);

                // 第二遍：带场景 —— 顺便确认场景真的叠上去了（而不是静默失效）
                bool sceneApplied = true;
                if (rendered && cases[c].scene) {
                    RenderOut scRo;
                    if (!renderAsset(e, oo, vctx, true, scRo, verr)) {
                        sceneApplied = false;
                    } else {
                        int diff = 0;
                        for (size_t px = 0; px < scRo.frames[0].size() && diff < 32; ++px)
                            if ((scRo.frames[0][px] & 0x00FFFFFFu) !=
                                (baseRo.frames[0][px] & 0x00FFFFFFu)) ++diff;
                        sceneApplied = (diff > 0);
                        if (!sceneApplied) verr = "场景未生效（叠加前后画面无差异）";
                    }
                }

                std::string status;
                if (!rendered)          { status = "FAIL";  ++nFail;  if (verr.empty()) verr = "没有绘制实现"; }
                else if (a.empty)       { status = "EMPTY"; ++nEmpty; }
                else if (!sceneApplied) { status = "FAIL";  ++nFail;  }
                else                    { status = "OK";    ++nOk;    }

                if (o.wantJson) {
                    if (!firstJson) json += ",";
                    firstJson = false;
                    char b[640];
                    snprintf(b, sizeof(b),
                             "{\"id\":%s,\"name\":%s,\"case\":%s,\"scene\":%s,"
                             "\"status\":%s,\"pixels\":%d,\"bbox\":[%d,%d,%d,%d]}",
                             jstr(e.id).c_str(), jstr(e.name).c_str(), jstr(cases[c].label).c_str(),
                             cases[c].scene ? "true" : "false", jstr(status).c_str(),
                             a.contentPixels, a.bboxX, a.bboxY, a.bboxW, a.bboxH);
                    json += b;
                } else {
                    const char* mark = (status == "OK") ? "  OK  " : (status == "EMPTY" ? "  空  " : " 失败 ");
                    char b[640];
                    if (status == "OK")
                        snprintf(b, sizeof(b), "%s %-32s %-10s %-16s %6d 像素  %dx%d",
                                 mark, e.id.c_str(), e.category.c_str(), cases[c].label.c_str(),
                                 a.contentPixels, a.bboxW, a.bboxH);
                    else if (status == "EMPTY")
                        snprintf(b, sizeof(b), "%s %-32s %-10s %-16s 渲染为空白",
                                 mark, e.id.c_str(), e.category.c_str(), cases[c].label.c_str());
                    else
                        snprintf(b, sizeof(b), "%s %-32s %-10s %-16s %s",
                                 mark, e.id.c_str(), e.category.c_str(), cases[c].label.c_str(),
                                 verr.c_str());
                    printf("%s\n", b);
                }
            }
        }

        if (o.wantJson) {
            char tail[256];
            snprintf(tail, sizeof(tail), "],\"ok\":%d,\"empty\":%d,\"fail\":%d,\"total\":%d}",
                     nOk, nEmpty, nFail, nOk + nEmpty + nFail);
            json += tail;
            printf("%s\n", json.c_str());
        } else {
            printf("\n────────────────────────────────────────────────\n");
            printf("  通过 %d   空白 %d   失败 %d   共 %d\n",
                   nOk, nEmpty, nFail, nOk + nEmpty + nFail);
        }
        SDL_Quit();
        return (nFail > 0) ? 1 : 0;
    }

    // ---------------- render ----------------
    if (o.cmd != "render") { usage(); SDL_Quit(); return 1; }

    AssetEntry* e = findAsset(o.assetId);
    if (!e) {
        fprintf(stderr, "[ERR] 找不到素材: %s\n可用的有:\n", o.assetId.c_str());
        std::deque<AssetEntry>& v = assetRegistry();
        for (size_t i = 0; i < v.size(); ++i)
            fprintf(stderr, "  %-28s %s\n", v[i].id.c_str(), v[i].name.c_str());
        SDL_Quit();
        return 1;
    }

    // 组装参数：默认值 + 命令行覆盖
    DrawCtx ctx;
    for (size_t i = 0; i < e->params.size(); ++i)
        ctx.params[e->params[i].key] = e->params[i].def;
    for (std::map<std::string, double>::iterator it = o.params.begin(); it != o.params.end(); ++it) {
        bool known = false;
        for (size_t i = 0; i < e->params.size(); ++i)
            if (e->params[i].key == it->first) known = true;
        if (!known)
            fprintf(stderr, "[WARN] 素材 %s 没有参数 '%s'，仍按原样传入\n",
                    e->id.c_str(), it->first.c_str());
        ctx.params[it->first] = it->second;
    }
    ctx.state = o.state.empty()
              ? (e->states.empty() ? std::string() : e->states[0].key)
              : o.state;

    // ---- 第一遍：不带场景，用于计算素材自身的内容包围盒 ----
    // 场景会把画布每个像素都涂上颜色，"背景色"判定不再成立，
    // 包围盒会退化成一整张画布。所以素材范围和最终画面必须分两遍算。
    RenderOut base;
    std::string err;
    if (!renderAsset(*e, o, ctx, false, base, err)) {
        fprintf(stderr, "[ERR] %s\n", err.c_str());
        SDL_Quit();
        return 1;
    }

    // ---- 第二遍：需要时才带场景上下文 ----
    bool wantScene = o.sceneOn && !e->scene.empty() && findAsset(e->scene) != nullptr;
    RenderOut out;
    if (wantScene) {
        DrawCtx sctx;                  // 全新上下文，避免跨遍复用持久状态
        sctx.params = ctx.params;
        sctx.state  = ctx.state;
        if (!renderAsset(*e, o, sctx, true, out, err)) {
            fprintf(stderr, "[ERR] %s\n", err.c_str());
            SDL_Quit();
            return 1;
        }
    } else {
        out = base;
    }

    // ---- 分析：基于"无场景"的底图，多帧取包围盒并集避免动画抖动 ----
    unsigned int bgKey = 0xFF000000u | o.bg;
    AsAnalysis uni;
    uni.empty = true;
    int uMinX = base.W, uMinY = base.H, uMaxX = -1, uMaxY = -1;
    std::vector<AsAnalysis> perFrame;
    for (size_t f = 0; f < base.frames.size(); ++f) {
        AsAnalysis a = asAnalyze(&base.frames[f][0], base.W, base.H, bgKey);
        perFrame.push_back(a);
        if (a.empty) continue;
        uni.empty = false;
        uMinX = std::min(uMinX, a.bboxX);
        uMinY = std::min(uMinY, a.bboxY);
        uMaxX = std::max(uMaxX, a.bboxX + a.bboxW - 1);
        uMaxY = std::max(uMaxY, a.bboxY + a.bboxH - 1);
    }
    if (!uni.empty) {
        uni.bboxX = uMinX; uni.bboxY = uMinY;
        uni.bboxW = uMaxX - uMinX + 1; uni.bboxH = uMaxY - uMinY + 1;
        uni.contentPixels = perFrame[0].contentPixels;
        uni.fillRatio = perFrame[0].fillRatio;
        uni.symmetryLR = perFrame[0].symmetryLR;
        uni.symmetryTB = perFrame[0].symmetryTB;
        uni.symmetry = perFrame[0].symmetry;
        uni.lineWidth = perFrame[0].lineWidth;
        uni.topColors = perFrame[0].topColors;
    }

    // ---- 叠加层：画在分析之后，所以不会污染包围盒 ----
    bool wantOverlay = o.grid || o.axes || o.symmetry || o.bboxOverlay || e->overlay;
    if (wantOverlay) {
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, out.W, out.H, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
        for (size_t f = 0; f < out.frames.size(); ++f) {
            // 把已渲染帧贴回画布，再叠加辅助线
            memcpy(surf->pixels, &out.frames[f][0], (size_t)out.W * out.H * 4);

            if (e->overlay) {
                DrawCtx octx;
                octx.r = r; octx.W = out.W; octx.H = out.H;
                octx.params = ctx.params;
                octx.state  = ctx.state;
                octx.frame  = (int)f;
                octx.totalFrames = (int)out.frames.size();
                octx.t = out.frames.size() > 1
                       ? (double)f / (double)out.frames.size() : 0.0;
                e->overlay(octx);
            }
            drawOverlays(r, out.W, out.H, o, uni);
            SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, &out.frames[f][0], out.W * 4);
        }
        SDL_DestroyRenderer(r);
        SDL_FreeSurface(surf);
    }

    // ---- 裁剪 + 放大 + 写出 ----
    bool doCrop = (o.cropOverride == -1) ? e->crop : (o.cropOverride == 1);
    // 开场景时多留边距，否则裁剪得太紧就看不到上下文了
    int effPad = wantScene ? std::max(o.pad, o.scenePad) : o.pad;
    int cx = 0, cy = 0, cw = out.W, ch = out.H;
    if (doCrop && !uni.empty) {
        cx = std::max(0, uni.bboxX - effPad);
        cy = std::max(0, uni.bboxY - effPad);
        cw = std::min(out.W - cx, uni.bboxW + effPad * 2);
        ch = std::min(out.H - cy, uni.bboxH + effPad * 2);
    }

    int zoom = o.zoom;
    if (zoom < 1) zoom = 1;

    std::vector<std::string> written;
    if (!o.outPath.empty()) {
        bool multi = out.frames.size() > 1;
        if (multi) mkdirs(o.outPath);

        for (size_t f = 0; f < out.frames.size(); ++f) {
            std::vector<unsigned char> rgb;
            int ow = 0, oh = 0;
            cropZoomToRgb(&out.frames[f][0], out.W, out.H, cx, cy, cw, ch, zoom, rgb, ow, oh);

            char path[1024];
            if (multi) snprintf(path, sizeof(path), "%s/frame_%03d.png", o.outPath.c_str(), (int)f);
            else       snprintf(path, sizeof(path), "%s", o.outPath.c_str());

            if (!as_png::write(path, &rgb[0], ow, oh)) {
                fprintf(stderr, "[ERR] 写出 PNG 失败: %s\n", path);
                SDL_Quit();
                return 1;
            }
            written.push_back(path);
        }
    }

    // ---- 文本输出 ----
    size_t midFrame = out.frames.size() / 2;
    if (o.wantAnalysis) {
        printf("素材: %s  (%s)\n", e->name.c_str(), e->id.c_str());
        printf("状态: %s\n", ctx.state.c_str());
        printf("调色板: %s\n", pal.name.c_str());
        printf("帧数: %d\n", (int)out.frames.size());
        printf("%s", asAnalysisText(uni, out.W, out.H).c_str());
        printf("  裁剪区域: x=%d y=%d w=%d h=%d   (输出 %dx%d, zoom=%d)\n",
               cx, cy, cw, ch, cw * zoom, ch * zoom, zoom);
        printf("  调色板拦截调用: %d 次\n", paletteHits());
    }
    if (o.wantAscii) {
        printf("%s", asAsciiArt(&out.frames[midFrame][0], out.W, out.H, o.asciiCols, bgKey, doCrop).c_str());
    }
    if (o.wantJson) {
        printf("{\"id\":%s,\"state\":%s,\"frames\":%d,\"canvasW\":%d,\"canvasH\":%d,"
               "\"bbox\":[%d,%d,%d,%d],\"crop\":[%d,%d,%d,%d],\"zoom\":%d,"
               "\"contentPixels\":%d,\"symmetry\":%.4f,\"empty\":%s,\"files\":[",
               jstr(e->id).c_str(), jstr(ctx.state).c_str(), (int)out.frames.size(),
               out.W, out.H,
               uni.bboxX, uni.bboxY, uni.bboxW, uni.bboxH,
               cx, cy, cw, ch, zoom,
               uni.contentPixels, uni.symmetry, uni.empty ? "true" : "false");
        for (size_t i = 0; i < written.size(); ++i)
            printf("%s%s", i ? "," : "", jstr(written[i]).c_str());
        printf("]}\n");
    }

    SDL_Quit();
    return 0;
}
