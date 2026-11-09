#pragma once

#include <atomic>

#include "player.h"
#include "types.h"


// ============== AudioEngine 类 ==============
class AudioEngine {
    enum MusicMode { MUSIC_SILENT, MUSIC_CH1, MUSIC_CH2, MUSIC_BOSS, MUSIC_TITLE };

    SDL_AudioDeviceID audioDev;
    std::atomic<int> bgmVolume, sfxVolume, eqLow, eqMid, eqHigh;
    std::atomic<bool> bossMusicOn, ch2Bgm, bgmOff, startBgm;
    std::atomic<int> musicIntensity;
    std::vector<ActiveSound> activeSnd;

    // Demo-style step sequencer voices.  Music is generated inside the audio
    // callback, so these are not game state and are intentionally not saved.
    struct MusicVoice {
        bool active = false;
        int wave;          // 0 sine, 1 triangle, 2 square, 3 saw, 4 filtered noise
        float phase, f0, f1;
        float t, dur, attack, decay, release, gain;
        float lpf, lpCoeff;
    };
    static const int MUSIC_VOICES = 24;
    MusicVoice musicVoices[MUSIC_VOICES];
    int lastSeqStep;
    int teletypeIndex;
    int sphereTickIndex;
    int shardIndex;

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
    float bgmPadPhase0, bgmPadPhase1, bgmPadPhase2;
    float bgmArpPhase, bgmSubPhase, bgmBellPhase;
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

    static float midiHz(int note) {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    // 轻量多波形振荡器：0=正弦 1=三角 2=锯齿 3=窄脉冲。
    static float advanceWave(float& phase, float freq, int wave) {
        phase += freq / 44100.0f;
        if (phase >= 1.0f) phase -= 1.0f;
        switch (wave) {
            case 1: return 1.0f - 4.0f * std::fabs(phase - 0.5f);
            case 2: return 2.0f * phase - 1.0f;
            case 3: return phase < 0.20f ? 1.0f : -1.0f;
            default: return sinf(phase * 2.0f * (float)M_PI);
        }
    }

    float noiseSample() {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return ((noiseState & 0xFFFFu) / 32767.5f) - 1.0f;
    }

    void triggerMusicVoice(int wave, float f0, float f1, float dur,
                           float attack, float decay, float release, float gain,
                           float delay = 0.0f, float lp = 0.5f) {
        int idx = -1;
        for (int i = 0; i < MUSIC_VOICES; ++i)
            if (!musicVoices[i].active) { idx = i; break; }
        if (idx < 0) {
            // 优先换掉最轻、剩余时间最短的音符，保住 bass / pad。
            float best = 1e30f;
            for (int i = 0; i < MUSIC_VOICES; ++i) {
                const MusicVoice& v = musicVoices[i];
                float rem = v.t - v.dur;
                if (rem < 0.0f) rem = 0.0f;
                float score = v.gain + rem * 0.03f;
                if (score < best) { best = score; idx = i; }
            }
        }
        if (idx < 0) return;
        MusicVoice& v = musicVoices[idx];
        v.active = true;
        v.wave = wave;
        v.phase = 0.0f;
        v.f0 = f0; v.f1 = f1;
        v.dur = dur;
        v.attack = attack < 0.001f ? 0.001f : attack;
        v.decay = decay < 0.002f ? 0.002f : decay;
        v.release = release < 0.002f ? 0.002f : release;
        v.gain = gain;
        v.t = -delay;
        v.lpf = 0.0f;
        v.lpCoeff = lp;
        if (v.lpCoeff < 0.01f) v.lpCoeff = 0.01f;
        if (v.lpCoeff > 0.95f) v.lpCoeff = 0.95f;
    }

    float renderMusicVoices() {
        float mix = 0.0f;
        for (int i = 0; i < MUSIC_VOICES; ++i) {
            MusicVoice& v = musicVoices[i];
            if (!v.active) continue;
            v.t += 1.0f / 44100.0f;
            if (v.t < 0.0f) continue;
            float env;
            if (v.t < v.attack) env = v.t / v.attack;
            else env = std::exp(-(v.t - v.attack) / v.decay);
            if (v.t > v.dur) {
                env *= std::exp(-(v.t - v.dur) / v.release);
                if (v.t > v.dur + v.release * 7.0f) { v.active = false; continue; }
            }
            if (env < 0.0004f && v.t > v.attack) { v.active = false; continue; }

            float u = v.t / (v.dur > 0.001f ? v.dur : 0.001f);
            if (u < 0.0f) u = 0.0f;
            if (u > 1.0f) u = 1.0f;
            float freq = v.f0 + (v.f1 - v.f0) * u;
            float smp = 0.0f;
            if (v.wave == 4) {
                noiseState ^= noiseState << 13;
                noiseState ^= noiseState >> 17;
                noiseState ^= noiseState << 5;
                float n = (float)(noiseState & 0xFFFFu) / 32768.0f - 1.0f;
                v.lpf += (n - v.lpf) * v.lpCoeff;
                smp = v.lpf;
            } else {
                v.phase += freq / 44100.0f;
                if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);
                float p = v.phase;
                switch (v.wave) {
                    case 0: smp = std::sin(p * 2.0f * (float)M_PI); break;
                    case 1: smp = 1.0f - 4.0f * std::fabs(p - 0.5f); break;
                    case 2: smp = p < 0.5f ? 1.0f : -1.0f; break;
                    default: smp = 2.0f * p - 1.0f; break;
                }
            }
            mix += smp * env * v.gain;
        }
        return mix;
    }

