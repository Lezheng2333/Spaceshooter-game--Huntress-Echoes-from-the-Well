#pragma once
// ============================================================
// 玩家战机（7 架：5 可驾驶 + 1 训练机升级休眠 + 2 休眠）
// 只读接入：直接调用游戏真实的 draw()，主游戏零改动。
// ============================================================

#include "reg_common.h"
#include "player.h"

// 碰撞盒调试叠加层：在内容分析之后画，不撑大包围盒。
// 绿色矩形 = 机身中段胶囊（实际判定还带圆角；这里用方角近似显示）。
static void drawHitboxCapsule(DrawCtx& c, const Player& p) {
    int cx = p.getX(), cy = p.getY();
    int x0 = cx - (int)p.getHitBack();
    int x1 = cx + (int)p.getHitFront();
    int r  = (int)p.getHitRadius();
    setDrawColorRaw(c.r, 120, 255, 160, 210);
    SDL_RenderDrawLine(c.r, x0, cy - r, x1, cy - r);
    SDL_RenderDrawLine(c.r, x0, cy + r, x1, cy + r);
    SDL_RenderDrawLine(c.r, x0, cy - r, x0, cy + r);
    SDL_RenderDrawLine(c.r, x1, cy - r, x1, cy + r);
    setDrawColorRaw(c.r, 120, 255, 160, 80);
    SDL_RenderDrawLine(c.r, x0, cy, x1, cy);
    setDrawColorRaw(c.r, 190, 255, 220, 170);
    SDL_RenderDrawLine(c.r, x0 - 2, cy - r, x0 + 2, cy - r);
    SDL_RenderDrawLine(c.r, x1 - 2, cy + r, x1 + 2, cy + r);
}

// ---------------- 训练机·雨燕（第一章 · 透视视角）----------------
static void drawTrainingPlane(DrawCtx& c) {
    TrainingPlane p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 480));
    p.setRollAngle(c.P("roll", 0) * M_PI / 180.0);
    p.draw(c.r);
}

inline void registerPlayerTrainingPlane() {
    reg("ch1.player.trainingplane", "训练机·雨燕", "ch1", "player")
        .describe("第一章透视视角的默认训练机。五案评审后采用 B 方案「雨燕」：后掠翼 + 单发 + V 尾，"
                  "俯视平面图与第二章共用。游戏内仍叫 Trainer；补充剧情里主角在第一章临时驾驶的"
                  "这架机体名为「雨燕」。「横滚角度」控制机翼滚转。")
        .from("v2.0.0/multiple code files/player.h:76 (TrainingPlane::draw)")
        .tag("战机").tag("可驾驶").tag("第一章").tag("雨燕").tag("默认训练机").tag("透视投影")
        .param("posX", "水平位置", 100, 700, 400, 1)
        .param("posY", "垂直位置", 220, 570, 480, 1)
        .param("roll", "横滚角度", 0, 360, 0, 5)
        .drawFn(drawTrainingPlane);
}

// ---------------- 训练机·雨燕（第二章 · 侧滚视角）----------------
static void drawCh2Trainer(DrawCtx& c) {
    Ch2Trainer p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 300));
    p.draw(c.r);
}

static void overlayCh2TrainerHitbox(DrawCtx& c) {
    if (c.P("hitbox", 0) <= 0.5) return;
    Ch2Trainer p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 300));
    drawHitboxCapsule(c, p);
}

inline void registerPlayerCh2Trainer() {
    reg("ch2.player.trainer", "训练机·雨燕（侧滚）", "ch2", "player")
        .describe("与第一章同一架「雨燕」，把同一张俯视平面图旋转 90° 得到的侧滚视图；"
                  "游戏内仍叫 Trainer。碰撞盒改为机身中段胶囊（8 / 9 / 4），翼尖、尾焰不再参与判定；"
                  "勾选「显示碰撞盒」可看绿色胶囊。")
        .from("v2.0.0/multiple code files/player.h:153 (Ch2Trainer::draw)")
        .tag("战机").tag("可驾驶").tag("第二章").tag("雨燕").tag("默认训练机").tag("侧滚视角")
        .param("posX", "水平位置", 0, 800, 400, 1)
        .param("posY", "垂直位置", 0, 600, 300, 1)
        .param("hitbox", "显示碰撞盒", 0, 1, 0, 1)
        .withScene("ch2.bg.corridor")
        .drawFn(drawCh2Trainer)
        .overlayFn(overlayCh2TrainerHitbox);
}

