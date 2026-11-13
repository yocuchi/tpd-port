// Secuenciador RSEQ (NW4R, bytecode tipo SSEQ) + sintetizador por muestras con envolvente ADSR.
#pragma once
#include "rwav.hpp"
#include "rbnk.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace snd {

static const int OUT_RATE = 32000;
static const int ENV_RATE = 192;          // actualizaciones de envolvente por segundo
static const int ENV_MIN = -92544;        // silencio (unidades DS: 1/128 cB)

inline float envAmp(int env) { return env <= ENV_MIN ? 0.0f : powf(10.0f, env / 25600.0f); }
inline int sustainLevel(int s) { return s <= 0 ? ENV_MIN : s >= 127 ? 0 : (int)(25600.0 * log10(s / 127.0)); }
inline int decayRate(int d) { return d == 127 ? 0xFFFF : d == 126 ? 0x3C00 : d < 50 ? d * 2 + 1 : 0x1E00 / (126 - d); }
inline int attackCoef(int a) { static const uint8_t T[19] = {0x00, 0x01, 0x05, 0x0E, 0x1A, 0x26, 0x33, 0x3F, 0x49, 0x54, 0x5C, 0x64, 0x6D, 0x74, 0x7B, 0x7F, 0x84, 0x89, 0x8F}; return a >= 109 ? T[127 - a] : 255 - a; }

struct Voice {
    bool active = false; std::shared_ptr<Sample> smp; double pos = 0; double step = 1; int track = -1; int key = 60, vel = 127; int prio = 64; uint32_t age = 0;
    int env = ENV_MIN; int phase = 0;  // 0 attack 1 decay 2 sustain 3 release
    int attack, decay, sustain, release; bool released = false; float volL = 0, volR = 0; float baseStep = 1; int lengthTicks = 0; bool tie = false;
    int modDepth = 0, modSpeed = 16, modDelay = 0, modType = 0; double modPhase = 0; int modTimer = 0; float bend = 0; float sweep = 0;
    void keyOff() { if (!released) { released = true; phase = 3; } }
    void updateEnv() {
        if (phase == 0) { env = (int)((long long)env * attackCoef(attack) / 255); if (env >= -1) { env = 0; phase = 1; } }
        else if (phase == 1) { int sl = sustainLevel(sustain); env -= decayRate(decay); if (env <= sl) { env = sl; phase = 2; } }
        else if (phase == 3) { env -= decayRate(release); if (env <= ENV_MIN) { env = ENV_MIN; active = false; } }
    }
};

struct Track {
    bool active = false; uint32_t pc = 0; int wait = 0; bool noteWait = true, tie = false, mono = false, mute = false;
    int vol = 127, vol2 = 127, pan = 64, prio = 64, transpose = 0, program = 0, bendRange = 2, bend = 0;
    int attack = -1, decay = -1, sustain = -1, release = -1; int modDepth = 0, modSpeed = 16, modDelay = 0, modType = 0;
    uint32_t callStack[4]; int callDepth = 0; uint32_t loopStart[4]; int loopCount[4]; int loopDepth = 0;
    bool condFlag = true; int16_t vars[16] = {};
};

class Sequencer {
public:
    const uint8_t* seq = nullptr; size_t seqLen = 0; uint32_t base = 0; const Bank* bank = nullptr; WaveArc* waves = nullptr;
    Track tracks[16]; Voice voices[48]; int16_t gvars[16] = {}; int tempo = 120, timebase = 48; int mainVol = 127; bool playing = false; bool loopedForever = false;
    double tickAcc = 0; double envAcc = 0; uint32_t ageCounter = 0; uint32_t seed = 12345; float masterGain = 1.0f; bool finished() const { return !playing; }

