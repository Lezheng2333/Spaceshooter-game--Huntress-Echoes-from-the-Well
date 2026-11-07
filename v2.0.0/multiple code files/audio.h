#pragma once

#include <atomic>

#include "player.h"
#include "types.h"


// ============== AudioEngine 类 ==============
class AudioEngine {
    enum MusicMode { MUSIC_SILENT, MUSIC_CH1, MUSIC_CH2, MUSIC_BOSS };

    SDL_AudioDeviceID audioDev;
    std::atomic<int> bgmVolume, sfxVolume, eqLow, eqMid, eqHigh;
    std::atomic<bool> bossMusicOn, ch2Bgm, bgmOff;
    std::atomic<int> musicIntensity;
    std::vector<ActiveSound> activeSnd;

    // Voice channel (narration / dialogue WAV clips, normalized mono 44.1k)
    struct VoiceClip {
        std::vector<float> samples;
        Uint32 pos;
        bool active;
    };
    std::vector<VoiceClip> voiceClips;
    float voiceGain;

    // BGM state
    int currentMusicMode;
    float musicGain;
    float bgmPhase, bgmPulsePhase, bgmHarmonyPhase, bgmDrumPhase;
    int bgmStepCounter, bgmNoteIndex;
    float bgmNoteFreq;
    int bgmNoteLen;
    uint32_t noiseState;

    // Ch2 BGM bass melody state
    int ch2BassIdx, ch2BassLen;
    float ch2BassFreq, ch2BassPhase;

    static const float BGM_MELODY[32];
    static const int BGM_NOTE_DUR[32];
    static const float BOSS_MELODY[16];
    static const int BOSS_DUR[16];
    static const float CH2_MELODY[126];
    static const int CH2_MEL_DUR[126];
    static const float CH2_BASS[64];
    static const int CH2_BASS_DUR[64];

    static void sdlAudioCB(void* userdata, Uint8* stream, int len) {
        ((AudioEngine*)userdata)->audioCB(stream, len);
    }

    static float advancePhase(float& phase, float freq) {
        phase += freq / 44100.0f;
        if (phase >= 1.0f) phase -= 1.0f;
        return sinf(phase * 2.0f * (float)M_PI);
    }

    float noiseSample() {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return ((noiseState & 0xFFFFu) / 32767.5f) - 1.0f;
    }

    void resetMusicState(int mode) {
        currentMusicMode = mode;
        bgmPhase = bgmPulsePhase = bgmHarmonyPhase = bgmDrumPhase = 0.0f;
        bgmStepCounter = 0;
        bgmNoteIndex = -1;
        bgmNoteFreq = 0.0f;
        bgmNoteLen = 0;
        ch2BassIdx = -1;
        ch2BassLen = 0;
        ch2BassFreq = 0.0f;
        ch2BassPhase = 0.0f;
    }