// ---------------- 训练机升级·狮鹫（休眠资产 · 双视角）----------------
static void drawTrainerUpgrade(DrawCtx& c) {
    TrainerUpgrade p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 480));
    p.setRollAngle(c.P("roll", 0) * M_PI / 180.0);
    p.paintView(c.r, c.S("ch2"));
}

inline void registerPlayerTrainerUpgrade() {
    reg("ch2.player.trainerupgrade", "训练机升级·狮鹫", "ch2", "player")
        .describe("休眠资产：由训练机 D 方案定稿而来的升级型号，三角翼 + 前翼 + 单发，"
                  "保留训练机的单发/单炮结构。默认显示第二章侧滚；在详情「参数」页的「状态」里"
                  "切到「第一章 · 透视」看配套视图。游戏内尚未激活，不接入主线。")
        .from("v2.0.0/multiple code files/player.h:185 (TrainerUpgrade::draw)")
        .tag("战机").tag("休眠资产").tag("训练机升级").tag("双视图").tag("第二章")
        .state_("ch2", "第二章 · 侧滚")
        .state_("ch1", "第一章 · 透视")
        .param("posX", "水平位置", 100, 700, 400, 1)
        .param("posY", "垂直位置", 220, 570, 480, 1)
        .param("roll", "横滚角度（第一章）", 0, 360, 0, 5)
        .drawFn(drawTrainerUpgrade);
}

// ---------------- 暗夜精灵（第二章主力机 · 定稿）----------------
static void drawNightElf(DrawCtx& c) {
    NightElf elf;
    elf.reset();
    elf.setX((int)c.P("posX", 400));
    elf.setY((int)c.P("posY", 300));
    elf.setTripleFire(c.S("triple"));
    elf.draw(c.r);
}

// 炮口 / 碰撞盒都放在叠加层：内容分析之后绘制，不会撑大包围盒或拉低对称度。
static void overlayNightElfExtras(DrawCtx& c) {
    NightElf elf;
    elf.reset();
    elf.setX((int)c.P("posX", 400));
    elf.setY((int)c.P("posY", 300));
    elf.setTripleFire(c.S("triple"));
    if (c.P("muzzle", 0) > 0.5) {
        int n = elf.getGunCount();
        for (int i = 0; i < n; ++i) {
            int ox = 0, oy = 0;
            elf.getGunOffset(i, ox, oy);
            drawMuzzleMarker(c.r, elf.getX() + ox, elf.getY() + oy);
        }
    }
    if (c.P("hitbox", 0) > 0.5) drawHitboxCapsule(c, elf);
}

inline void registerPlayerNightElf() {
    reg("ch2.player.nightelf", "暗夜精灵号", "ch2", "player")
        .describe("第二章门禁序列后解锁的主力战机，也是本轮五案评审后的定稿："
                  "大后掠翼 + 翼尖双发 + 中央主发，三股尾焰落在同一条尾线上；"
                  "上表面受光、下缘冷灰反光，中轴尖头胶囊座舱。"
                  "碰撞盒改为机身中段胶囊（12 / 22 / 5），长机鼻尖端、翼尖不参与判定；"
                  "勾选「显示碰撞盒」可看绿色胶囊。三炮模式下机鼻与上下前翼各一处炮口。")
        .from("v2.0.0/multiple code files/player.h:225 (NightElf::draw)")
        .tag("战机").tag("可驾驶").tag("第二章").tag("三发机").tag("翼尖引擎").tag("定稿")
        .state_("single", "单炮模式")
        .state_("triple", "三炮模式")
        .param("posX",   "水平位置", 0, 800, 400, 1)
        .param("posY",   "垂直位置", 0, 600, 300, 1)
        .param("muzzle", "显示炮口标记", 0, 1, 0, 1)
        .param("hitbox", "显示碰撞盒", 0, 1, 0, 1)
        .withScene("ch2.bg.corridor")
        .drawFn(drawNightElf)
        .overlayFn(overlayNightElfExtras);
}

