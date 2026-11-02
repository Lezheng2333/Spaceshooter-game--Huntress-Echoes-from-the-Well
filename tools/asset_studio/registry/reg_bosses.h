#pragma once
// ============================================================
// Boss（3 个）
//
// Boss 都是多状态机，所以预览必须能切阶段 + 推进帧数。
// 阶段靠「推进帧数」滑块扫，状态按钮用来直接跳到关键姿态。
// ============================================================

#include "reg_common.h"
#include "audio.h"
#include "font.h"
#include "particles.h"
#include "player.h"
#include "bullets.h"
#include "chapter_manager.h"
#include "ch1/ch1_aliens.h"
#include "ch1/ch1_boss.h"
#include "ch2/ch2_background.h"
#include "ch2/ch2_sphere_boss.h"
#include "ch2/ch2_boss.h"

// ============================================================
// 第一章 Boss · TELAMONDO
// ============================================================
// cfg 是未入档的指针成员，游戏里读档后由 Game 重新绑定。
// 这里同样要绑一次，否则 Boss 尺寸会退化成内置的兜底值。
struct Ch1BossHost {
    Ch1Boss boss;
    ParticleManager  pm;
    Ch1AlienManager  aliens;
    Font font;
    bool prepared;
    Ch1BossHost() : prepared(false) {}
};

static void prepareCh1Boss(Ch1BossHost* st, DrawCtx& c) {
    if (st->prepared) return;
    st->prepared = true;
    st->boss.setConfig(&previewChapterMgr().getChapterConfig(0).bossConfig);
    st->boss.reset();
    st->boss.setActive(true);
    if (c.S("phase2")) st->boss.phase2TriggeredRef() = true;
    if (c.S("heal"))   st->boss.setCh1HealWavesEnabled(true);
}

static void drawCh1Boss(DrawCtx& c) {
    Ch1BossHost* st = (Ch1BossHost*)c.priv;
    if (!st) { st = new Ch1BossHost(); c.priv = st; prepareCh1Boss(st, c); }

    int frames = (int)c.P("advance", 170);
    for (int i = 0; i < frames; ++i) {
        st->boss.updateEnterAnimation(nullptr);   // enterDuration 默认 150
        st->boss.updateMovement();
    }
    if (c.P("heal", 0) > 0.5 || c.S("heal")) {
        st->boss.setCh1HealWavesEnabled(true);
        for (int i = 0; i < (int)c.P("healAdvance", 260); ++i)
            st->boss.updateCh1HealWaves(st->pm, st->aliens, nullptr);
    }

    st->pm.draw(c.r);
    st->boss.drawCircularShockwave(c.r);
    st->boss.drawBody(c.r);
    st->boss.drawCh1HealWaves(c.r);
    if (c.P("hpbar", 1) > 0.5) st->boss.drawHPBar(c.r, st->font);
}

inline void registerCh1Boss() {
    reg("ch1.boss.telamondo", "第一章 Boss · TELAMONDO", "ch1", "boss")
        .describe("第一章章末 Boss。菱形机体，入场从屏幕上方下降（150 帧），"
                  "到位后按正弦轨迹左右巡航。血量降到一半进入二阶段。"
                  "「治疗波」是它吸收外星飞船回血的机制，会从机体发出绿色波纹。"
                  "「推进帧数」150 帧左右入场完成。")
        .from("v2.0.0/multiple code files/ch1/ch1_boss.h:350 (Ch1Boss::drawBody, "
              "Ch1Boss::drawCircularShockwave, Ch1Boss::drawCh1HealWaves, Ch1Boss::drawHPBar)")
        .tag("Boss").tag("第一章").tag("多阶段").tag("动画")
        .state_("normal", "一阶段")
        .state_("phase2", "二阶段")
        .state_("heal",   "治疗波")
        .param("advance",     "推进帧数",   0, 600, 170, 10)
        .param("heal",        "开启治疗波", 0,   1,   0, 1)
        .param("healAdvance", "治疗波帧数", 0, 600, 260, 10)
        .param("hpbar",       "显示血条",   0,   1,   1, 1)
        .drawFn(drawCh1Boss);
}