    bool start(const uint8_t* rseq, size_t n, uint32_t offset, const Bank* b, WaveArc* w, uint32_t allocTrack) {
        if (n < 0x20 || memcmp(rseq, "RSEQ", 4)) return false; uint32_t dataOff = be32(rseq + 16); uint32_t codeBase = be32(rseq + dataOff + 8);
        seq = rseq + dataOff + codeBase; seqLen = n - dataOff - codeBase; bank = b; waves = w;
        for (auto& t : tracks) t = Track(); for (auto& v : voices) v = Voice();
        tracks[0].active = true; tracks[0].pc = offset; (void)allocTrack; tempo = 120; timebase = 48; playing = true; tickAcc = 0; return true;
    }
    void stop() { playing = false; for (auto& v : voices) v.active = false; }
    void release() { for (auto& v : voices) v.keyOff(); playing = false; }

    // ---- lectura ----
    uint8_t rd8(uint32_t& pc) { return pc < seqLen ? seq[pc++] : 0xff; }
    int16_t rd16(uint32_t& pc) { int16_t v = (int16_t)(seq[pc] << 8 | seq[pc + 1]); pc += 2; return v; }
    uint32_t rd24(uint32_t& pc) { uint32_t v = (uint32_t)seq[pc] << 16 | seq[pc + 1] << 8 | seq[pc + 2]; pc += 3; return v; }
    uint32_t rdVar(uint32_t& pc) { uint32_t v = 0; for (int i = 0; i < 5; i++) { uint8_t b = rd8(pc); v = (v << 7) | (b & 0x7f); if (!(b & 0x80)) break; } return v; }
    uint32_t rnd() { seed = seed * 1664525u + 1013904223u; return seed >> 8; }
    int16_t& var(Track& t, int i) { return i < 16 ? gvars[i & 15] : t.vars[(i - 16) & 15]; }

