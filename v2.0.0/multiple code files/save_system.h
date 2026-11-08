#pragma once

// ============== SaveSystem — 存档格式 / 文件 IO / 设置持久化 ==============
// 存档文件布局（全部小端序）：
//   SaveHeader { char magic[8]="SFSSAVE0"; u32 fmtVer; u32 gameVer; u32 payloadBytes; u32 payloadCrc32; }
//   payload    { SaveMeta; 各系统状态 ... }   ← 由 Game::serializeAll() 以固定顺序写入
// 读档时先校验 magic/版本/长度/CRC，任何一项失败都拒绝载入并提示，不会崩溃。

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char SAVE_MAGIC[8] = {'S', 'F', 'S', 'S', 'A', 'V', 'E', '0'};
static const uint32_t SAVE_FORMAT_VERSION = 1;    // 结构变更时 +1（旧存档自动判为不兼容）
static const uint32_t SAVE_GAME_VERSION = 10226;  // 游戏版本：major*10000 + minor*100 + patch（Ver 1.2.26）

// ============== CRC32 ==============
static inline uint32_t saveCrc32(const unsigned char* d, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ d[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// ============== SaveArchive — 双端序列化（writing=true 写，false 读） ==============
// 用法：每个状态类实现 template<class Ar> void visit(Ar& ar) { ar.ioNum(field); ... }
class SaveArchive {
public:
    static const uint32_t MAX_ELEMS = 300000;   // 向量长度上限（防损坏文件炸内存）
    static const uint32_t MAX_STRING = 200000;  // 字符串长度上限

    bool writing;
    bool bad;                      // 读取越界 / 非法长度 → true
    std::vector<unsigned char> buf; // 写=输出缓冲，读=输入数据
    size_t rp;                     // 读指针

    explicit SaveArchive(bool w = true) : writing(w), bad(false), rp(0) {}

    void raw(void* p, size_t n) {
        if (bad || n == 0) return;
        if (writing) {
            const unsigned char* s = (const unsigned char*)p;
            buf.insert(buf.end(), s, s + n);
        } else {
            if (rp + n > buf.size()) { bad = true; memset(p, 0, n); return; }
            memcpy(p, buf.data() + rp, n);
            rp += n;
        }
    }

    // 数值（bool 请用 ioBool；enum 请用 ioEnum）
    template <class T> void ioNum(T& v) { raw(&v, sizeof(T)); }

    void ioBool(bool& v) {
        unsigned char b = v ? 1 : 0;
        raw(&b, 1);
        if (!writing) v = (b != 0);
    }

    template <class E> void ioEnum(E& v) {
        int t = (int)v;
        ioNum(t);
        if (!writing) v = (E)t;
    }

    void ioBytes(void* p, size_t n) { raw(p, n); }

    void ioStr(std::string& s) {
        uint32_t n = writing ? (uint32_t)s.size() : 0u;
        ioNum(n);
        if (bad) return;
        if (n > MAX_STRING) { bad = true; if (!writing) s.clear(); return; }
        if (writing) { if (n) raw(&s[0], n); }
        else { s.resize(n); if (n) raw(&s[0], n); }
    }

    template <class T> void ioVecStr(std::vector<T>& v) {   // T = std::string
        uint32_t n = writing ? (uint32_t)v.size() : 0u;
        ioNum(n);
        if (bad) return;
        if (n > MAX_ELEMS) { bad = true; if (!writing) v.clear(); return; }
        if (!writing) v.resize(n);
        for (uint32_t i = 0; i < n; ++i) ioStr(v[i]);
    }

    template <class T> void ioVecNum(std::vector<T>& v) {   // T = POD 数值
        uint32_t n = writing ? (uint32_t)v.size() : 0u;
        ioNum(n);
        if (bad) return;
        if (n > MAX_ELEMS) { bad = true; if (!writing) v.clear(); return; }
        if (!writing) v.resize(n);
        if (n) raw(v.data(), (size_t)n * sizeof(T));
    }

    template <class T> void ioVecObj(std::vector<T>& v) {   // T 需实现 visit(Ar&)
        uint32_t n = writing ? (uint32_t)v.size() : 0u;
        ioNum(n);
        if (bad) return;
        if (n > MAX_ELEMS) { bad = true; if (!writing) v.clear(); return; }
        if (!writing) v.resize(n);
        for (uint32_t i = 0; i < n; ++i) v[i].visit(*this);
    }

    template <class T> void ioObj(T& v) { v.visit(*this); }
};

// ============== SaveHeader ==============
#pragma pack(push, 1)
struct SaveHeader {
    char magic[8];
    uint32_t fmtVer;
    uint32_t gameVer;
    uint32_t payloadBytes;
    uint32_t payloadCrc32;
};
#pragma pack(pop)

// ============== SaveMeta — 存档槽列表所需的摘要（位于 payload 最前端） ==============
struct SaveMeta {
    uint32_t fmtVer;
    uint32_t gameVer;
    uint32_t savedAt;        // unix 时间戳
    uint32_t playFrames;     // 本局游戏帧数
    int chapterIdx;          // 0-based 章节
    char chapterTitle[24];   // 章节名（显示用）
    int score;
    int baseHP;              // Ch1 基地血量
    int playerHP;            // Ch2 战机血量
    int flow;                // Ch2Flow（-1 = 第一章）
    int plane;               // 0=Ch2Trainer 1=NightElf 2=TrainingPlane
    bool valid;
    bool broken;             // 文件存在但读不出（损坏 / 版本不符）——仅界面用，不入档

    SaveMeta() : fmtVer(SAVE_FORMAT_VERSION), gameVer(SAVE_GAME_VERSION), savedAt(0), playFrames(0),
                 chapterIdx(0), score(0), baseHP(0), playerHP(0), flow(-1), plane(0),
                 valid(false), broken(false) {
        chapterTitle[0] = '\0';
    }

    void setTitle(const char* t) {
        // 必须整块清零：snprintf 不会填充尾部，否则存档字节不确定（无法做逐字节校验）
        memset(chapterTitle, 0, sizeof(chapterTitle));
        snprintf(chapterTitle, sizeof(chapterTitle), "%s", t ? t : "");
    }

    template <class Ar> void visit(Ar& ar) {
        ar.ioNum(fmtVer);
        ar.ioNum(gameVer);
        ar.ioNum(savedAt);
        ar.ioNum(playFrames);
        ar.ioNum(chapterIdx);
        ar.ioBytes(chapterTitle, sizeof(chapterTitle));
        ar.ioNum(score);
        ar.ioNum(baseHP);
        ar.ioNum(playerHP);
        ar.ioNum(flow);
        ar.ioNum(plane);
    }

    std::string versionText() const {
        char b[24];
        snprintf(b, sizeof(b), "%u.%u.%u", gameVer / 10000, (gameVer / 100) % 100, gameVer % 100);
        return std::string(b);
    }

    std::string timeText() const {
        if (savedAt == 0) return "--";
        time_t t = (time_t)savedAt;
        struct tm tmv;
        localtime_r(&t, &tmv);
        char b[32];
        strftime(b, sizeof(b), "%Y-%m-%d %H:%M", &tmv);
        return std::string(b);
    }
};

// ============== SaveSystem — 路径解析 / 文件读写 / 目录枚举 ==============
class SaveSystem {
public:
    static bool dirExists(const std::string& p) {
        struct stat st;
        return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    static bool fileExists(const std::string& p) {
        struct stat st;
        return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
    static bool ensureDir(const std::string& p) {
        if (dirExists(p)) return true;
        return mkdir(p.c_str(), 0755) == 0;
    }
    static bool writable(const std::string& p) { return access(p.c_str(), W_OK) == 0; }

    static std::string appSupportDir() {
        const char* home = getenv("HOME");
        std::string base = home ? std::string(home) : std::string(".");
        return base + "/Library/Application Support/StarFoxSpaceShooter";
    }

    // 存档根目录：优先 ./saves（开发时在可执行文件旁），不可写则回退 Application Support
    // （发布成 .app 后从 Finder / DMG 启动时 cwd=/，且写进 bundle 会破坏签名）
    // 环境变量 SFSS_SAVE_DIR 可强制指定（--selftest 用它把存档写进临时目录，避免污染玩家存档）
    static const std::string& baseDir() {
        static std::string cached;
        if (!cached.empty()) return cached;
        const char* forced = getenv("SFSS_SAVE_DIR");
        if (forced && forced[0]) {
            std::string dir = forced;
            ensureDir(dir);
            cached = dir;
            return cached;
        }
        std::string local = "saves";
        if (ensureDir(local) && writable(local)) { cached = local; return cached; }
        std::string app = appSupportDir();
        ensureDir(app);
        std::string sub = app + "/saves";
        ensureDir(sub);
        cached = sub;
        return cached;
    }

    static std::string slotPath(int idx) {
        char name[32];
        if (idx <= 0) snprintf(name, sizeof(name), "/auto.sav");
        else          snprintf(name, sizeof(name), "/slot%d.sav", idx);
        return baseDir() + name;
    }
    static const char* slotLabel(int idx) {
        static const char* labels[7] = {"AUTO", "SLOT 1", "SLOT 2", "SLOT 3", "SLOT 4", "SLOT 5", "SLOT 6"};
        if (idx < 0) idx = 0;
        if (idx > 6) idx = 6;
        return labels[idx];
    }
    static const int SLOT_COUNT = 7;   // 0=AUTO（自动写入） 1..6=手动槽

    static std::string settingsPath() { return baseDir() + "/settings.dat"; }

    static bool writeBytes(const std::string& path, const std::vector<unsigned char>& data) {
        std::string tmp = path + ".tmp";
        FILE* f = fopen(tmp.c_str(), "wb");
        if (!f) return false;
        bool ok = true;
        if (!data.empty()) ok = (fwrite(data.data(), 1, data.size(), f) == data.size());
        ok = (fclose(f) == 0) && ok;
        if (!ok) { remove(tmp.c_str()); return false; }
        if (rename(tmp.c_str(), path.c_str()) != 0) { remove(tmp.c_str()); return false; }
        return true;
    }

    static bool readBytes(const std::string& path, std::vector<unsigned char>& out) {
        out.clear();
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        if (sz <= 0) { fclose(f); return false; }
        fseek(f, 0, SEEK_SET);
        out.resize((size_t)sz);
        bool ok = (fread(out.data(), 1, (size_t)sz, f) == (size_t)sz);
        fclose(f);
        if (!ok) out.clear();
        return ok;
    }

    // 组装完整存档文件内容（header + payload + CRC）
    static std::vector<unsigned char> pack(const std::vector<unsigned char>& payload) {
        SaveHeader h;
        memcpy(h.magic, SAVE_MAGIC, 8);
        h.fmtVer = SAVE_FORMAT_VERSION;
        h.gameVer = SAVE_GAME_VERSION;
        h.payloadBytes = (uint32_t)payload.size();
        h.payloadCrc32 = saveCrc32(payload.empty() ? (const unsigned char*)"" : payload.data(),
                                   payload.size());
        std::vector<unsigned char> out;
        out.resize(sizeof(SaveHeader));
        memcpy(out.data(), &h, sizeof(SaveHeader));
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    // 校验 header + CRC；成功时把 payload 交给 ar（已就绪的读档 archive）
    static bool unpack(std::vector<unsigned char>& file, SaveArchive& ar, std::string& err) {
        if (file.size() < sizeof(SaveHeader)) { err = "FILE TOO SHORT"; return false; }
        SaveHeader h;
        memcpy(&h, file.data(), sizeof(SaveHeader));
        if (memcmp(h.magic, SAVE_MAGIC, 8) != 0) { err = "NOT A SAVE FILE"; return false; }
        // 只按"存档结构版本"把关：格式没变就允许读旧版本的档（游戏版本号仅作显示/诊断）。
        // 规则：任何影响 serializeAll() 字段顺序/类型的改动，都必须把 SAVE_FORMAT_VERSION +1。
        if (h.fmtVer != SAVE_FORMAT_VERSION) { err = "SAVE FORMAT v" + toStr((int)h.fmtVer) + " UNSUPPORTED"; return false; }
        if (file.size() < sizeof(SaveHeader) + h.payloadBytes) { err = "FILE TRUNCATED"; return false; }
        const unsigned char* p = file.data() + sizeof(SaveHeader);
        if (saveCrc32(p, h.payloadBytes) != h.payloadCrc32) { err = "CHECKSUM MISMATCH"; return false; }
        ar.writing = false;
        ar.bad = false;
        ar.rp = 0;
        ar.buf.assign(p, p + h.payloadBytes);
        return true;
    }

    // 只读摘要（供读档列表显示），不解析完整状态
    static bool readMeta(const std::string& path, SaveMeta& meta, std::string& err) {
        std::vector<unsigned char> file;
        if (!readBytes(path, file)) { err = "CANNOT READ FILE"; return false; }
        SaveArchive ar(false);
        if (!unpack(file, ar, err)) return false;
        meta = SaveMeta();
        meta.visit(ar);
        if (ar.bad) { err = "META CORRUPTED"; return false; }
        meta.valid = true;
        return true;
    }

    static std::string toStr(int v) {
        char b[16];
        snprintf(b, sizeof(b), "%d", v);
        return std::string(b);
    }

    // 目录枚举：先子目录（按名字排序），后文件；"." 与隐藏文件跳过
    struct Entry { std::string name; bool isDir; };
    static void listDir(const std::string& dir, std::vector<Entry>& out) {
        out.clear();
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            if (n.size() > 0 && n[0] == '.') continue;   // 隐藏文件（含 .tmp）
            std::string full = dir + "/" + n;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            Entry en;
            en.name = n;
            en.isDir = S_ISDIR(st.st_mode);
            out.push_back(en);
        }
        closedir(d);
        std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return a.name < b.name;
        });
    }

    static std::string parentOf(const std::string& dir) {
        if (dir == "/" || dir.empty()) return "/";
        size_t pos = dir.find_last_of('/');
        if (pos == std::string::npos) return ".";
        if (pos == 0) return "/";
        return dir.substr(0, pos);
    }

    // 自动文件名（另存为用）：save_20250829_142600.sav
    static std::string autoFileName() {
        time_t t = time(nullptr);
        struct tm tmv;
        localtime_r(&t, &tmv);
        char b[64];
        strftime(b, sizeof(b), "save_%Y%m%d_%H%M%S.sav", &tmv);
        return std::string(b);
    }
};

// ============== FileBrowser — 游戏内迷你文件浏览器（自由读档 / 另存为） ==============
class FileBrowser {
public:
    struct Row {
        std::string label;   // 显示文本
        std::string path;    // 完整路径（".." 行为父目录）
        bool isDir;
        bool isNew;          // 另存为模式下的"新文件"行
        bool isSav;
    };

    std::string dir;
    std::vector<Row> rows;
    int cursor;
    bool saveMode;          // false=读档模式（选文件读取） true=另存为模式（选目录/文件名）
    std::string newName;    // 另存为模式下的自动文件名
    int scroll;

    FileBrowser() : cursor(0), saveMode(false), scroll(0) {}

    void open(const std::string& startDir, bool saveAs) {
        dir = startDir;
        saveMode = saveAs;
        cursor = 0;
        scroll = 0;
        refresh();
    }

    void refresh() {
        rows.clear();
        // 另存为模式：第一行是"新文件"（自动命名），读档模式没有
        if (saveMode) {
            Row r;
            r.label = "[ NEW FILE: " + newName + " ]";
            r.path = dir + "/" + newName;
            r.isDir = false; r.isNew = true; r.isSav = true;
            rows.push_back(r);
        }
        if (dir != "/") {
            Row up;
            up.label = "../";
            up.path = SaveSystem::parentOf(dir);
            up.isDir = true; up.isNew = false; up.isSav = false;
            rows.push_back(up);
        }
        std::vector<SaveSystem::Entry> entries;
        SaveSystem::listDir(dir, entries);
        for (size_t i = 0; i < entries.size(); ++i) {
            Row r;
            r.isDir = entries[i].isDir;
            r.isNew = false;
            r.isSav = false;
            r.path = dir + "/" + entries[i].name;
            if (r.isDir) {
                r.label = entries[i].name + "/";
            } else {
                r.label = entries[i].name;
                size_t n = entries[i].name.size();
                r.isSav = (n > 4 && entries[i].name.compare(n - 4, 4, ".sav") == 0);
            }
            if (!r.isDir && !saveMode && !r.isSav) continue;   // 读档模式只列 .sav
            rows.push_back(r);
        }
        if (cursor >= (int)rows.size()) cursor = (int)rows.size() - 1;
        if (cursor < 0) cursor = 0;
        clampScroll();
    }

    void clampScroll() {
        const int VISIBLE = 9;
        if (cursor < scroll) scroll = cursor;
        if (cursor >= scroll + VISIBLE) scroll = cursor - VISIBLE + 1;
        if (scroll < 0) scroll = 0;
    }

    void move(int d) {
        if (rows.empty()) return;
        cursor += d;
        if (cursor < 0) cursor = 0;
        if (cursor >= (int)rows.size()) cursor = (int)rows.size() - 1;
        clampScroll();
    }

    const Row* current() const {
        if (cursor < 0 || cursor >= (int)rows.size()) return nullptr;
        return &rows[cursor];
    }

    bool enterDir(const std::string& p) {
        if (!SaveSystem::dirExists(p)) return false;
        dir = p;
        cursor = 0; scroll = 0;
        refresh();
        return true;
    }

    bool goUp() {
        if (dir == "/") return false;
        return enterDir(SaveSystem::parentOf(dir));
    }

    std::string displayDir() const {
        // 路径过长时省略中段
        if (dir.size() <= 46) return dir;
        return "..." + dir.substr(dir.size() - 43);
    }
};

// ============== GameSettings — OPTIONS 设置持久化 ==============
struct GameSettings {
    int aimAssist;   // 0/1
    int voiceLang;   // 0=中文 1=English 2=OFF
    int bgm, sfx;    // 1..10
    int eqLow, eqMid, eqHigh;  // -5..5

    GameSettings() : aimAssist(0), voiceLang(0), bgm(7), sfx(7), eqLow(0), eqMid(0), eqHigh(0) {}

    static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    void load() {
        FILE* f = fopen(SaveSystem::settingsPath().c_str(), "rb");
        if (!f) return;
        char key[32];
        int val;
        while (fscanf(f, "%31s %d", key, &val) == 2) {
            if (strcmp(key, "aim_assist") == 0) aimAssist = clampInt(val, 0, 1);
            else if (strcmp(key, "voice_lang") == 0) voiceLang = clampInt(val, 0, 2);
            else if (strcmp(key, "bgm") == 0) bgm = clampInt(val, 1, 10);
            else if (strcmp(key, "sfx") == 0) sfx = clampInt(val, 1, 10);
            else if (strcmp(key, "eq_low") == 0) eqLow = clampInt(val, -5, 5);
            else if (strcmp(key, "eq_mid") == 0) eqMid = clampInt(val, -5, 5);
            else if (strcmp(key, "eq_high") == 0) eqHigh = clampInt(val, -5, 5);
        }
        fclose(f);
    }

    void save() const {
        FILE* f = fopen(SaveSystem::settingsPath().c_str(), "wb");
        if (!f) return;
        fprintf(f, "version 1\n");
        fprintf(f, "aim_assist %d\n", aimAssist);
        fprintf(f, "voice_lang %d\n", voiceLang);
        fprintf(f, "bgm %d\n", bgm);
        fprintf(f, "sfx %d\n", sfx);
        fprintf(f, "eq_low %d\n", eqLow);
        fprintf(f, "eq_mid %d\n", eqMid);
        fprintf(f, "eq_high %d\n", eqHigh);
        fclose(f);
    }
};