// ---------------- 第一章 Boss · 血条 -
static void drawCh1BossHpBar(DrawCtx& c) {
    Ch1Boss boss;
    Font font;
    boss.setConfig(&previewChapterMgr().getChapterConfig(0).bossConfig);
    boss.reset();
    boss.setActive(true);
    boss.hpRef() = (int)c.P("hp", 700);
    boss.drawHPBar(c.r, font);
}
inline void registerCh1BossHpBar() {
    reg("ch1.boss.hpbar", "第一章 Boss · 血条", "ch1", "boss")
        .describe("TELAMONDO 的血条（含奖励血量段的显示）。")
        .from("v2.0.0/multiple code files/ch1/ch1_boss.h:385 (Ch1Boss::drawHPBar)")
        .tag("Boss").tag("第一章").tag("HUD").tag("零件")
        .param("hp", "当前血量", 0, 1000, 700, 10)
        .drawFn(drawCh1BossHpBar);
}

// ============================================================
// 第二章 Boss · 球体（六角密铺菱形球）
// ============================================================
// 注意：Ch2SphereBoss 的 private 段从第 357 行才开始，
// bg / playerRef / state / cells 全是 private，只能通过公有 API 操作：
//   init(bg, player) 绑定指针 · startEntering()/startAtCenter() 决定入场方式
//   getState() 读状态 · update() 推进状态机
struct SphereBossHost {
    Ch2SphereBoss  boss;
    Ch2Background  bg;
    Player         pl;
    SphereBossHost() { pl.setX(100); pl.setY(300); }
};

// 推进到目标状态（用公有 getState() 判断），返回实际推进帧数
static int advanceToSphereState(Ch2SphereBoss& b, Ch2SphereBoss::State target, int limit) {
    int i = 0;
    for (; i < limit; ++i) {
        if (b.getState() == target) return i;
        b.update();
    }
    return i;
}

static void drawSphereBoss(DrawCtx& c) {
    SphereBossHost* st = (SphereBossHost*)c.priv;
    if (!st) {
        st = new SphereBossHost();
        c.priv = st;
        st->boss.init(&st->bg, &st->pl);   // 指针成员必须走 init

        if (c.S("entering")) {
            st->boss.startEntering();      // 从右侧 1200 处滚入
            // 注意：update() 只设置背景滚动速度，球体的实际位移由外部
            // syncScreenPos(scrollX) 完成（游戏主循环里做的）。
            // 预览里必须自己驱动 scroll，否则球体永远停在屏幕外。
            for (int i = 0; i < 3000 && st->boss.getCx() > 640; ++i) {
                st->bg.update();
                st->boss.syncScreenPos(st->bg.getScrollX());
                st->boss.update();
                double sp = st->boss.getBgTargetSpeed();
                if (sp >= 0) st->bg.setSpeed(sp);
            }
        } else if (c.S("shatter")) {
            st->boss.startAtCenter();
            st->boss.takeDamage(st->boss.getMaxHp());
            advanceToSphereState(st->boss, Ch2SphereBoss::SHATTERING, 2);
            st->boss.update();
        } else {
            st->boss.startAtCenter();      // FIGHT 态：直接摆在屏幕中央，单元全激活
        }
    }
    int frames = (int)c.P("advance", 0);
    int steps = c.frame == 0 ? frames : 1;
    for (int i = 0; i < steps; ++i) {
        st->bg.update();
        if (st->boss.getState() == Ch2SphereBoss::ENTERING)
            st->boss.syncScreenPos(st->bg.getScrollX());
        st->boss.update();
        double speed = st->boss.getBgTargetSpeed();
        if (speed >= 0) st->bg.setSpeed(speed);
    }

    // 注意：这里不自己画背景。背景交给"真实场景"开关统一处理，
    // 否则基础渲染里已经含走廊，场景开关叠加前后就没有差异了。
    st->boss.draw(c.r);
}