    // ---- ejecución de una pista hasta que tenga que esperar ----
    void runTrack(int ti) {
        Track& t = tracks[ti];
        int guard = 0;
        while (t.active && t.wait <= 0 && guard++ < 10000) {
            uint8_t cmd = rd8(t.pc);
            bool useVar = false, useRand = false, cond = false;
            if (cmd == 0xa0) { useRand = true; cmd = rd8(t.pc); } else if (cmd == 0xa1) { useVar = true; cmd = rd8(t.pc); } else if (cmd == 0xa2) { cond = true; cmd = rd8(t.pc); }
            auto argVar = [&](uint32_t& pc) -> int { if (useVar) return var(t, rd8(pc)); if (useRand) { int16_t a = rd16(pc), b = rd16(pc); if (b < a) std::swap(a, b); return a + (int)(rnd() % (uint32_t)(b - a + 1)); } return rdVar(pc); };
            auto arg8 = [&](uint32_t& pc) -> int { if (useVar) return var(t, rd8(pc)); if (useRand) { int16_t a = rd16(pc), b = rd16(pc); if (b < a) std::swap(a, b); return a + (int)(rnd() % (uint32_t)(b - a + 1)); } return rd8(pc); };
            auto arg16 = [&](uint32_t& pc) -> int { if (useVar) return var(t, rd8(pc)); if (useRand) { int16_t a = rd16(pc), b = rd16(pc); if (b < a) std::swap(a, b); return a + (int)(rnd() % (uint32_t)(b - a + 1)); } return rd16(pc); };
            bool skip = cond && !t.condFlag;
            if (cmd < 0x80) {  // nota
                int vel = rd8(t.pc); int len = argVar(t.pc);
                if (!skip && !t.mute) noteOn(ti, cmd + t.transpose, vel, len);
                if (t.noteWait && !skip) t.wait = len;
                continue;
            }
            switch (cmd) {
            case 0x80: { int w = argVar(t.pc); if (!skip) t.wait = w; break; }
            case 0x81: { int p = argVar(t.pc); if (!skip) t.program = p & 0xffff; break; }
            case 0x88: { int tr = rd8(t.pc); uint32_t off = rd24(t.pc); if (!skip && tr < 16) { tracks[tr] = Track(); tracks[tr].active = true; tracks[tr].pc = off; } break; }
            case 0x89: { uint32_t off = rd24(t.pc); if (!skip) t.pc = off; break; }
            case 0x8a: { uint32_t off = rd24(t.pc); if (!skip && t.callDepth < 4) { t.callStack[t.callDepth++] = t.pc; t.pc = off; } break; }
            case 0xb0: { int v = arg8(t.pc); if (!skip) timebase = v; break; }
            case 0xb1: arg8(t.pc); break;  // env_hold
            case 0xb2: { int v = arg8(t.pc); if (!skip) t.mono = v != 0; break; }
            case 0xb3: arg8(t.pc); break;  // vel_range
            case 0xb4: case 0xb5: arg8(t.pc); break;
            case 0xc0: { int v = arg8(t.pc); if (!skip) t.pan = v; break; }
            case 0xc1: { int v = arg8(t.pc); if (!skip) t.vol = v; break; }
            case 0xc2: { int v = arg8(t.pc); if (!skip) mainVol = v; break; }
            case 0xc3: { int v = arg8(t.pc); if (!skip) t.transpose = (int8_t)v; break; }
            case 0xc4: { int v = arg8(t.pc); if (!skip) { t.bend = (int8_t)v; for (auto& vc : voices) if (vc.active && vc.track == ti) vc.bend = t.bend * t.bendRange / 128.0f; } break; }
            case 0xc5: { int v = arg8(t.pc); if (!skip) t.bendRange = v; break; }
            case 0xc6: { int v = arg8(t.pc); if (!skip) t.prio = v; break; }
            case 0xc7: { int v = arg8(t.pc); if (!skip) t.noteWait = v != 0; break; }
            case 0xc8: { int v = arg8(t.pc); if (!skip) { t.tie = v != 0; if (!t.tie) for (auto& vc : voices) if (vc.active && vc.track == ti && vc.tie) vc.keyOff(); } break; }
            case 0xc9: arg8(t.pc); break;  // portamento (no implementado)
            case 0xca: { int v = arg8(t.pc); if (!skip) t.modDepth = v; break; }
            case 0xcb: { int v = arg8(t.pc); if (!skip) t.modSpeed = v; break; }
            case 0xcc: { int v = arg8(t.pc); if (!skip) t.modType = v; break; }
            case 0xcd: arg8(t.pc); break;
            case 0xce: arg8(t.pc); break;
            case 0xcf: arg8(t.pc); break;
            case 0xd0: { int v = arg8(t.pc); if (!skip) t.attack = v; break; }
            case 0xd1: { int v = arg8(t.pc); if (!skip) t.decay = v; break; }
            case 0xd2: { int v = arg8(t.pc); if (!skip) t.sustain = v; break; }
            case 0xd3: { int v = arg8(t.pc); if (!skip) t.release = v; break; }
            case 0xd4: { int v = arg8(t.pc); if (!skip && t.loopDepth < 4) { t.loopStart[t.loopDepth] = t.pc; t.loopCount[t.loopDepth] = v; t.loopDepth++; } break; }
            case 0xd5: { int v = arg8(t.pc); if (!skip) t.vol2 = v; break; }
            case 0xd6: arg8(t.pc); break;
            case 0xd7: arg8(t.pc); break;
            case 0xd8: arg8(t.pc); break;
            case 0xd9: case 0xda: case 0xdb: case 0xdc: arg8(t.pc); break;
            case 0xdd: { int v = arg8(t.pc); if (!skip) t.pan = v; break; }
            case 0xde: { int v = arg8(t.pc); if (!skip) t.mute = v != 0; break; }
            case 0xdf: arg8(t.pc); break;
            case 0xe0: { int v = arg16(t.pc); if (!skip) t.modDelay = v; break; }
            case 0xe1: { int v = arg16(t.pc); if (!skip && v > 0) tempo = v; break; }
            case 0xe3: { int v = arg16(t.pc); if (!skip) for (auto& vc : voices) if (vc.active && vc.track == ti) vc.sweep = v / 64.0f; break; }
            case 0xf0: {  // operaciones con variables
                uint8_t op = rd8(t.pc); int vi = rd8(t.pc); int16_t val = useVar ? var(t, rd8(t.pc)) : useRand ? (int16_t)arg16(t.pc) : rd16(t.pc); if (skip) break; int16_t& v = var(t, vi);
                switch (op) { case 0x80: v = val; break; case 0x81: v += val; break; case 0x82: v -= val; break; case 0x83: v *= val; break; case 0x84: if (val) v /= val; break; case 0x85: v = val >= 0 ? v << val : v >> -val; break;
                    case 0x86: v = val ? (int16_t)(rnd() % (uint32_t)std::abs(val) * (val < 0 ? -1 : 1)) : 0; break; case 0x87: v &= val; break; case 0x88: v |= val; break; case 0x89: v ^= val; break; case 0x8a: v = ~val; break; case 0x8b: if (val) v %= val; break;
                    case 0x90: t.condFlag = v == val; break; case 0x91: t.condFlag = v >= val; break; case 0x92: t.condFlag = v > val; break; case 0x93: t.condFlag = v <= val; break; case 0x94: t.condFlag = v < val; break; case 0x95: t.condFlag = v != val; break; }
                break; }
            case 0xfc: { if (!skip && t.loopDepth > 0) { int d = t.loopDepth - 1; if (t.loopCount[d] == 0) t.pc = t.loopStart[d]; else if (--t.loopCount[d] > 0) t.pc = t.loopStart[d]; else t.loopDepth--; } break; }
            case 0xfd: { if (!skip) { if (t.callDepth > 0) t.pc = t.callStack[--t.callDepth]; else t.active = false; } break; }
            case 0xfe: rd16(t.pc); break;
            case 0xff: t.active = false; for (auto& vc : voices) if (vc.active && vc.track == ti && !vc.tie) {} break;
            default: t.active = false; break;
            }
        }
    }