// ---------------- 德鲁伊（第三章 · 休眠中）----------------
static void drawDruid(DrawCtx& c) {
    Druid p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 300));
    p.draw(c.r);
}

inline void registerPlayerDruid() {
    reg("ch3.player.druid", "德鲁伊号", "ch3", "player")
        .describe("第三章飞机，游戏内标注为 [DORMANT — Chapter 3 激活]，尚未在主线出现。"
                  "燕尾造型：机头 60°、尾部 90° 凹口，正中有导航箭头的引导线。")
        .from("v2.0.0/multiple code files/player.h:254 (Druid::draw)")
        .tag("战机").tag("可驾驶").tag("第三章").tag("休眠资产")
        .param("posX", "水平位置", 0, 800, 400, 1)
        .param("posY", "垂直位置", 0, 600, 300, 1)
        .withScene("ch2.bg.corridor")
        .drawFn(drawDruid);
}

// ---------------- 树妖（第三章 · 休眠资产）----------------
static void drawDryad(DrawCtx& c) {
    Dryad p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 300));
    p.draw(c.r);
}

inline void registerPlayerDryad() {
    reg("ch3.player.dryad", "树妖", "ch3", "player")
        .describe("休眠资产：月银飞翼 + 羽刃翼尖 + 三座嵌入式短舱，由评审方案 B 的飞翼方向定稿而来。"
                  "机头 34px、翼展 ±18px；三股短尾焰从翼后缘短舱喷出，中轴尖头胶囊座舱。"
                  "游戏内尚未激活，只作为第三章候选保留；原版德鲁伊不受影响。")
        .from("v2.0.0/multiple code files/player.h:295 (Dryad::draw)")
        .tag("战机").tag("休眠资产").tag("第三章").tag("飞翼").tag("暗夜精灵家族")
        .param("posX", "水平位置", 0, 800, 400, 1)
        .param("posY", "垂直位置", 0, 600, 300, 1)
        .withScene("ch2.bg.corridor")
        .drawFn(drawDryad);
}

// ---------------- 德鲁伊·幽光（第三章 · 休眠资产 / 原版 Druid 迭代）----------------
static void drawDruidWraithlight(DrawCtx& c) {
    DruidWraithlight p;
    p.reset();
    p.setX((int)c.P("posX", 400));
    p.setY((int)c.P("posY", 300));
    p.draw(c.r);
}

inline void registerPlayerDruidWraithlight() {
    reg("ch3.player.druid.wraithlight", "德鲁伊·幽光", "ch3", "player")
        .describe("休眠资产：无尾隐身箭头 + 折线前缘 + 三缝式尾喷口，由评审方案 E「幽光」定稿而来，"
                  "作为原版德鲁伊战机的迭代型号保留。机头 36px，冰白高光只勾前缘，"
                  "三发靠三道短促的高压喷流识别。原版 Druid 的燕尾线稿仍原样保留；本机尚未在主线激活。")
        .from("v2.0.0/multiple code files/player.h:318 (DruidWraithlight::draw)")
        .tag("战机").tag("休眠资产").tag("第三章").tag("德鲁伊迭代").tag("隐身")
        .param("posX", "水平位置", 0, 800, 400, 1)
        .param("posY", "垂直位置", 0, 600, 300, 1)
        .withScene("ch2.bg.corridor")
        .drawFn(drawDruidWraithlight);
}

inline void registerAllPlayers() {
    registerPlayerTrainingPlane();
    registerPlayerCh2Trainer();
    registerPlayerTrainerUpgrade();
    registerPlayerNightElf();
    registerPlayerDruid();
    registerPlayerDryad();
    registerPlayerDruidWraithlight();
}
