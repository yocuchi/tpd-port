// Sistema de sonido del juego: BRSAR + secuenciadores (1 BGM + varios SE) mezclados en el callback de SDL.
#pragma once
#include "brsar.hpp"
#include "seq.hpp"
#include <SDL2/SDL.h>
#include <memory>

namespace snd {

// ids de sonido del BRSAR (índice en INFO): coinciden con los ids que usa el juego (FUN_800427b4 / BGM)
enum : int { BGM_TITLE = 0, BGM_MENU_A = 1, BGM_MENU_B = 2, BGM_MENU_C = 3, BGM_CREDIT = 4, JINGLE_CLEAR = 5, JINGLE_GAMEOVER = 6, JINGLE_START = 7, JINGLE_WINNER = 8, BGM_GAMEOVER = 9,
    SE_CURSOR = 13, SE_DECISION_A = 14, SE_DECISION_B = 15, SE_CANCEL = 16, SE_ERROR = 17, SE_MESSAGE = 18, SE_WIN_OPEN = 19, SE_WIN_CLOSE = 20, SE_CHOICE = 21,
    BGM_GAME_A = 44, SE_MINO_SPIN = 59, SE_MINO_MOVE = 60, SE_MINO_TOUCH = 61, SE_MINO_LANDING = 62, SE_MINO_LOCKDOWN = 63, SE_SPIN_FAILURE = 64, SE_GAMEOVER = 65,
    SE_LINE_SINGLE = 66, SE_LINE_DOUBLE = 67, SE_LINE_TRIPLE = 68, SE_LINE_TETRIS = 69, SE_TSPIN_EZ = 70, SE_B2B = 74, SE_SOFTDROP = 75, SE_HARDDROP = 76, SE_HOLD = 77, SE_LEVELUP = 78, SE_GAME_CLEAR = 79, SE_ALERT = 80, SE_ALLCLEAR = 110, SE_PAUSE = 111, SE_NEW_RECORD = 122 };

class SoundSystem {
public:
    Brsar brsar; std::vector<const SoundInfo*> byIndex;
    std::map<uint32_t, std::unique_ptr<Bank>> banks; std::map<uint32_t, std::unique_ptr<WaveArc>> waveArcs;
    Sequencer bgm; Sequencer se[8]; int seNext = 0; SDL_AudioDeviceID dev = 0; bool ok = false; float bgmVol = 1.0f, bgmFade = 0, bgmFadeRate = 0; int currentBgm = -1;
    float bgmDuck = 1.0f; bool bgmOn = true, seOn = true;  // PAUSA: atenuación de la BGM en pausa y interruptores música / SE del menú de pausa
    void setBgmDuck(float v) { bgmDuck = v; }

    bool load(const std::string& path) {
        if (!brsar.load(path)) return false;
        // índice → sonido (orden de la tabla INFO = orden de nombres en SYMB para este juego)
        std::vector<std::pair<uint32_t, const SoundInfo*>> v; for (auto& kv : brsar.sounds) { uint32_t idx = 0; for (size_t i = 0; i < brsar.names.size(); i++) if (brsar.names[i] == kv.first) { idx = (uint32_t)i; break; } v.push_back({idx, &kv.second}); }
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; }); for (auto& p : v) byIndex.push_back(p.second);
        SDL_AudioSpec want = {}, have; want.freq = OUT_RATE; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = &SoundSystem::cb; want.userdata = this;
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0); if (!dev) { fprintf(stderr, "audio: %s\n", SDL_GetError()); return false; }
        SDL_PauseAudioDevice(dev, 0); ok = true; return true;
    }
    const Bank* bankFor(const SoundInfo* si, WaveArc*& wa) {
        uint32_t bf = brsar.bankFile[si->bankId];
        auto it = banks.find(bf); if (it == banks.end()) { size_t bl, wl; const uint8_t* bd = brsar.file(bf, bl); const uint8_t* wd = brsar.waveArc(bf, wl); auto b = std::make_unique<Bank>(); auto w = std::make_unique<WaveArc>(); if (!bd || !b->load(bd, bl) || !wd || !w->load(wd, wl)) return nullptr; waveArcs[bf] = std::move(w); it = banks.emplace(bf, std::move(b)).first; }
        wa = waveArcs[bf].get(); return it->second.get();
    }
    const SoundInfo* info(int id) { return (id >= 0 && id < (int)byIndex.size()) ? byIndex[id] : nullptr; }
    void startSeq(Sequencer& s, const SoundInfo* si, float gain) { WaveArc* wa; const Bank* b = bankFor(si, wa); if (!b) return; size_t sl; const uint8_t* sd = brsar.file(si->fileId, sl); if (!sd) return; s.start(sd, sl, si->seqOffset, b, wa, si->allocTrack); s.masterGain = gain * (si->volume / 127.0f); }

    void playBgm(int id) { if (!ok || id == currentBgm) return; const SoundInfo* si = info(id); if (!si) return; SDL_LockAudioDevice(dev); startSeq(bgm, si, 0.8f); bgmVol = 1.0f; bgmFadeRate = 0; currentBgm = id; SDL_UnlockAudioDevice(dev); }
    void stopBgm(float fadeSec = 0.5f) { if (!ok) return; SDL_LockAudioDevice(dev); if (fadeSec <= 0) { bgm.stop(); bgmVol = 0; } else bgmFadeRate = 1.0f / (fadeSec * OUT_RATE); currentBgm = -1; SDL_UnlockAudioDevice(dev); }
    // SE en bucle (p. ej. SE_ALERT, cuya secuencia se repite sola): devuelve la ranura para pararlo con stopSe
    int playSeLoop(int id) { if (!ok || !seOn) return -1; const SoundInfo* si = info(id); if (!si) return -1; SDL_LockAudioDevice(dev); int k = seNext; Sequencer& s = se[k]; seNext = (seNext + 1) % 8; startSeq(s, si, 0.9f); SDL_UnlockAudioDevice(dev); return k; }
    void stopSe(int slot) { if (!ok || slot < 0 || slot >= 8) return; SDL_LockAudioDevice(dev); se[slot].stop(); SDL_UnlockAudioDevice(dev); }
    void playSe(int id) { if (!ok || !seOn) return; const SoundInfo* si = info(id); if (!si) return; SDL_LockAudioDevice(dev); Sequencer& s = se[seNext]; seNext = (seNext + 1) % 8; startSeq(s, si, 0.9f); SDL_UnlockAudioDevice(dev); }

    static void cb(void* ud, Uint8* stream, int len) { ((SoundSystem*)ud)->mix((float*)stream, len / (int)(sizeof(float) * 2)); }
    void mix(float* out, int n) {
        memset(out, 0, n * 2 * sizeof(float));
        if (bgm.playing) { bgm.masterGain = bgm.masterGain; std::vector<float> tmp(n * 2, 0.0f); bgm.render(tmp.data(), n); for (int i = 0; i < n; i++) { if (bgmFadeRate > 0) { bgmVol -= bgmFadeRate; if (bgmVol <= 0) { bgmVol = 0; bgm.stop(); bgmFadeRate = 0; } } float bv = bgmVol * bgmDuck * (bgmOn ? 1.0f : 0.0f); out[i * 2] += tmp[i * 2] * bv; out[i * 2 + 1] += tmp[i * 2 + 1] * bv; } }
        for (auto& s : se) if (s.playing) s.render(out, n);
        for (int i = 0; i < n * 2; i++) { float v = out[i]; out[i] = v > 1 ? 1 : v < -1 ? -1 : v; }
    }
};

} // namespace snd