    // Ported from the First Flight demo sequencer: 92 BPM CH1 and 132 BPM boss.
    void scheduleDemoStep(int seqStep, bool boss, int layer) {
        int step = seqStep & 63;
        int bar = step / 16;
        int local = step % 16;

        if (!boss) {
            static const int BASS[4] = {38, 34, 41, 45};
            static const int ARP[4][4] = {{62, 65, 69, 74}, {58, 62, 65, 70},
                                          {65, 69, 72, 77}, {57, 60, 64, 69}};
            static const int ARPPAT[16] = {0, 1, 2, 3, 1, 2, 3, 2,
                                           0, 2, 1, 3, 2, 1, 0, 1};
            static const int MEL[4] = {0, 0, 81, 77};
            if (local == 0) {
                for (int i = 0; i < 3; ++i) {
                    float f = midiHz(ARP[bar][i]);
                    triggerMusicVoice(0, f, f, 3.2f, 0.75f, 3.5f, 0.4f, 0.014f);
                }
            }
            if (local % 4 == 0)
                triggerMusicVoice(0, midiHz(BASS[bar]), midiHz(BASS[bar]),
                                  0.40f, 0.006f, 0.18f, 0.12f, 0.105f);
            if (local == 6 || local == 14)
                triggerMusicVoice(0, midiHz(BASS[bar] + 7), midiHz(BASS[bar] + 7),
                                  0.25f, 0.006f, 0.12f, 0.10f, 0.045f);
            {
                int note = ARP[bar][ARPPAT[local]];
                triggerMusicVoice(1, midiHz(note), midiHz(note),
                                  0.24f, 0.004f, 0.10f, 0.08f, 0.030f);
                if (local == 8)
                    triggerMusicVoice(1, midiHz(note + 12), midiHz(note + 12),
                                      0.30f, 0.004f, 0.12f, 0.10f, 0.013f);
            }
            if (local == 8 && MEL[bar] > 0) {
                triggerMusicVoice(1, midiHz(MEL[bar]), midiHz(MEL[bar]),
                                  0.55f, 0.01f, 0.25f, 0.25f, 0.028f);
                triggerMusicVoice(1, midiHz(MEL[bar]), midiHz(MEL[bar]),
                                  0.55f, 0.01f, 0.25f, 0.25f, 0.012f, 0.38f);
            }
            if (layer >= 1) {
                if (local == 0 || local == 8)
                    triggerMusicVoice(0, 105.0f, 44.0f, 0.13f, 0.002f, 0.05f, 0.08f, 0.075f);
                if (local % 8 == 4)
                    triggerMusicVoice(1, midiHz(ARP[bar][2] + 12), midiHz(ARP[bar][2] + 12),
                                      0.12f, 0.004f, 0.05f, 0.06f, 0.010f);
            }
            if (layer >= 2) {
                if (local % 2 == 1)
                    triggerMusicVoice(4, 6000.0f, 6000.0f, 0.025f,
                                      0.001f, 0.010f, 0.01f, 0.010f, 0.0f, 0.80f);
                if (local == 4 || local == 12)
                    triggerMusicVoice(1, midiHz(ARP[bar][3] + 12), midiHz(ARP[bar][3] + 12),
                                      0.10f, 0.004f, 0.04f, 0.05f, 0.008f);
            }
        } else {
            static const int OST[16] = {38, 38, 39, 38, 41, 39, 38, 36,
                                        38, 38, 39, 38, 41, 43, 44, 41};
            static const int STAB[4][3] = {{50, 53, 57}, {46, 50, 53},
                                           {44, 47, 50}, {50, 53, 58}};
            static const int HIGH[8] = {74, 73, 74, 77, 76, 74, 70, 69};
            triggerMusicVoice(3, midiHz(OST[local]), midiHz(OST[local]),
                              0.115f, 0.002f, 0.05f, 0.05f, 0.055f);
            if (local == 0) {
                triggerMusicVoice(0, midiHz(50), midiHz(50), 3.4f, 0.5f, 3.5f, 0.4f, 0.020f);
                triggerMusicVoice(0, midiHz(53), midiHz(53), 3.4f, 0.5f, 3.5f, 0.4f, 0.014f);
                triggerMusicVoice(0, midiHz(57), midiHz(57), 3.4f, 0.5f, 3.5f, 0.4f, 0.011f);
            }
            if (local == 2 || local == 10 || (layer >= 2 && (local == 6 || local == 14))) {
                for (int i = 0; i < 3; ++i)
                    triggerMusicVoice(3, midiHz(STAB[bar][i]), midiHz(STAB[bar][i]),
                                      0.13f, 0.003f, 0.05f, 0.06f, 0.017f);
            }
            if (local % 2 == 0)
                triggerMusicVoice(1, midiHz(HIGH[(step / 2) % 8]), midiHz(HIGH[(step / 2) % 8]),
                                  0.14f, 0.004f, 0.06f, 0.06f, 0.020f);
            if (local == 0 || local == 4 || local == 8 || local == 12 ||
                (layer >= 2 && (local == 6 || local == 14)))
                triggerMusicVoice(0, 120.0f, 44.0f, 0.14f, 0.002f, 0.055f, 0.08f, 0.085f);
            if (local == 4 || local == 12)
                triggerMusicVoice(4, 2400.0f, 1200.0f, 0.10f,
                                  0.002f, 0.04f, 0.05f, 0.025f, 0.0f, 0.50f);
            if (layer >= 2 && local % 4 == 2) {
                triggerMusicVoice(1, midiHz(69), midiHz(69), 0.075f,
                                  0.003f, 0.03f, 0.05f, 0.008f);
                triggerMusicVoice(4, 5000.0f, 5000.0f, 0.02f,
                                  0.001f, 0.008f, 0.01f, 0.007f, 0.0f, 0.85f);
            }
        }
    }