    float renderMusicSample(int mode, int intensity) {
        if (mode == MUSIC_CH2) {
            const int tick = 700;
            if (bgmNoteLen <= 0) {
                bgmNoteIndex = (bgmNoteIndex + 1) % 126;
                bgmNoteLen = CH2_MEL_DUR[bgmNoteIndex] * tick;
                bgmNoteFreq = CH2_MELODY[bgmNoteIndex];
            }
            bgmNoteLen--;
            float total = (float)(CH2_MEL_DUR[bgmNoteIndex] * tick);
            float t = (float)bgmNoteLen / total;
            float env = t < 0.12f ? t / 0.12f : (t > 0.85f ? (1.0f - t) / 0.15f : 0.80f);
            float melody = advancePhase(bgmPhase, bgmNoteFreq) * env * 0.044f;

            if (ch2BassLen <= 0) {
                ch2BassIdx = (ch2BassIdx + 1) % 64;
                ch2BassLen = CH2_BASS_DUR[ch2BassIdx] * tick;
                ch2BassFreq = CH2_BASS[ch2BassIdx];
            }
            ch2BassLen--;
            float bassTotal = (float)(CH2_BASS_DUR[ch2BassIdx] * tick);
            float bassT = (float)ch2BassLen / bassTotal;
            float bassEnv = bassT < 0.10f ? bassT / 0.10f
                : (bassT > 0.80f ? 0.40f + 0.60f * (1.0f - bassT) / 0.20f : 0.85f);
            float bass = advancePhase(ch2BassPhase, ch2BassFreq) * bassEnv * 0.050f;

            // 战斗时逐层加入高八度星光与低频推进，不改变既有 32 秒主题旋律。
            float shimmer = 0.0f, drive = 0.0f;
            if (intensity > 0 && bgmNoteFreq > 0.0f)
                shimmer = advancePhase(bgmHarmonyPhase, bgmNoteFreq * 2.0f) * env * 0.010f * intensity;
            if (intensity > 1) {
                int beat = bgmStepCounter % 22050;
                float beatEnv = beat < 2205 ? 1.0f - beat / 2205.0f : 0.0f;
                drive = advancePhase(bgmDrumPhase, 52.0f + 35.0f * beatEnv) * beatEnv * 0.025f;
            }
            bgmStepCounter++;
            return melody + bass + shimmer + drive;
        }

        const bool boss = mode == MUSIC_BOSS;
        const float* notes = boss ? BOSS_MELODY : BGM_MELODY;
        const int* durations = boss ? BOSS_DUR : BGM_NOTE_DUR;
        const int count = boss ? 16 : 32;
        const int tick = boss ? 450 : 800;
        if (bgmNoteLen <= 0) {
            bgmNoteIndex = (bgmNoteIndex + 1) % count;
            bgmNoteLen = durations[bgmNoteIndex] * tick;
            bgmNoteFreq = notes[bgmNoteIndex];
        }
        bgmNoteLen--;
        float total = (float)(durations[bgmNoteIndex] * tick);
        float t = (float)bgmNoteLen / total;
        float env = t < 0.06f ? t / 0.06f : (t > 0.78f ? (1.0f - t) / 0.22f : 1.0f);
        float melody = advancePhase(bgmPhase, bgmNoteFreq) * env * (boss ? 0.050f : 0.041f);
        float bassFreq = boss ? 65.0f : 55.0f;
        float pulse = 0.55f + 0.45f * sinf(bgmStepCounter * (boss ? 0.0015f : 0.0005f));
        float bass = advancePhase(bgmPulsePhase, bassFreq) * pulse * (boss ? 0.034f : 0.027f);

        float harmony = 0.0f, drum = 0.0f;
        if (boss || intensity > 0)
            harmony = advancePhase(bgmHarmonyPhase, bgmNoteFreq * (boss ? 1.5f : 2.0f)) * env * 0.012f;
        if (boss || intensity > 1) {
            const int beatLen = boss ? 11025 : 22050;
            int beat = bgmStepCounter % beatLen;
            float beatEnv = beat < 1800 ? 1.0f - beat / 1800.0f : 0.0f;
            drum = advancePhase(bgmDrumPhase, 48.0f + 55.0f * beatEnv) * beatEnv * (boss ? 0.034f : 0.020f);
            if (boss && beat > beatLen / 2 && beat < beatLen / 2 + 600)
                drum += noiseSample() * (1.0f - (beat - beatLen / 2) / 600.0f) * 0.010f;
        }
        bgmStepCounter++;
        return melody + bass + harmony + drum;
    }