inline void registerSphereBoss() {
    reg("ch2.boss.sphere", "第二章 Boss · 球体", "ch2", "boss")
        .describe("第二章中段的球体 Boss：由六角密铺的菱形单元拼成的球体，"
                  "状态机共 9 段（入场 → 激活 → 战斗 → 碎裂 → 塌缩 → 震动 → 冲出 → 结束）。"
                  "「战斗」状态直接摆到屏幕中央且全部单元已点亮；「入场」从右侧 1200 处滚入；"
                  "「碎裂」会自动推进到炸散那一刻，再用「推进帧数」继续往下演。")
        .from("v2.0.0/multiple code files/ch2/ch2_sphere_boss.h:291 (Ch2SphereBoss::draw)")
        .tag("Boss").tag("第二章").tag("多阶段").tag("动画").tag("线稿复杂")
        .state_("fight",    "战斗（球体完整）")
        .state_("entering", "入场（随廊桥滚入）")
        .state_("shatter",  "碎裂")
        .param("advance", "推进帧数", 0, 900, 0, 10)
        .withScene("ch2.bg.corridor")
        .framesN(20)
        .drawFn(drawSphereBoss);
}

// ============================================================
// 第二章 Boss · MOONWELL WARDEN
// ============================================================
struct WardenHost {
    Ch2WardenBoss boss;
    ParticleManager pm;
    AudioEngine audio;
    FloatingTextManager ft;
    Player pl;
    Font font;
    int hp; bool go;
    Ch2Background bg;
    WardenHost() : hp(3), go(false) { pl.setX(100); pl.setY(300); }
};

// 推进到目标状态（用公有 getState() 判断）
static int advanceToWardenState(WardenHost* st, Ch2WardenBoss::State target, int limit) {
    int i = 0;
    for (; i < limit; ++i) {
        if (st->boss.getState() == target) return i;
        st->boss.update(st->pl, st->hp, st->go, st->pm, st->audio, st->ft);
    }
    return i;
}

static void drawWarden(DrawCtx& c) {
    WardenHost* st = (WardenHost*)c.priv;
    if (!st) {
        st = new WardenHost();
        c.priv = st;
        st->boss.startEntering();            // reset() 后进入 ENTERING
        if (!c.S("entering")) advanceToWardenState(st, Ch2WardenBoss::FIGHT, 600);

        if (c.S("enraged")) {
            st->boss.takeDamage(Ch2WardenBoss::MAX_HP - Ch2WardenBoss::ENRAGE_HP + 1);
        }
        if (c.S("dying")) {
            // 走游戏真实的死亡路径（takeDamage 里 hp<=0 → DYING）
            st->boss.takeDamage(Ch2WardenBoss::MAX_HP + 1);
        }
    }
    int frames = (int)c.P("advance", 90);
    int steps = c.frame == 0 ? frames : 1;
    for (int i = 0; i < steps; ++i) {
        st->boss.update(st->pl, st->hp, st->go, st->pm, st->audio, st->ft);
        st->pm.update(); st->pm.removeInactive();
    }

    st->pm.draw(c.r);
    st->boss.draw(c.r);
    if (c.P("bullets", 1) > 0.5) st->boss.drawBullets(c.r);
    if (c.P("hpbar", 1) > 0.5)   st->boss.drawHPBar(c.r, st->font);
}

inline void registerWarden() {
    reg("ch2.boss.warden", "第二章 Boss · MOONWELL WARDEN", "ch2", "boss")
        .describe("第二章章末 Boss：悬停的菱形核心 + 旋转碎片环 + 扫掠刀锋。"
                  "四种状态：入场（无敌下降）→ 战斗（放射爆发 / 瞄准齐射 / 螺旋 三种弹幕循环）"
                  "→ 暴怒（血量低于 50%，更快更密）→ 濒死（连锁爆炸）。")
        .from("v2.0.0/multiple code files/ch2/ch2_boss.h:241 (Ch2WardenBoss::draw, "
              "Ch2WardenBoss::drawBullets, Ch2WardenBoss::drawHPBar)")
        .tag("Boss").tag("第二章").tag("多阶段").tag("弹幕").tag("动画")
        .state_("entering", "入场")
        .state_("fight",    "战斗")
        .state_("enraged",  "暴怒")
        .state_("dying",    "濒死（爆炸阶段）")
        .param("advance", "推进帧数", 0, 900, 90, 10)
        .param("bullets", "显示弹幕", 0,   1,  1, 1)
        .param("hpbar",   "显示血条", 0,   1,  1, 1)
        .withScene("ch2.bg.corridor")
        .framesN(20)
        .drawFn(drawWarden);
}

inline void registerAllBosses() {
    registerCh1Boss();
    registerCh1BossHpBar();
    registerSphereBoss();
    registerWarden();
}