    float renderDemoMusic(bool boss, int intensity) {
        double bpm = boss ? 132.0 : 92.0;
        double samplesPerStep = 44100.0 * 60.0 / (bpm * 4.0);
        int seqStep = (int)(bgmStepCounter / samplesPerStep);
        if (seqStep != lastSeqStep) {
            lastSeqStep = seqStep;
            scheduleDemoStep(seqStep, boss, intensity);
        }
        float mix = renderMusicVoices();
        bgmStepCounter++;
        // demo 的原始 voice gain 以 64 声部混音器为基准；这里补偿到当前主总线，
        // 让玩家打开游戏时能明确听到 demo 的旋律层次，同时留足 SFX 动态余量。
        return mix * 1.8f;
    }

    void resetMusicState(int mode) {
        currentMusicMode = mode;
        bgmPhase = bgmPulsePhase = bgmHarmonyPhase = bgmDrumPhase = 0.0f;
        bgmPadPhase0 = bgmPadPhase1 = bgmPadPhase2 = 0.0f;
        bgmArpPhase = bgmSubPhase = bgmBellPhase = 0.0f;
        bgmStepCounter = 0;
        bgmNoteIndex = -1;
        bgmNoteFreq = 0.0f;
        bgmNoteLen = 0;
        ch2BassIdx = -1;
        ch2BassLen = 0;
        ch2BassFreq = 0.0f;
        ch2BassPhase = 0.0f;
        for (int i = 0; i < MUSIC_VOICES; ++i) musicVoices[i].active = false;
        lastSeqStep = -1;
    }