    void audioCB(Uint8* stream, int len) {
        float* buf = (float*)stream;
        int n = len / (int)sizeof(float);
        memset(buf, 0, len);

        int desiredMode = bossMusicOn.load() ? MUSIC_BOSS : (ch2Bgm.load() ? MUSIC_CH2 : MUSIC_CH1);
        int intensity = musicIntensity.load();
        int bgmVol = bgmVolume.load();
        float voiceDuck = voiceClips.empty() ? 1.0f : 0.38f;
        float bgmScale = bgmVol * bgmVol / 70.0f * voiceDuck;
        const float fadeStep = 1.0f / 8820.0f; // 200 ms fade, avoids pause/phase hard cuts
        for (int i = 0; i < n; ++i) {
            if (desiredMode != currentMusicMode) {
                musicGain -= fadeStep;
                if (musicGain <= 0.0f) {
                    musicGain = 0.0f;
                    resetMusicState(desiredMode); // also prevents cross-score index overrun
                }
            } else if (bgmOff.load()) {
                if (musicGain > 0.0f) musicGain -= fadeStep;
                if (musicGain < 0.0f) musicGain = 0.0f;
            } else if (musicGain < 1.0f) {
                musicGain += fadeStep;
                if (musicGain > 1.0f) musicGain = 1.0f;
            }
            if (currentMusicMode != MUSIC_SILENT && musicGain > 0.0f)
                buf[i] = renderMusicSample(currentMusicMode, intensity) * bgmScale * musicGain;
        }

        float eqGain[3];
        eqGain[0] = std::pow(10.0f, eqLow.load()  / 20.0f);
        eqGain[1] = std::pow(10.0f, eqMid.load()  / 20.0f);
        eqGain[2] = std::pow(10.0f, eqHigh.load() / 20.0f);
        float sfxScale = sfxVolume.load() * sfxVolume.load() / 70.0f;

        for (auto& s : activeSnd) {
            if (s.samplesLeft <= 0) continue;
            int process = n < s.samplesLeft ? n : s.samplesLeft;
            for (int i = 0; i < process; ++i) {
                float t = 1.0f - (float)s.samplesLeft / s.totalSamples;
                float env = 1.0f;
                if (t < 0.02f) env = t / 0.02f;
                else if (t > 0.55f) env = (1.0f - t) / 0.45f;

                float freq = s.freq;
                if (s.type == 3) freq = s.freq + (s.sweepEnd - s.freq) * t;

                s.phase += freq / 44100.0f;
                if (s.phase > 2.0f) s.phase -= 2.0f;

                float smp = 0.0f;
                if (s.type == 2)
                    smp = noiseSample();
                else if (s.type == 1)
                    smp = (sinf(s.phase * 2.0f * M_PI) > 0.0f ? 1.0f : -1.0f) * 0.5f;
                else
                    smp = sinf(s.phase * 2.0f * M_PI);

                buf[i] += smp * env * s.volume * sfxScale * eqGain[s.band];
                s.samplesLeft--;
            }
        }
        // Voice clips (narration / dialogue): mono 44.1k float
        for (auto& v : voiceClips) {
            if (!v.active) continue;
            for (int i = 0; i < n && v.pos < (Uint32)v.samples.size(); ++i, ++v.pos) {
                buf[i] += v.samples[v.pos] * voiceGain;
            }
        }
        voiceClips.erase(std::remove_if(voiceClips.begin(), voiceClips.end(),
            [](const VoiceClip& v){ return v.pos >= v.samples.size(); }), voiceClips.end());
        // Smooth master limiter: retains impact detail where the old hard clip flattened peaks.
        for (int i = 0; i < n; ++i)
            buf[i] = 0.96f * tanhf(buf[i] / 0.96f);
        activeSnd.erase(std::remove_if(activeSnd.begin(), activeSnd.end(),
            [](const ActiveSound& s){ return s.samplesLeft <= 0; }), activeSnd.end());
    }

public:
    AudioEngine() : audioDev(0), bgmVolume(7), sfxVolume(7),
                    eqLow(0), eqMid(0), eqHigh(0), bossMusicOn(false),
                    ch2Bgm(false), bgmOff(false), musicIntensity(0),
                    voiceGain(0.9f), currentMusicMode(MUSIC_CH1), musicGain(0.0f),
                    bgmPhase(0), bgmPulsePhase(0), bgmHarmonyPhase(0), bgmDrumPhase(0),
                    bgmStepCounter(0), bgmNoteIndex(-1), bgmNoteFreq(0), bgmNoteLen(0),
                    noiseState(0x9E3779B9u), ch2BassIdx(-1), ch2BassLen(0),
                    ch2BassFreq(0), ch2BassPhase(0) {
        activeSnd.reserve(48);
        SDL_AudioSpec want;
        SDL_memset(&want, 0, sizeof(want));
        want.freq = 44100;
        want.format = AUDIO_F32;
        want.channels = 1;
        want.samples = 1024;
        want.callback = sdlAudioCB;
        want.userdata = this;
        audioDev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
        if (audioDev) SDL_PauseAudioDevice(audioDev, 0);
    }

