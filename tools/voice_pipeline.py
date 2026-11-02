#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
voice_pipeline.py — 星际火狐 旁白/对话 双语语音生成管线
========================================================
从 game.h 自动提取全部旁白页 + 对话行(英文原文),配合内置中英对照表,
按"角色 → 音色"映射,合成 voice/zh/<crc32>.wav 与 voice/en/<crc32>.wav。
文件名 = 英文原文的 CRC32(zlib,与游戏内 AudioEngine::voiceCrc32 完全一致),
因此游戏文本改动会自动映射到新文件,缺失的语音文件会被游戏静默跳过。

用法:
  python3 tools/voice_pipeline.py collect             # 提取台词并校验翻译 → voice_script.json
  python3 tools/voice_pipeline.py generate            # 合成全部语音(默认 edge-tts 在线)
  python3 tools/voice_pipeline.py generate --backend piper \
        --zh-model <zh.onnx> --zh-config <zh.json> \
        --en-model <en.onnx> --en-config <en.json>
  python3 tools/voice_pipeline.py generate --backend gpt-sovits --api http://127.0.0.1:9880
"""
import asyncio, json, os, re, subprocess, sys, tempfile, time, zlib

ROOT = os.path.dirname(os.path.abspath(__file__))          # tools/
PROJECT = os.path.dirname(ROOT)
GAME_DIR = os.path.join(PROJECT, "v2.0.0", "multiple code files")
GAME_H = os.path.join(GAME_DIR, "game.h")
OUT_DIR = os.path.join(GAME_DIR, "voice")                  # voice/zh voice/en
SCRIPT_JSON = os.path.join(ROOT, "voice_script.json")

# ==================== 角色 → 音色映射(可自行调整) ====================
# edge-tts 在线音色(已验证在当前网络环境可用);
# GPT-SoVITS/piper 等后端可在此扩展 refer/模型字段
EDGE_VOICES = {
    "NARRATOR":         {"zh": "zh-CN-YunyangNeural",   "en": "en-US-ChristopherNeural", "rate": "-10%"},
    "Martha":           {"zh": "zh-CN-XiaoxiaoNeural",  "en": "en-US-JennyNeural",       "rate": "+0%"},
    "Bryssa":           {"zh": "zh-CN-XiaoyiNeural",    "en": "en-GB-SoniaNeural",       "rate": "+0%"},
    "Bryssa from Tower": {"zh": "zh-CN-XiaoyiNeural",   "en": "en-GB-SoniaNeural",       "rate": "+0%"},
    "Ally (ai copilot)": {"zh": "zh-CN-YunxiNeural",    "en": "en-US-GuyNeural",         "rate": "+0%"},
    "Tower (ai)":       {"zh": "zh-CN-YunjianNeural",   "en": "en-US-BrianNeural",       "rate": "+0%"},
    "Moonwell (ai)":    {"zh": "zh-CN-YunjianNeural",   "en": "en-US-AndrewNeural",      "rate": "-8%"},
    "":                 {"zh": "zh-CN-YunxiNeural",     "en": "en-US-AriaNeural",        "rate": "+0%"},
}

# ==================== 中英对照表(键 = 英文原文,含 \n 转义后的真实换行) ====================
ZH = {
    # ---- Chapter 1 开幕旁白 ----
    "Stellar Calendar 24th.\nIn the deep space.\nThe \"Life\" base.":
        "星海历24年。\n宇宙深处。\n\"生命\"基地。",
    "Martha aces her final test.\nShe's now a member of \"Huntress\".":
        "玛莎以全优成绩通过最终考核。\n她正式成为\"Huntress\"中队的一员。",
    "When she is just off the plane.\nHeading for the rest area.\nAnnouncement sounds.":
        "她刚走下飞机。\n正走向休息区。\n警报骤然响起。",
    "Tower: \"Hostile objects approaching!\nAll fighters scramble!\"":
        "塔台:\"检测到大量不明飞行物!\n所有战机紧急升空!\"",
    "\"Repeat: Scramble! Scramble!\"":
        "\"重复:紧急升空!紧急升空!\"",
    "At the same moment,\na series of massive explosions behind her.\nShe turns --":
        "与此同时,\n身后传来一连串巨大的爆炸。\n她猛地回头——",
    "All fighters become wreckage...":
        "所有战机,皆成残骸……",
    "Martha runs back to the hangar.\nOnly one old trainer left.":
        "玛莎冲回机库。\n只剩一架老旧的教练机。",
    "Into the cockpit. Engines up.\nShe reports:\n\"I'm taking off. Flight code:21395\"":
        "她登上座舱,引擎启动。\n她向塔台汇报:\n\"我准备起飞。飞行代码21395\"",
    "No response from the tower.\nOnly a sharp, harsh noise.":
        "塔台没有回应。\n只有一阵刺耳的噪声。",
    "Martha heads into space.\nAgain and alone.":
        "玛莎飞向太空。\n再一次,孤身一人。",

    # ---- Chapter 2 开幕旁白 ----
    "A hard-won victory. But the cost is plain.\nThe \"Life\" base lies scarred and broken.\nRepairs will take weeks.":
        "一场艰难的胜利。代价却清晰可见。\n\"生命\"基地满目疮痍。\n修复工作将持续数周。",
    "The shockwave defense system,\npushed far beyond its limits,\nneeds a full overhaul\nbefore it can fire again.":
        "冲击波防御系统,\n被推至极限之外,\n需要彻底的检修\n才能再次开火。",
    "The base's long-range comms array\nis destroyed.\nMartha must fly to the\n\"Moonwell\" R&D center,\nwith Bryssa in her backseat,\nto bring home new equipment.":
        "基地的大功率通讯设备\n已被摧毁。\n玛莎必须飞往\n\"月井\"研发中心,\n后座载着Bryssa,\n运回新的设备。",
    "Ally enters its standby mode.\nBryssa, exhausted, drifts off.\nThe old trainer's engine hums\na tired, weary vibration.":
        "Ally进入待机状态。\n精疲力竭的Bryssa沉沉睡去。\n老旧教练机的引擎\n发出疲惫的震动。",
    "Space is terribly quiet...":
        "宇宙安静得可怕……",
    "......":
        "……",
    "............":
        "…………",
    "After an eternity in the void,\nthe R&D center finally\ndrifts into Martha's comms range.":
        "在虚空中度过漫长的时光后,\n研发中心终于\n进入了玛莎的通讯范围。",
    "Martha opens the channel.\n\"Moonwell, Moonwell, this is Martha\nfrom 'Life' base. Flight code 21395.\"":
        "玛莎尝试建立通讯。\n\"月井,月井,这里是'生命'基地的玛莎。\n飞行代码21395。\"",
    "\"......\"":
        "\"……\"",
    "Silence.":
        "没有回应。",
    "\"Moonwell, Moonwell, 21395.\nRequesting landing clearance.\"":
        "\"月井,月井,这里是21395。\n申请飞机停泊。\"",
    "Nothing but still silence.":
        "依旧毫无回应。",
    "Martha has already flown to the center's gate.\nA massive vacuum door blocks her way.":
        "玛莎已经飞到了研究中心的门前。\n巨大的真空门将她挡在外面。",

    # ---- Chapter 3-5 占位旁白 ----
    "CH.3  ENEMY FORTRESS": "第三章  敌人要塞",
    "We have located the enemy stronghold.": "我们已定位敌人的据点。",
    "Break through their defense line!": "突破他们的防线!",
    "CH.4  ASTEROID BELT": "第四章  小行星带",
    "Danger lurks among the asteroids.": "危险潜伏在小行星之间。",
    "Fly carefully and stay alert.": "小心飞行,保持警惕。",
    "CH.5  FINAL ASSAULT": "第五章  最终突击",
    "This is the final battle.": "这是最后的战役。",
    "Give it everything you've got!": "倾尽全力!",

    # ---- Ch2 结局旁白 ----
    "The Warden falls.\nThe lab falls silent.": "守护者倒下了。\n实验室归于寂静。",
    "The long-range comms unit is found intact.\nBryssa loads it into the trainer's hold.":
        "大功率通讯设备完好无损。\nBryssa将它装入教练机的货舱。",
    "Martha takes one last look at Moonwell.\nThe engines roar to life.":
        "玛莎最后看了一眼月井。\n引擎轰鸣着苏醒。",
    "Home. The \"Life\" base is waiting.": "回家。\n\"生命\"基地正在等待。",

    # ---- Chapter 1 游戏内对话 ----
    "Martha, you're the only one in the air.": "玛莎,现在只有你起飞了。",
    "Hold on as long as you can. The base shockwave cannon is charging.":
        "你尽可能撑住,基地的冲击波防御炮正在准备。",
    "These enemies are made of energy.": "这些敌人都是能量体构成的。",
    "Destroy them. We can collect the energy.": "击碎他们,我们可以得到能量。",
    "Tower communication restored.": "塔台恢复通讯。",
    "Shockwave cannon ready.": "防御炮已经准备好了。",
    "A little more energy!": "还需要一点点能量!",
    "Defense system charged.": "防御体系充能成功。",
    "More enemies incoming. Keep gathering energy.": "敌人越来越多,请继续积攒能量。",
    "The trainer shares energy with the base.": "训练机和基地共享能量系统。",
    "You and the base will upgrade together.": "你会和基地同步升级!",
    "Keep gathering energy.": "请继续积攒能量。",
    "System checking.": "系统自检。",
    "Done.": "自检完成。",
    "Base upgraded again.": "基地已再次获得升级。",
    "Radar shows even more enemies! Watch out!": "雷达上显示的敌人越来越多了,小心!",
    "System checking result:": "系统自检结果:",
    "Aim assist system on this plane.": "本机搭载了瞄准辅助系统。",
    "You shall find it somewhere.": "你应该能在某处找到它。",
    "I've lost contact with the tower!": "我和塔台失去通讯了!",
    "But you and the base can upgrade again soon.": "不过你和基地很快又可以进行一次升级。",
    "Massive energy signature detected.": "检测到巨大能量信号。",
    "Analyzing source...": "正在分析来源……",
    "It's a capital ship.": "是一艘主力舰。",
    "Telamondo-class. Martha, this is what the defense system was built for.":
        "Telamondo级。玛莎,这就是防御体系存在的意义。",
    "Enemy capital ship approaching.": "敌方主力舰正在逼近。",
    "Entering weapons range in 40 seconds.": "预计40秒后进入射程。",
    "Shockwave Defense System is fully charged": "冲击波防御系统已完全充能。",
    "Martha, just keep them off us!": "玛莎,守住防线!",
    "Here it comes...!": "它来了……!",
    "Base defense systems are strengthening.": "基地防御体系正在加强。",
    "Firepower systems being enhanced.": "正在加强火力体系。",
    "Telamondo can absorb energy!": "Telamondo能够吸收能量!",
    "Our firepower can match it!": "我们的火力足够应对!",

    # ---- Chapter 2 门禁对话 ----
    "Are we there? ... Why did we stop?": "到了?……怎么停了?",
    "'Moonwell' ... no response at all.": "\"月井\"……完全没有反应。",
    "So we can't get in?": "那我们进不去了?",
    "Hold on. I'll have Ally try to link into their system.": "别急,我让Ally试试能不能接入研究中心的系统。",
    "... ?": "……          ?",
    "Accessing Moonwell system.": "接入研究中心中。",
    "Connecting. Please wait.": "接入中,请稍等。",
    "All their devices are offline. But the facility still has power.": "他们的设备都是关机状态。但是建筑内依旧有电力。",
    "... Preparing to emit a strong pulse signal. Forcefully waking up nearby hardware.":
        "……准备发射强力脉冲信号,强行启动周围硬件设备。",
    "Scanning code. Verifying ID.": "正在扫描代码。验证身份。",
    "Flight code verified, 21395. Cargo confirmed. No obvious threats. You may proceed.":
        "飞行代码已验证,21395。内容物确认,无明显威胁。请通行。",
    "Welcome, Martha. Ally.": "欢迎,玛莎。Ally。",

    # ---- Chapter 2 廊桥/球体/追逐/研究室/Boss 对话 ----
    "We're in. The whole facility is silent.": "我们进来了。整座设施寂静无声。",
    "It feels like no one has been here for years.": "感觉这里已经好几年没人来过了。",
    "Main power is online. No life signs detected.": "主电源在线。未检测到生命信号。",
    "Stay sharp. We came here for the comms equipment.": "保持警惕。我们来这里是为了通讯设备。",
    "Martha... what is THAT?": "玛莎……那是什么?",
    "Massive object ahead! Slowing down!": "前方发现巨大物体!正在减速!",
    "Brace yourselves.": "大家做好心理准备。",
    "Its surface is shifting. Blue to orange!": "它的表面在变化。蓝色变成橙色了!",
    "Orange means vulnerable. Open fire!": "橙色代表可以攻击。开火!",
    "Energy constructs! We'll have to fight through!": "这些是能量构造体!我们只能一路打过去了!",
    "That one dropped something! Break the shell, grab the core!": "那个家伙掉了东西!打碎外壳,抓住核心!",
    "Pulse skill online! Full bar + SHIFT to release.": "脉冲技能上线!能量满时按Shift释放。",
    "Look! The center of the lab...": "快看!实验室中央……",
    "A fighter. A real one.": "一架战斗机。一架真正的战斗机。",
    "Prototype identified: NightElf. Touch it to transfer control.": "原型机确认:暗夜精灵号。触碰以接管控制权。",
    "NightElf online. Welcome aboard, Martha.": "暗夜精灵号上线。欢迎登机,玛莎。",
    "Chain hits to charge white energy. Full bar = triple fire!": "连续命中积攒白色能量。满格即可三炮齐发!",
    "Massive energy signature! It's blocking the exit!": "巨大能量信号!它挡住了出口!",
    "Then we go through it.": "那就从它身上碾过去。",
    "It's enraged! Watch the patterns!": "它狂暴了!小心弹幕!",
}


# ==================== 提取 ====================
def c_unescape(s: str) -> str:
    out = []
    i = 0
    esc = {'n': '\n', 't': '\t', 'r': '\r', '"': '"', "'": "'", '\\': '\\'}
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            e = s[i + 1]
            out.append(esc.get(e, e))
            i += 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def extract_lines() -> list:
    src = open(GAME_H, encoding='utf-8').read()
    entries = []

    # ---- narration pages (startChapterNarration: chapter switch) ----
    fn = src[src.index('void startChapterNarration'):src.index('void updateNarration')]
    case = -1
    for m in re.finditer(r'case (\d+):|narration\.queue\("((?:[^"\\]|\\.)*)"\)', fn):
        if m.group(1) is not None:
            case = int(m.group(1))
        elif m.group(2) is not None:
            entries.append({"kind": "narration", "chapter": case + 1,
                            "speaker": "NARRATOR", "en": c_unescape(m.group(2))})

    # ---- Ch2 epilogue narration ----
    if 'void startCh2Epilogue' in src:
        fn2 = src[src.index('void startCh2Epilogue'):src.index('void updateBossDefeat')]
        for m in re.finditer(r'narration\.queue\("((?:[^"\\]|\\.)*)"\)', fn2):
            entries.append({"kind": "narration", "chapter": 2,
                            "speaker": "NARRATOR", "en": c_unescape(m.group(1))})

    # ---- dialogue lines (chapter attribution by file position) ----
    # Ch1 dialogue block lives in updateGameplay (before updateCh2Gate);
    # every queueDialogue between updateCh2Gate and updateBossDefeat is Ch2.
    gate_pos = src.index('void updateCh2Gate')
    bossdefeat_pos = src.index('void updateBossDefeat')
    for m in re.finditer(r'dialogueSys\.queueDialogue\("((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\)', src):
        spk, txt = c_unescape(m.group(1)), c_unescape(m.group(2))
        chapter = 2 if gate_pos <= m.start() < bossdefeat_pos else 1
        entries.append({"kind": "dialogue", "chapter": chapter, "speaker": spk, "en": txt})

    # de-duplicate by hash (same text reused, e.g. "Tower communication restored.")
    seen = set()
    unique = []
    for e in entries:
        h = zlib.crc32(e["en"].encode('utf-8')) & 0xFFFFFFFF
        if h in seen:
            continue
        seen.add(h)
        e["hash"] = "%08x" % h
        e["zh"] = ZH.get(e["en"], "")
        unique.append(e)

    # Meaningful file names: voice/<lang>/ch<N>/nar_XX.wav + dlg_<speaker>_XX.wav
    SLUG = {"NARRATOR": "narrator", "Martha": "martha", "Bryssa": "bryssa",
            "Bryssa from Tower": "bryssa", "Ally (ai copilot)": "ally",
            "Tower (ai)": "tower", "Moonwell (ai)": "moonwell", "": "system"}
    counters = {}
    for e in unique:
        slug = SLUG.get(e["speaker"], "other")
        key = (e["chapter"], slug)
        counters[key] = counters.get(key, 0) + 1
        idx = counters[key]
        if e["kind"] == "narration":
            e["name"] = f"ch{e['chapter']}/nar_{idx:02d}.wav"
        else:
            e["name"] = f"ch{e['chapter']}/dlg_{slug}_{idx:02d}.wav"
    return unique


# ==================== 合成后端 ====================
def synth_edge(text: str, voice: str, rate: str, out_wav: str) -> bool:
    import edge_tts
    async def run():
        com = edge_tts.Communicate(text, voice, rate=rate)
        tmp_mp3 = out_wav + ".mp3"
        await com.save(tmp_mp3)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp_mp3,
                        "-ar", "22050", "-ac", "1", "-c:a", "pcm_s16le", out_wav],
                       check=True)
        if os.path.exists(tmp_mp3):
            os.remove(tmp_mp3)
    asyncio.run(run())
    return True


def synth_edge_retry(text, voice, rate, out_wav, tries=3):
    import time
    for i in range(tries):
        try:
            return synth_edge(text, voice, rate, out_wav)
        except Exception as e:
            if os.path.exists(out_wav):
                os.remove(out_wav)
            if os.path.exists(out_wav + ".mp3"):
                os.remove(out_wav + ".mp3")
            if i == tries - 1:
                print(f"  !! synth failed ({voice}): {str(e)[:100]}")
                return False
            time.sleep(1.5 * (i + 1))
    return False


def synth_piper(text: str, model: str, config: str, out_wav: str) -> bool:
    with open(out_wav, "wb") as f:
        subprocess.run(["piper", "--model", model, "--config", config,
                        "--output_file", out_wav], input=text.encode('utf-8'), check=True)
    return True


def synth_gpt_sovits(text: str, lang: str, api: str, speaker: str, out_wav: str) -> bool:
    import urllib.request
    cfg = {
        "text": text, "text_language": lang,
        "refer_wav_path": REFER.get(speaker, {}).get(lang, ""),
        "prompt_text": PROMPT.get(speaker, {}).get(lang, ""),
        "prompt_language": lang,
    }
    data = json.dumps(cfg).encode('utf-8')
    req = urllib.request.Request(api, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as resp, open(out_wav, "wb") as f:
        f.write(resp.read())
    return True


# GPT-SoVITS 按角色配置(参考音频)——需要你提供后填入
REFER = {}
PROMPT = {}


def generate(entries, backend, args, chapters=None):
    if chapters is not None:
        entries = [e for e in entries if e["chapter"] in chapters]
        print(f"filtering: only chapters {sorted(chapters)} ({len(entries)} lines)")
    missing_zh = [e for e in entries if not e["zh"]]
    if missing_zh:
        print("!! 以下台词缺少中文翻译(将跳过 zh 合成):")
        for e in missing_zh:
            print("   -", repr(e["en"]))
    total = [0]

    def one(e, lang):
        if lang == "zh" and not e["zh"]:
            return
        out_wav = os.path.join(OUT_DIR, lang, e["name"])
        os.makedirs(os.path.dirname(out_wav), exist_ok=True)
        if os.path.exists(out_wav) and os.path.getsize(out_wav) > 1000:
            return
        text = e["zh"] if lang == "zh" else e["en"]
        # Punctuation-only lines ("......", "... ?") are dramatic pauses — skip
        # synthesis entirely (edge-tts rejects them; the game stays silent).
        if not any(ch.isalnum() for ch in text):
            return
        vc = EDGE_VOICES.get(e["speaker"], EDGE_VOICES[""])
        if backend == "edge":
            synth_edge_retry(text, vc[lang], vc.get("rate", "+0%"), out_wav)
        elif backend == "piper":
            model = args.zh_model if lang == "zh" else args.en_model
            config = args.zh_config if lang == "zh" else args.en_config
            synth_piper(text, model, config, out_wav)
        elif backend == "gpt-sovits":
            synth_gpt_sovits(text, "zh" if lang == "zh" else "en",
                             args.api, e["speaker"], out_wav)
        total[0] += 1
        if total[0] % 20 == 0:
            print(f"  ... {total[0]} clips done ({lang}: {os.path.basename(out_wav)})")

    import concurrent.futures as cf
    tasks = [(e, lang) for e in entries for lang in ("zh", "en")]
    with cf.ThreadPoolExecutor(max_workers=6) as ex:
        list(ex.map(lambda t: one(*t), tasks))
    print(f"done: {total[0]} clips synthesized under {OUT_DIR}")


def write_manifest(entries):
    """voice_manifest.txt: <lang> <crc32> <relpath-from-game-dir> —
    the game maps text hash → audio file (paths include the voice/ prefix)."""
    lines = []
    for e in entries:
        for lang in ("zh", "en"):
            if lang == "zh" and not e["zh"]:
                continue
            p = os.path.join(OUT_DIR, lang, e["name"])
            if os.path.exists(p) and os.path.getsize(p) > 1000:
                lines.append(f"{lang} {e['hash']} voice/{lang}/{e['name']}")
    with open(os.path.join(OUT_DIR, "voice_manifest.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote voice_manifest.txt ({len(lines)} entries)")


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("action", choices=["collect", "generate"])
    ap.add_argument("--backend", choices=["edge", "piper", "gpt-sovits"], default="edge")
    ap.add_argument("--api", default="http://127.0.0.1:9880")
    ap.add_argument("--zh-model", default="")
    ap.add_argument("--zh-config", default="")
    ap.add_argument("--en-model", default="")
    ap.add_argument("--en-config", default="")
    ap.add_argument("--chapters", default=None,
                    help="only synthesize these game chapters (comma list, e.g. '1' or '1,2')")
    args = ap.parse_args()

    entries = extract_lines()
    print(f"extracted {len(entries)} unique lines "
          f"({sum(1 for e in entries if e['kind']=='narration')} narration, "
          f"{sum(1 for e in entries if e['kind']=='dialogue')} dialogue)")

    if args.action == "collect":
        with open(SCRIPT_JSON, "w", encoding="utf-8") as f:
            json.dump(entries, f, ensure_ascii=False, indent=1)
        print(f"wrote {SCRIPT_JSON}")
        missing = [e for e in entries if not e["zh"]]
        print(f"translation coverage: {len(entries)-len(missing)}/{len(entries)}")
        for e in missing:
            print("  MISSING ZH:", repr(e["en"]))
        return

    if not os.path.isdir(os.path.join(GAME_DIR, "voice")):
        os.makedirs(os.path.join(GAME_DIR, "voice"), exist_ok=True)
    chapters = None
    if args.chapters:
        chapters = set(int(c) for c in args.chapters.split(","))
    generate(entries, args.backend, args, chapters)
    write_manifest(entries)
    with open(SCRIPT_JSON, "w", encoding="utf-8") as f:
        json.dump(entries, f, ensure_ascii=False, indent=1)
    print(f"wrote {SCRIPT_JSON}")


if __name__ == "__main__":
    main()