    float renderMusicSample(int mode, int intensity) {
        // ---------------- Title: slow, spacious moonwell terminal ----------------
        if (mode == MUSIC_TITLE) {
            static const float ROOT[4]  = {110.00f, 98.00f, 130.81f, 87.31f};
            static const float THIRD[4] = {130.81f, 116.54f, 164.81f, 110.00f};
            static const float FIFTH[4] = {164.81f, 146.83f, 196.00f, 130.81f};
            static const float BELL[8]  = {440.00f, 523.25f, 659.25f, 392.00f,
                                           493.88f, 587.33f, 698.46f, 440.00f};
            int chordIdx = (bgmStepCounter / (44100 * 4)) % 4;
            int local = bgmStepCounter % (44100 * 4);
            float ct = (float)local / (44100.0f * 4.0f);
            float fade = ct < 0.10f ? ct / 0.10f : (ct > 0.86f ? (1.0f - ct) / 0.14f : 1.0f);
            float drift = 0.82f + 0.18f * sinf(bgmStepCounter * 0.00033f);
            float pad = advanceWave(bgmPadPhase0, ROOT[chordIdx], 0) * 0.020f;
            pad += advanceWave(bgmPadPhase1, THIRD[chordIdx], 1) * 0.010f;
            pad += advanceWave(bgmPadPhase2, FIFTH[chordIdx], 0) * 0.008f;
            int subBeat = bgmStepCounter % 88200;
            float subEnv = subBeat < 14000 ? std::pow(1.0f - subBeat / 14000.0f, 2.0f) : 0.0f;
            float sub = advancePhase(bgmSubPhase, ROOT[chordIdx] * 0.5f) * subEnv * 0.014f;
            int bellIdx = (bgmStepCounter / 44100) % 8;
            int bellLocal = bgmStepCounter % 44100;
            float bellEnv = bellLocal < 26000 ? std::pow(1.0f - bellLocal / 26000.0f, 2.6f) : 0.0f;
            float bell = advancePhase(bgmBellPhase, BELL[bellIdx]) * bellEnv * 0.018f;
            bgmStepCounter++;
            return (pad + sub + bell) * fade * drift;
        }

        // ---------------- Chapter 2: ambient arpeggio / deep-space pad ----------------
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
            float env = t < 0.10f ? t / 0.10f : (t > 0.82f ? (1.0f - t) / 0.18f : 0.82f);
            float melody = advancePhase(bgmPhase, bgmNoteFreq) * env * 0.038f;

            if (ch2BassLen <= 0) {
                ch2BassIdx = (ch2BassIdx + 1) % 64;
                ch2BassLen = CH2_BASS_DUR[ch2BassIdx] * tick;
                ch2BassFreq = CH2_BASS[ch2BassIdx];
            }
            ch2BassLen--;
            float bassTotal = (float)(CH2_BASS_DUR[ch2BassIdx] * tick);
            float bassT = (float)ch2BassLen / bassTotal;
            float bassEnv = bassT < 0.08f ? bassT / 0.08f
                : (bassT > 0.78f ? 0.35f + 0.65f * (1.0f - bassT) / 0.22f : 0.82f);
            float bass = advanceWave(ch2BassPhase, ch2BassFreq, 1) * bassEnv * 0.046f;

            // 固定音高的冷色 pad，随低音呼吸；不改变 32 秒旋律本身。
            static const float PAD[4] = {277.18f, 311.13f, 349.23f, 415.30f};
            float padEnv = 0.70f + 0.30f * sinf(bgmStepCounter * 0.000095f);
            float pad = advancePhase(bgmPadPhase0, PAD[0]) * 0.0060f;
            pad += advancePhase(bgmPadPhase1, PAD[2]) * 0.0045f;
            pad += advanceWave(bgmPadPhase2, 69.0f, 2) * 0.0038f;
            pad *= padEnv;

            float shimmer = 0.0f, drive = 0.0f, tickNoise = 0.0f;
            if (intensity > 0 && bgmNoteFreq > 0.0f) {
                shimmer = advanceWave(bgmHarmonyPhase, bgmNoteFreq * 2.0f, 1) * env * 0.010f * intensity;
                int arpBeat = bgmStepCounter % 7350;
                float arpEnv = arpBeat < 2000 ? std::pow(1.0f - arpBeat / 2000.0f, 2.0f) : 0.0f;
                static const float ARP[4] = {554.37f, 698.46f, 830.61f, 622.25f};
                shimmer += advancePhase(bgmArpPhase, ARP[(bgmStepCounter / 7350) % 4]) * arpEnv * 0.009f;
            }
            if (intensity > 1) {
                int beat = bgmStepCounter % 22050;
                float beatEnv = beat < 2600 ? 1.0f - beat / 2600.0f : 0.0f;
                drive = advancePhase(bgmDrumPhase, 48.0f + 48.0f * beatEnv) * beatEnv * 0.027f;
                int tickBeat = bgmStepCounter % 5512;
                if (tickBeat < 700) tickNoise = noiseSample() * (1.0f - tickBeat / 700.0f) * 0.0055f;
            }
            bgmStepCounter++;
            return melody + bass + pad + shimmer + drive + tickNoise;
        }