    ~AudioEngine() { if (audioDev) SDL_CloseAudioDevice(audioDev); }

    // ==================== Voice clips (narration / dialogue) ====================
    // CRC-32 (IEEE, same as Python zlib.crc32) of a text line → clip filename.
    // The voice pipeline (tools/voice_pipeline.py) writes voice/<lang>/<hash>.wav
    // using the same hash, so game text and audio stay in sync automatically.
    static uint32_t voiceCrc32(const char* s) {
        uint32_t crc = 0xFFFFFFFFu;
        for (; *s; ++s) {
            crc ^= (unsigned char)*s;
            for (int k = 0; k < 8; ++k)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return crc ^ 0xFFFFFFFFu;
    }

    // Loads a PCM WAV (8/16-bit int or 32-bit float, mono/stereo, any rate),
    // normalizes to mono 44.1kHz float and plays it. Returns false when the
    // file is missing (game silently falls back to text / synthetic blips).
    bool playVoice(const char* path) {
        std::vector<float> samples;
        if (!loadWavToMonoFloat(path, samples) || samples.empty()) return false;
        VoiceClip v;
        v.samples.swap(samples);
        v.pos = 0;
        v.active = true;
        if (audioDev) SDL_LockAudioDevice(audioDev);
        voiceClips.push_back(v);
        if (audioDev) SDL_UnlockAudioDevice(audioDev);
        return true;
    }

    void stopVoice() {
        if (audioDev) SDL_LockAudioDevice(audioDev);
        voiceClips.clear();
        if (audioDev) SDL_UnlockAudioDevice(audioDev);
    }
    bool voicePlaying() const {
        if (audioDev) SDL_LockAudioDevice(audioDev);
        bool playing = false;
        for (const auto& v : voiceClips)
            if (v.pos < v.samples.size()) { playing = true; break; }
        if (audioDev) SDL_UnlockAudioDevice(audioDev);
        return playing;
    }

private:
    static bool loadWavToMonoFloat(const char* path, std::vector<float>& out) {
        SDL_AudioSpec spec;
        Uint8* wav = nullptr;
        Uint32 wavLen = 0;
        if (!SDL_LoadWAV(path, &spec, &wav, &wavLen)) return false;
        int srcRate = spec.freq > 0 ? spec.freq : 44100;
        int ch = spec.channels > 0 ? spec.channels : 1;
        Uint16 fmt = spec.format;

        // Decode interleaved samples to float
        std::vector<float> inter;
        inter.reserve(wavLen / 2 + 1);
        int bits = SDL_AUDIO_BITSIZE(fmt);
        bool isFloat = SDL_AUDIO_ISFLOAT(fmt) != 0;
        bool isBig = SDL_AUDIO_ISBIGENDIAN(fmt) != 0;
        if (bits == 16) {
            for (Uint32 i = 0; i + 1 < wavLen; i += 2) {
                short s;
                if (isBig) s = (short)((wav[i] << 8) | wav[i + 1]);
                else       s = (short)((wav[i + 1] << 8) | wav[i]);
                inter.push_back((float)s / 32768.0f);
            }
        } else if (bits == 8) {
            for (Uint32 i = 0; i < wavLen; ++i)
                inter.push_back(((float)wav[i] - 128.0f) / 128.0f);
        } else if (bits == 32 && isFloat) {
            for (Uint32 i = 0; i + 3 < wavLen; i += 4) {
                Uint32 u;
                if (isBig) u = ((Uint32)wav[i] << 24) | ((Uint32)wav[i+1] << 16)
                             | ((Uint32)wav[i+2] << 8) | (Uint32)wav[i+3];
                else       u = ((Uint32)wav[i+3] << 24) | ((Uint32)wav[i+2] << 16)
                             | ((Uint32)wav[i+1] << 8) | (Uint32)wav[i];
                float f;
                memcpy(&f, &u, 4);
                inter.push_back(f);
            }
        }
        SDL_FreeWAV(wav);
        if (inter.empty()) return false;

        // Downmix to mono
        std::vector<float> mono;
        if (ch > 1) {
            mono.resize(inter.size() / ch);
            for (size_t i = 0; i < mono.size(); ++i) {
                float s = 0.0f;
                for (int c = 0; c < ch; ++c) s += inter[i * ch + c];
                mono[i] = s / (float)ch;
            }
        } else {
            mono.swap(inter);
        }

        // Linear resample to 44.1kHz
        const int OUT_RATE = 44100;
        if (srcRate == OUT_RATE) { out.swap(mono); return true; }
        size_t outLen = (size_t)((double)mono.size() * OUT_RATE / (double)srcRate);
        out.resize(outLen);
        for (size_t i = 0; i < outLen; ++i) {
            double t = (double)i * (double)srcRate / (double)OUT_RATE;
            size_t i0 = (size_t)t, i1 = i0 + 1;
            double f = t - (double)i0;
            float a = mono[i0];
            float b = (i1 < mono.size()) ? mono[i1] : a;
            out[i] = a + (b - a) * (float)f;
        }
        return true;
    }

public:
    void setBossMusic(bool on) { bossMusicOn = on; }
    void setCh2Bgm(bool on) { ch2Bgm = on; }
    void setBgmOff(bool off) { bgmOff = off; }
    void setMusicIntensity(int level) {
        if (level < 0) level = 0;
        if (level > 2) level = 2;
        musicIntensity = level;
    }
    void setStartBgm(bool on) { /* [DORMANT] — placeholder: start screen BGM interface */ (void)on; }

    void playSound(float freq, float sweepEnd, int durMs, float vol, int type, int band) {
        if (!audioDev) return;
        SDL_LockAudioDevice(audioDev);
        ActiveSound s;
        s.freq = freq; s.sweepEnd = sweepEnd;
        s.totalSamples = durMs * 44100 / 1000;
        s.samplesLeft = s.totalSamples;
        s.volume = vol; s.type = type; s.phase = 0.0f;
        s.band = band;
        if (activeSnd.size() < 48) {
            activeSnd.push_back(s);
        } else {
            // Preserve loud gameplay cues when a collision burst fills the SFX bus.
            size_t quietest = 0;
            for (size_t i = 1; i < activeSnd.size(); ++i)
                if (activeSnd[i].volume < activeSnd[quietest].volume) quietest = i;
            if (s.volume > activeSnd[quietest].volume) activeSnd[quietest] = s;
        }
        SDL_UnlockAudioDevice(audioDev);
    }

    // 预定义 SFX
    void sndShoot()     { playSound(1200, 600, 28, 0.20f, 3, 2); }
    void sndEnemyShoot(){ playSound(520, 260, 34, 0.09f, 3, 1); }
    void sndHit()       { playSound(350, 0, 35, 0.22f, 1, 1); }
    void sndExplosionSmall() {
        playSound(600, 0, 35, 0.16f, 2, 2);    // high crisp noise crack
        playSound(1400, 0, 16, 0.09f, 0, 2);   // sine tap for "snap"
    }
    void sndExplosionBig()   {
        playSound(500, 0, 48, 0.12f, 2, 2);    // layered high noise
        playSound(200, 0, 40, 0.09f, 2, 1);    // mid noise fill
        playSound(1800, 400, 22, 0.07f, 3, 2); // quick high sweep (shattering)
    }
    void sndShockwave() { playSound(60, 150, 250, 0.30f, 3, 0); }
    void sndShockwaveHit()   { playSound(100, 0, 60, 0.25f, 0, 1); }
    void sndBossHeal()  { playSound(300, 600, 120, 0.22f, 3, 1); }
    void sndBossAbsorb() { playSound(200, 80, 200, 0.25f, 3, 0); }
    void sndBossEntrance() {
        playSound(60, 25, 700, 0.35f, 3, 0);
        playSound(150, 60, 550, 0.25f, 3, 0);
        playSound(280, 160, 350, 0.18f, 3, 0);
        playSound(180, 90, 500, 0.20f, 1, 1);
        playSound(40, 0, 120, 0.30f, 2, 0);
    }
    void sndBaseDamage() { playSound(45, 0, 150, 0.28f, 2, 0); }
    void sndBossHit() {
        playSound(280, 0, 28, 0.20f, 1, 2);
        playSound(80, 0, 35, 0.28f, 1, 0);
    }
    void sndShake()   { playSound(35, 0, 80, 0.12f, 2, 0); }
    void sndCrystalCrush() {
        playSound(2200, 500, 24, 0.15f, 1, 1);   // square sweep: 8-bit crystal shatter
        playSound(800, 0, 18, 0.12f, 3, 2);      // noise crunch: impact texture
        playSound(3600, 1200, 10, 0.06f, 0, 2);  // sine sparkle: bright glass top
    }
    void sndVillainTalk() {
        playSound(3500, 2400, 16, 0.09f, 3, 2);  // high sweep: radar chirp
        playSound(500, 0, 12, 0.05f, 0, 1);       // low sine body: radio depth
    }
    void sndTeletype()  { playSound(2400, 1800, 20, 0.10f, 3, 2); }
    void sndTripleOn()  { playSound(2600, 0, 22, 0.16f, 0, 2);    // crisp metallic ping
                          playSound(3200, 0, 14, 0.10f, 0, 2); }  // harmonic overtone
    void sndTripleCountdown() { playSound(1600, 0, 8, 0.08f, 0, 2); } // short beep
    void sndAllyTalk()  { playSound(3200, 2200, 14, 0.07f, 0, 2); }  // AI copilot: high sine blip
    void sndTowerTalk() { playSound(800, 400, 16, 0.08f, 3, 1); }   // tower AI: mid sweep
    void sndBryssaTalk(){ playSound(1500, 600, 12, 0.07f, 1, 2); }  // human comms: mid-high square
    void sndSystemTalk(){ playSound(1000, 0, 10, 0.05f, 0, 2); }    // system msg: pure sine ping
    void sndMarthaTalk(){ playSound(950, 500, 12, 0.06f, 1, 1); }   // pilot Martha: mid-low square
    void sndMoonwellTalk(){ playSound(1400, 500, 18, 0.07f, 0, 2); } // Moonwell AI: descending sine
    void sndPlayerHit() { // heavy damage impact
        playSound(80, 0, 55, 0.22f, 2, 0);    // low thud
        playSound(200, 0, 40, 0.16f, 1, 1);   // mid impact
        playSound(40, 0, 70, 0.18f, 2, 0);    // deep rumble
    }

    // 纯代码离线渲染巡检：覆盖三套乐谱切换与 48 声部压力混音。
    bool runSynthesisSelfTest() {
        if (!audioDev) {
            printf("[audio] synthesis test: SKIP (no SDL audio device)\n");
            return true;
        }
        SDL_PauseAudioDevice(audioDev, 1);
        SDL_LockAudioDevice(audioDev);
        bool oldBoss = bossMusicOn.load(), oldCh2 = ch2Bgm.load(), oldOff = bgmOff.load();
        int oldIntensity = musicIntensity.load();
        int oldBgmVol = bgmVolume.load(), oldSfxVol = sfxVolume.load();
        int oldLow = eqLow.load(), oldMid = eqMid.load(), oldHigh = eqHigh.load();
        voiceClips.clear();
        activeSnd.clear();
        bgmOff = false;
        musicIntensity = 2;
        bgmVolume = 7; sfxVolume = 7;
        eqLow = 0; eqMid = 0; eqHigh = 0;

        bool ok = true;
        float modeRms[3] = {0.0f, 0.0f, 0.0f};
        std::vector<float> block(1024);
        for (int mode = MUSIC_CH1; mode <= MUSIC_BOSS; ++mode) {
            bossMusicOn = (mode == MUSIC_BOSS);
            ch2Bgm = (mode == MUSIC_CH2);
            double energy = 0.0;
            float peak = 0.0f;
            int measured = 0;
            for (int b = 0; b < 24; ++b) {
                audioCB((Uint8*)block.data(), (int)(block.size() * sizeof(float)));
                if (b >= 20) for (float sample : block) {
                    if (!std::isfinite(sample)) ok = false;
                    float a = fabsf(sample);
                    if (a > peak) peak = a;
                    energy += sample * sample;
                    measured++;
                }
            }
            modeRms[mode - 1] = measured ? sqrtf((float)(energy / measured)) : 0.0f;
            if (modeRms[mode - 1] < 0.005f || peak > 0.961f) ok = false;
            printf("[audio] mode=%s rms=%.4f peak=%.4f\n",
                   mode == MUSIC_CH1 ? "ch1" : (mode == MUSIC_CH2 ? "ch2" : "boss"),
                   modeRms[mode - 1], peak);
        }

        for (int i = 0; i < 48; ++i) {
            ActiveSound s;
            s.freq = 70.0f + i * 31.0f; s.sweepEnd = s.freq * 0.55f;
            s.totalSamples = s.samplesLeft = 8820;
            s.volume = 0.12f + (i % 4) * 0.03f;
            s.type = i % 4; s.band = i % 3; s.phase = 0.0f;
            activeSnd.push_back(s);
        }
        float stressPeak = 0.0f;
        for (int b = 0; b < 4; ++b) {
            audioCB((Uint8*)block.data(), (int)(block.size() * sizeof(float)));
            for (float sample : block) {
                if (!std::isfinite(sample)) ok = false;
                stressPeak = std::max(stressPeak, fabsf(sample));
            }
        }
        if (stressPeak > 0.961f || stressPeak < 0.05f) ok = false;
        printf("[audio] 48-voice stress peak=%.4f limiter=%s\n", stressPeak, ok ? "PASS" : "FAIL");

        activeSnd.clear();
        bossMusicOn = oldBoss; ch2Bgm = oldCh2; bgmOff = oldOff;
        musicIntensity = oldIntensity;
        bgmVolume = oldBgmVol; sfxVolume = oldSfxVol;
        eqLow = oldLow; eqMid = oldMid; eqHigh = oldHigh;
        resetMusicState(MUSIC_CH1);
        musicGain = 0.0f;
        SDL_UnlockAudioDevice(audioDev);
        SDL_PauseAudioDevice(audioDev, 0);
        return ok;
    }

    int getBgmVolume() const { return bgmVolume.load(); }
    int getSfxVolume() const { return sfxVolume.load(); }
    int getEqLow() const { return eqLow.load(); }
    int getEqMid() const { return eqMid.load(); }
    int getEqHigh() const { return eqHigh.load(); }

    static int clampSetting(int value, int low, int high) {
        return value < low ? low : (value > high ? high : value);
    }
    void adjBgmVolume(int d) { bgmVolume = clampSetting(bgmVolume.load() + d, 1, 10); }
    void adjSfxVolume(int d) { sfxVolume = clampSetting(sfxVolume.load() + d, 1, 10); }
    void adjEqLow(int d) { eqLow = clampSetting(eqLow.load() + d, -5, 5); }
    void adjEqMid(int d) { eqMid = clampSetting(eqMid.load() + d, -5, 5); }
    void adjEqHigh(int d) { eqHigh = clampSetting(eqHigh.load() + d, -5, 5); }

    // 设置持久化恢复用（范围与 adj* 相同，越界自动夹紧）
    void setSoundState(int bgm, int sfx, int low, int mid, int high) {
        bgmVolume = clampSetting(bgm, 1, 10);
        sfxVolume = clampSetting(sfx, 1, 10);
        eqLow = clampSetting(low, -5, 5);
        eqMid = clampSetting(mid, -5, 5);
        eqHigh = clampSetting(high, -5, 5);
    }
};

const float AudioEngine::BGM_MELODY[32] = {
    110,130,146,174,196,174,146,130,  110,130,146,174,196,220,196,174,
    130,146,174,196,220,196,174,146,  110,130,146,174,196,174,146,130
};
const int AudioEngine::BGM_NOTE_DUR[32] = {
    12,12,12,12, 8, 8,12,12,  12,12,12,12, 8, 4,12,12,
    12,12,12,12, 8, 8,12,12,  12,12,12,12, 8, 4,16,16
};
const float AudioEngine::BOSS_MELODY[16] = {
    196,220,261,294,330,294,261,220,  261,294,330,392,330,294,261,220
};
const int AudioEngine::BOSS_DUR[16] = {
    6,6,4,4,6,6,4,4,  6,4,4,6,8,6,4,8
};

// Chapter 2 BGM: 32s ambient pad loop — C# major, ~60 BPM
// 126 melody notes (16th notes) + 64 bass notes (8th notes)
const float AudioEngine::CH2_MELODY[126] = {
       0,    0,  415,  415,  277,  415,  277,    0,
       0,    0,    0,    0,  466,  466,  466,    0,
     466,  466,  349,  349,  349,  349,  349,  349,
     349,  349,  466,  466,  311,  311,  466,  466,
    1047,  349,  349,  349,  156,  156,  156,  156,
     349,  262,  175,  349,  175,  349,  175,  175,
       0,    0,    0,    0,  932,  932,  932,    0,
     932,  932,  932, 1047, 1047, 1047,  349,  349,
     349,  349,  349, 1047, 1047,  349,  175,  175,
     175,  175,  932,  932,  932,    0,    0,    0,
       0,    0,    0, 1047, 1047, 1047,  932,  932,
     932,  932,  415,  415,  415,  415,  415,  415,
     415,  415,  175,  175,  175,  175, 1047, 1047,
    1047,  175,  208,  208,  208,  208,  117,  117,
     117,  117,  415,  415,  175,  175,    0,    0,
       0,    0,    0,  415,  415,  277,
};
const int AudioEngine::CH2_MEL_DUR[126] = {
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16, 16,16,16,16,16,16,
};
const float AudioEngine::CH2_BASS[64] = {
     69,  69,  69,  69,  69,  69,  58,  58,
     58,  58,  65,  65,  65,  65,  69,  69,
     69,  69,  78,  78,  78,  78,  87,  87,
     87,  87,  73,  69,  69,  69,  69,  69,
     87,  87,  87,  87,  87,  87,  69,  69,
     69,  69,  69,  69,  87,  87,  69,  69,
     69,  69,  87,  87,  87,  87, 104,  58,
     58,  58,  69,  69,  69,  69,  69,  69,
};
const int AudioEngine::CH2_BASS_DUR[64] = {
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
    32,32,32,32,32,32,32,32, 32,32,32,32,32,32,32,32,
};