    void noteOn(int ti, int key, int vel, int len) {
        Track& t = tracks[ti]; if (key < 0) key = 0; if (key > 127) key = 127;
        if (t.tie) {  // ligadura: reutiliza la voz de la pista cambiando la altura
            for (auto& v : voices) if (v.active && v.track == ti && v.tie && !v.released) { v.key = key; v.baseStep = (float)(v.smp->rate / (double)OUT_RATE * pow(2.0, (key - v.key + 0) / 12.0)); v.lengthTicks = len; return; }
        }
        InstParam ip = bank ? bank->lookup(t.program, key, vel) : InstParam(); if (!ip.valid || !waves) return;
        auto smp = waves->get(ip.wave); if (!smp) return;
        if (t.mono) for (auto& v : voices) if (v.active && v.track == ti) v.keyOff();
        Voice* best = nullptr; for (auto& v : voices) if (!v.active) { best = &v; break; }
        if (!best) { for (auto& v : voices) if (!best || v.prio < best->prio || (v.prio == best->prio && v.age < best->age)) best = &v; if (best->prio > t.prio) return; }
        Voice& v = *best; v = Voice(); v.active = true; v.smp = smp; v.track = ti; v.key = key; v.vel = vel; v.prio = t.prio; v.age = ageCounter++;
        v.attack = t.attack >= 0 ? t.attack : ip.attack; v.decay = t.decay >= 0 ? t.decay : ip.decay; v.sustain = t.sustain >= 0 ? t.sustain : ip.sustain; v.release = t.release >= 0 ? t.release : ip.release;
        v.env = ENV_MIN; v.phase = 0; if (v.attack >= 127) { v.env = 0; v.phase = 1; }
        v.baseStep = (float)(smp->rate / (double)OUT_RATE * pow(2.0, (key - ip.origKey) / 12.0) * ip.pitch);
        v.bend = t.bend * t.bendRange / 128.0f; v.tie = t.tie; v.lengthTicks = len; v.modDepth = t.modDepth; v.modSpeed = t.modSpeed; v.modDelay = t.modDelay; v.modType = t.modType;
        float amp = (t.vol / 127.0f) * (t.vol2 / 127.0f) * (mainVol / 127.0f) * (ip.volume / 127.0f) * (vel / 127.0f);
        int pan = t.pan + (ip.pan - 64); if (pan < 0) pan = 0; if (pan > 127) pan = 127; float p = pan / 127.0f;
        v.volL = amp * cosf(p * 1.5707963f); v.volR = amp * sinf(p * 1.5707963f);
    }