        // ---------------- Chapter 1 / Boss: First Flight demo sequencer ----------------
        // 1:1 移植 demo 的 92 BPM CH1 / 132 BPM Boss 乐句，不改旋律与和声进行。
        return renderDemoMusic(mode == MUSIC_BOSS, intensity);
    }

    void audioCB(Uint8* stream, int len) {
        float* buf = (float*)stream;
        int n = len / (int)sizeof(float);
        memset(buf, 0, len);

        int desiredMode = bossMusicOn.load() ? MUSIC_BOSS
                        : (ch2Bgm.load() ? MUSIC_CH2
                        : (startBgm.load() ? MUSIC_TITLE : MUSIC_CH1));
        int intensity = musicIntensity.load();
        int bgmVol = bgmVolume.load();
        float voiceDuck = voiceClips.empty() ? 1.0f : 0.38f;
        float bgmScale = bgmVol * bgmVol / 60.0f * voiceDuck;
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
                // 快速 attack + 指数式尾音，比旧的线性 env 更像真实冲击/拨弦。
                float env = (t < 0.012f) ? (t / 0.012f) : std::pow(1.0f - t, 1.45f);
                if (env < 0.0f) env = 0.0f;

                float freq = s.freq;
                bool sweep = s.sweepEnd > 1.0f && std::fabs(s.sweepEnd - s.freq) > 0.5f;
                if (sweep) freq = s.freq + (s.sweepEnd - s.freq) * t;
                if (freq < 20.0f) freq = 20.0f;
                if (freq > 12000.0f) freq = 12000.0f;

                s.phase += freq / 44100.0f;
                if (s.phase > 2.0f) s.phase -= 2.0f;
                float ph = s.phase * 2.0f * (float)M_PI;
                float smp = 0.0f;
                switch (s.type) {
                    case 1:
                        smp = (sinf(ph) > 0.0f ? 1.0f : -1.0f) * 0.48f;
                        break;
                    case 2:
                        smp = noiseSample();
                        break;
                    case 4: {
                        float p = s.phase;
                        smp = 4.0f * std::fabs(p - 0.5f) - 1.0f;
                        break;
                    }
                    case 5:
                        smp = (2.0f * s.phase - 1.0f) * 0.72f;
                        break;
                    case 6:
                        s.filterLP += 0.09f * (noiseSample() - s.filterLP);
                        smp = s.filterLP * 2.2f;
                        break;
                    case 7: {
                        float p = s.phase;
                        smp = (p < 0.22f ? 1.0f : -1.0f) * 0.42f;
                        break;
                    }
                    default:
                        smp = sinf(ph);
                        break;
                }
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
                    ch2Bgm(false), bgmOff(false), startBgm(false), musicIntensity(0),
                    lastSeqStep(-1), teletypeIndex(0), sphereTickIndex(0), shardIndex(0),
                    voiceGain(0.9f), currentMusicMode(MUSIC_CH1), musicGain(0.0f),
                    bgmPhase(0), bgmPulsePhase(0), bgmHarmonyPhase(0), bgmDrumPhase(0),
                    bgmPadPhase0(0), bgmPadPhase1(0), bgmPadPhase2(0),
                    bgmArpPhase(0), bgmSubPhase(0), bgmBellPhase(0),
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
    void setStartBgm(bool on) { startBgm = on; }

    void playSound(float freq, float sweepEnd, int durMs, float vol, int type, int band) {
        if (!audioDev) return;
        SDL_LockAudioDevice(audioDev);
        ActiveSound s;
        s.freq = freq; s.sweepEnd = sweepEnd;
        s.totalSamples = durMs * 44100 / 1000;
        s.samplesLeft = s.totalSamples;
        s.volume = vol; s.type = type; s.phase = 0.0f;
        s.filterLP = 0.0f;
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
    void sndMenuMove() {
        playSound(1500, 1360, 16, 0.034f, 4, 2);
    }
    void sndMenuConfirm() {
        playSound(880, 1480, 34, 0.060f, 4, 1);
        playSound(2500, 3200, 18, 0.018f, 0, 2);
    }
    void sndMenuBack() {
        playSound(1260, 760, 28, 0.042f, 4, 1);
    }
    void sndMenuDenied() {
        playSound(220, 180, 42, 0.055f, 5, 1);
    }
    void sndShoot()     {
        playSound(1500, 760, 26, 0.15f, 3, 2);
        playSound(3100, 1700, 11, 0.038f, 4, 2);
        playSound(180, 90, 18, 0.025f, 0, 1);
    }
    void sndEnemyShoot(){
        playSound(620, 280, 38, 0.075f, 3, 1);
        playSound(230, 160, 26, 0.028f, 5, 1);
    }
    void sndHit()       {
        playSound(420, 260, 30, 0.15f, 1, 1);
        playSound(2400, 900, 13, 0.045f, 2, 2);
    }
    void sndExplosionSmall() {
        playSound(2400, 380, 30, 0.10f, 6, 2);
        playSound(780, 180, 24, 0.11f, 2, 2);
        playSound(160, 70, 38, 0.075f, 0, 1);
    }
    void sndExplosionBig()   {
        playSound(1800, 120, 52, 0.10f, 6, 2);
        playSound(520, 90, 46, 0.10f, 2, 2);
        playSound(145, 36, 62, 0.14f, 0, 0);
        playSound(1800, 400, 20, 0.055f, 3, 2);
    }
    void sndShockwave() {
        // D 小调的低频能量脉冲：根音 + 五度 + 八度，外加很轻的空气噪声。
        // 不使用刺耳高频扫频，让基地光波听起来更宽、更深、更有重量。
        playSound(73.42f, 36.71f, 430, 0.26f, 0, 0);
        playSound(110.00f, 55.00f, 360, 0.13f, 0, 1);
        playSound(146.83f, 73.42f, 300, 0.080f, 4, 1);
        playSound(220.00f, 110.00f, 280, 0.040f, 4, 2);
        playSound(1200.0f, 450.0f, 24, 0.018f, 2, 2);
    }
    void sndShockwaveHit()   {
        playSound(146.83f, 110.0f, 45, 0.10f, 4, 1);
        playSound(880.0f, 1200.0f, 16, 0.018f, 4, 2);
    }
    void sndBossHeal()  {
        playSound(260, 720, 150, 0.17f, 3, 1);
        playSound(520, 1040, 120, 0.055f, 4, 2);
    }
    void sndBossAbsorb() {
        playSound(210, 54, 220, 0.21f, 3, 0);
        playSound(700, 130, 170, 0.040f, 6, 1);
    }
    void sndBossEntrance() {
        playSound(64, 24, 760, 0.34f, 3, 0);
        playSound(170, 55, 580, 0.20f, 5, 0);
        playSound(420, 120, 430, 0.11f, 6, 0);
        playSound(220, 90, 300, 0.13f, 1, 1);
        playSound(44, 0, 150, 0.24f, 2, 0);
    }
    void sndBaseDamage() { playSound(45, 0, 150, 0.28f, 2, 0); }
    void sndBossHit() {
        playSound(340, 230, 28, 0.13f, 1, 2);
        playSound(2900, 900, 14, 0.045f, 2, 2);
        playSound(82, 0, 38, 0.24f, 0, 0);
    }
    void sndShake()   {
        playSound(46.0f, 32.0f, 105, 0.11f, 0, 0);
        playSound(135.0f, 70.0f, 85, 0.040f, 6, 1);
    }
    void sndCrystalCrush() {
        // 玻璃碎裂的柔和版：暖三角波主体 + 少量空气噪声 + 高频晶闪，
        // 去掉旧版刺耳的 2.2 kHz 方波扫频。
        playSound(1760.0f, 1180.0f, 18, 0.046f, 4, 2);
        playSound(520.0f, 220.0f, 22, 0.030f, 6, 2);
        playSound(3200.0f, 1800.0f, 9, 0.010f, 0, 2);
    }
    void sndVillainTalk() {
        playSound(3500, 2400, 16, 0.09f, 3, 2);  // high sweep: radar chirp
        playSound(500, 0, 12, 0.05f, 0, 1);       // low sine body: radio depth
    }
    void sndTeletype()  {
        // 机械键盘式软"嗒"：三角波木质感 + 很轻的低频体 + 4ms 噪声齿。
        // 四个音高轮换，避免连续台词变成同一条高频蜂鸣。
        // D 小调内的四个软木鱼音高，与第一章 demo BGM 的调性一致。
        static const float TICK[4] = {293.66f, 349.23f, 440.00f, 349.23f};
        int k = teletypeIndex++ & 3;
        playSound(TICK[k], TICK[k] * 0.90f, 12, 0.028f, 4, 2);
        playSound(146.83f, 130.81f, 9, 0.010f, 0, 1);
        playSound(3100.0f, 0.0f, 4, 0.006f, 6, 2);
    }
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
        playSound(90, 42, 70, 0.24f, 3, 0);
        playSound(1500, 220, 30, 0.085f, 6, 2);
        playSound(210, 110, 46, 0.11f, 1, 1);
        playSound(42, 0, 85, 0.18f, 2, 0);
    }

    // ==================== Chapter 2 detail cues ====================
    void sndSphereAppear() {
        playSound(58.0f, 42.0f, 900, 0.22f, 0, 0);
        playSound(116.0f, 87.0f, 760, 0.090f, 4, 1);
        playSound(232.0f, 174.0f, 620, 0.025f, 6, 2);
        playSound(464.0f, 348.0f, 500, 0.012f, 0, 2);
    }
    void sndSphereActivateTick() {
        static const float TONE[6] = {587.33f, 698.46f, 880.00f,
                                      698.46f, 587.33f, 440.00f};
        int k = sphereTickIndex++ % 6;
        playSound(TONE[k], TONE[k] * 0.94f, 24, 0.018f, 4, 2);
        playSound(1800.0f, 2400.0f, 8, 0.006f, 6, 2);
    }
    void sndSphereLockOn() {
        playSound(146.83f, 110.00f, 220, 0.070f, 4, 1);
        playSound(220.00f, 146.83f, 250, 0.050f, 0, 1);
        playSound(293.66f, 220.00f, 280, 0.040f, 0, 2);
        playSound(440.00f, 293.66f, 300, 0.020f, 1, 2);
    }
    void sndSphereCrystalHit() {
        playSound(1320.0f, 780.0f, 20, 0.030f, 4, 2);
        playSound(260.0f, 180.0f, 26, 0.025f, 0, 1);
        playSound(2600.0f, 1100.0f, 8, 0.008f, 6, 2);
    }
    void sndGhostDrift() {
        playSound(392.0f, 110.0f, 1350, 0.110f, 3, 1);
        playSound(196.0f, 55.0f, 1500, 0.095f, 0, 0);
        playSound(700.0f, 180.0f, 1100, 0.024f, 6, 2);
        playSound(1568.0f, 784.0f, 800, 0.008f, 0, 2);
    }
    void sndGhostVanish() {
        playSound(220.0f, 880.0f, 720, 0.055f, 3, 2);
        playSound(440.0f, 1760.0f, 620, 0.018f, 4, 2);
        playSound(55.0f, 33.0f, 500, 0.075f, 0, 0);
    }
    void sndDebrisLand() {
        playSound(146.83f, 110.0f, 32, 0.055f, 4, 1);
        playSound(1100.0f, 420.0f, 10, 0.012f, 6, 2);
    }
    void sndDebrisRumble() {
        playSound(55.0f, 35.0f, 150, 0.100f, 0, 0);
        playSound(120.0f, 70.0f, 130, 0.035f, 6, 1);
    }
    void sndShardLaunch() {
        static const float P[4] = {1760.0f, 2080.0f, 1480.0f, 2340.0f};
        int k = shardIndex++ & 3;
        playSound(220.0f, 880.0f, 34, 0.032f, 3, 2);
        playSound(P[k], P[k] * 0.55f, 18, 0.012f, 4, 2);
    }
    void sndShardSwarm() {
        playSound(80.0f, 220.0f, 600, 0.12f, 3, 1);
        playSound(300.0f, 1200.0f, 500, 0.038f, 6, 2);
        playSound(110.0f, 70.0f, 700, 0.045f, 0, 0);
    }
    void sndBossEnrage() {
        playSound(60.0f, 42.0f, 700, 0.22f, 0, 0);
        playSound(150.0f, 260.0f, 400, 0.055f, 5, 1);
        playSound(600.0f, 1200.0f, 300, 0.022f, 4, 2);
    }
    void sndShieldHit() {
        playSound(1400.0f, 900.0f, 18, 0.025f, 4, 2);
        playSound(250.0f, 180.0f, 30, 0.040f, 0, 1);
        playSound(2800.0f, 2200.0f, 7, 0.008f, 6, 2);
    }
    void sndAbsorbCharge() {
        static const float TONE[4] = {440.0f, 554.37f, 659.25f, 880.0f};
        int k = sphereTickIndex++ & 3;
        playSound(TONE[k], TONE[k] * 1.12f, 26, 0.020f, 0, 2);
        playSound(50.0f, 38.0f, 35, 0.020f, 0, 0);
    }
    void sndAlienWarp() {
        playSound(260.0f, 900.0f, 24, 0.022f, 3, 2);
        playSound(80.0f, 120.0f, 30, 0.014f, 0, 1);
    }
    void sndAlienEscape() {
        playSound(700.0f, 180.0f, 46, 0.032f, 3, 1);
    }
    void sndWardenBurst() {
        playSound(92.0f, 48.0f, 190, 0.095f, 0, 0);
        playSound(440.0f, 900.0f, 200, 0.025f, 4, 2);
    }
    void sndWardenAim() {
        playSound(880.0f, 1180.0f, 85, 0.036f, 4, 2);
        playSound(440.0f, 660.0f, 105, 0.025f, 0, 1);
    }
    void sndWardenSpiral() {
        playSound(220.0f, 660.0f, 260, 0.030f, 3, 2);
        playSound(660.0f, 330.0f, 210, 0.014f, 6, 2);
    }
    void sndTransitionSwell() {
        playSound(190.0f, 760.0f, 500, 0.040f, 3, 1);
        playSound(85.0f, 48.0f, 600, 0.055f, 0, 0);
        playSound(380.0f, 1100.0f, 380, 0.018f, 6, 2);
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
        bool oldStart = startBgm.load();
        int oldIntensity = musicIntensity.load();
        int oldBgmVol = bgmVolume.load(), oldSfxVol = sfxVolume.load();
        int oldLow = eqLow.load(), oldMid = eqMid.load(), oldHigh = eqHigh.load();
        voiceClips.clear();
        activeSnd.clear();
        bgmOff = false;
        startBgm = false;   // 巡检必须精确测量三套战斗乐谱，而不是标题主题
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

        // 标题主题单独巡检：游戏从菜单启动时走这条路径，不能被三套战斗乐谱
        // 的回归检查遗漏。
        {
            bossMusicOn = false; ch2Bgm = false; startBgm = true;
            double energy = 0.0;
            float peak = 0.0f;
            int measured = 0;
            for (int b = 0; b < 28; ++b) {
                audioCB((Uint8*)block.data(), (int)(block.size() * sizeof(float)));
                if (b >= 22) for (float sample : block) {
                    if (!std::isfinite(sample)) ok = false;
                    float a = fabsf(sample);
                    if (a > peak) peak = a;
                    energy += sample * sample;
                    measured++;
                }
            }
            float titleRms = measured ? sqrtf((float)(energy / measured)) : 0.0f;
            if (titleRms < 0.005f || peak > 0.961f) ok = false;
            printf("[audio] mode=title rms=%.4f peak=%.4f\n", titleRms, peak);
            startBgm = false;
        }

        for (int i = 0; i < 48; ++i) {
            ActiveSound s;
            s.freq = 70.0f + i * 31.0f; s.sweepEnd = s.freq * 0.55f;
            s.totalSamples = s.samplesLeft = 8820;
            s.volume = 0.12f + (i % 4) * 0.03f;
            s.type = i % 4; s.band = i % 3; s.phase = 0.0f; s.filterLP = 0.0f;
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
        bossMusicOn = oldBoss; ch2Bgm = oldCh2; bgmOff = oldOff; startBgm = oldStart;
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