    // avanza un tick del secuenciador
    void tick() {
        for (int i = 0; i < 16; i++) { Track& t = tracks[i]; if (!t.active) continue; if (t.wait > 0) t.wait--; if (t.wait <= 0) runTrack(i); }
        for (auto& v : voices) if (v.active && !v.released && v.lengthTicks > 0) { if (--v.lengthTicks == 0 && !v.tie) v.keyOff(); }
        bool any = false; for (auto& t : tracks) any = any || t.active; bool voicesOn = false; for (auto& v : voices) voicesOn = voicesOn || v.active;
        if (!any && !voicesOn) playing = false;
    }

    // mezcla n muestras estéreo (float, suma sobre out)
    void render(float* out, int n) {
        if (!playing) return;
        double tickLen = 60.0 / (tempo * (double)timebase) * OUT_RATE;  // muestras por tick
        int done = 0;
        while (done < n) {
            if (tickAcc <= 0) { tick(); tickAcc += tickLen; tickLen = 60.0 / (tempo * (double)timebase) * OUT_RATE; }
            if (envAcc <= 0) { envAcc += OUT_RATE / (double)ENV_RATE; for (auto& v : voices) if (v.active) { v.updateEnv(); if (v.modDelay > 0 && v.modTimer < v.modDelay) v.modTimer++; } }
            int chunk = (int)std::min((double)(n - done), std::min(std::ceil(tickAcc), std::ceil(envAcc))); if (chunk < 1) chunk = 1;
            mixVoices(out + done * 2, chunk);
            done += chunk; tickAcc -= chunk; envAcc -= chunk;
        }
    }
    void mixVoices(float* out, int n) {
        for (auto& v : voices) {
            if (!v.active || !v.smp) continue; const Sample& s = *v.smp; float a = envAmp(v.env) * masterGain; if (a <= 0 && v.phase == 3) { v.active = false; continue; }
            float mod = 0; if (v.modDepth > 0 && v.modTimer >= v.modDelay) { v.modPhase += n * (v.modSpeed / 16.0) * 6.2831853 * 0.05 / 32.0; mod = (float)sin(v.modPhase) * v.modDepth / 128.0f; }
            double step = v.baseStep * pow(2.0, (v.bend + v.sweep + mod) / 12.0);
            float gl = a * v.volL, gr = a * v.volR; size_t len = s.pcm.size();
            for (int i = 0; i < n; i++) {
                size_t i0 = (size_t)v.pos; if (i0 + 1 >= len) { if (s.loop && s.loopEnd > s.loopStart) { v.pos = s.loopStart + (v.pos - s.loopEnd); if (v.pos < s.loopStart || v.pos >= len) v.pos = s.loopStart; i0 = (size_t)v.pos; } else { v.active = false; break; } }
                float fr = (float)(v.pos - i0); float smpv = (s.pcm[i0] * (1 - fr) + s.pcm[i0 + 1 < len ? i0 + 1 : i0] * fr) / 32768.0f;
                out[i * 2] += smpv * gl; out[i * 2 + 1] += smpv * gr; v.pos += step;
                if (s.loop && v.pos >= s.loopEnd && s.loopEnd > s.loopStart) v.pos = s.loopStart + (v.pos - s.loopEnd);
            }
        }
    }
};

} // namespace snd
