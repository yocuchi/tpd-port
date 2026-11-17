// Ajustes de partida por modo (menú "Ajustes", notes_modes_basic.md §1.1 / notes_vs.md §1.7). Sin dependencias de SDL ni de menú:
// los edita gfx/menu.hpp (pantalla Ajustes), los aplica main_sdl.cpp (startGame/startVs) y se guardan en un fichero binario sencillo.
//   Mac:    ~/Library/Application Support/tetris-port/settings.bin      Switch: sdmc:/switch/tetris-port/settings.bin
// En el original solo Ghost/Mapa/Fondo/BGM van al save; Sin fin, NIV inicial, Nivel IA, Tetriminos y Penalización son BSS (se pierden al elegir datos).
// Aquí se guardan todos por comodidad [inferido: diferencia irrelevante en un port sin perfiles].
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace game {

enum SettingId { S_ENDLESS = 0, S_STARTLEVEL = 1, S_AILEVEL = 3, S_TETRIMINOS = 5, S_ITEMS = 6, S_PENALTY = 10, S_MAP = 11, S_RULES = 13, S_GHOST = 15, S_BG = 16, S_BGM = 17, S_PLAY = 18 };  // = bit de la máscara +0xD38 de CMenuContentsGameSetting

// Valores por modo (CGameLocalConfig, notes_modes_basic §1.1). bg/bgm = -1: el del modo (ModeSpec); BGM_NONE = sin música (0xff en el original).
struct ModeSettings {
    bool endless = false;     // "Sin fin" (+0xc5): sin meta de 150 líneas
    int startLevel = 0;       // "NIV inicial" 0..14 (+0xb0; se muestra 1..15)
    int aiLevel = 0;          // "Nivel IA" 0..14 (+0xb4)
    int tetriminos = 0;       // "Tetriminos" (+0xb8): 0 FÁCIL / 1 NORMAL
    bool slippery = false;    // "Penalización" (+0xc0): Normal / Resbaladizo
    bool map = false;         // "Mapa" (+0xbc)
    bool ghost = true;        // "Ghost" (+0xc4)
    int bg = -1;              // "Fondo" 0..15
    int bgm = -1;             // "Música de fondo": id de BGM (44..58) / BGM_NONE
};

struct Settings {
    static constexpr int MODES = 32, BGM_FIRST = 44, BGM_LAST = 58, BGM_NONE = 255;
    ModeSettings m[MODES];
    // Objetos activados (bit = id-1, como items.mask de game/items.hpp). Defecto: todos a OFF, como el original [inferido: BSS a 0]; se activan en Ajustes del modo Tetris VS.
    static uint32_t allItemsMask() { static const int B[13] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 17, 14, 15}; uint32_t k = 0; for (int b : B) k |= 1u << b; return k; }
    uint32_t itemMask = 0;
    Settings() { m[1].map = true; /* vs IA: "Mapa" ON por defecto (DAT_804e20a0[1] = 1) */ }
    ModeSettings& of(int mode) { if (mode == 27 || mode == 28) mode = 10; return m[mode >= 0 && mode < MODES ? mode : 0]; }  // 27 (Tetris VS 6) comparte los ajustes del 10

    // ---- Qué ajustes muestra cada modo (máscara de bits de FUN_80195a70, en orden de pantalla) ----
    static uint32_t maskOf(int mode) {
        uint32_t k = (1u << S_RULES) | (1u << S_GHOST) | (1u << S_BG) | (1u << S_BGM);  // 0x3A000: comunes
        switch (mode) {
        case 0: k |= (1u << S_ENDLESS) | (1u << S_STARTLEVEL); break;
        case 1: k |= (1u << S_AILEVEL) | (1u << S_PENALTY) | (1u << S_MAP); break;
        case 5: k |= (1u << S_ENDLESS) | (1u << S_STARTLEVEL) | (1u << S_TETRIMINOS); break;
        case 8: k |= (1u << S_ENDLESS) | (1u << S_STARTLEVEL); k &= ~(1u << S_GHOST); break;  // Maestro: sin Ghost
        case 10: case 25: case 27: case 28: k |= (1u << S_AILEVEL) | (1u << S_ITEMS) | (1u << S_PENALTY) | (1u << S_MAP); break;  // Tetris VS
        default: break;  // 6 All Clear Sprint, 7 Sprint y el resto: solo las comunes [modos 2/3/4/9 tienen ajustes propios sin implementar]
        }
        return k;
    }
    static int bgmCount() { return BGM_LAST - BGM_FIRST + 2; }       // 15 melodías + "sin música"
    static int bgmOfIndex(int i) { return i <= BGM_LAST - BGM_FIRST ? BGM_FIRST + i : BGM_NONE; }
    static int indexOfBgm(int id) { return id == BGM_NONE ? BGM_LAST - BGM_FIRST + 1 : id - BGM_FIRST; }
    // nombre visible de la melodía: BGM_GAME_A..M = 44..56, 57 = variante JAPAN [inferido], 58 = GAME_N
    static std::string bgmLabel(int id) {
        if (id == BGM_NONE) return "-";
        if (id >= BGM_FIRST && id <= 56) { std::string s = "GAME "; s += (char)('A' + id - BGM_FIRST); return s; }
        if (id == 57) return "GAME B JAPAN"; if (id == 58) return "GAME N"; return std::to_string(id);
    }

    // ---- persistencia ----
    static std::string path() {
#ifdef __SWITCH__
        return "sdmc:/switch/tetris-port/settings.bin";
#else
        const char* h = getenv("HOME"); return std::string(h ? h : ".") + "/Library/Application Support/tetris-port/settings.bin";
#endif
    }
    static void mkdirs(const std::string& file) {
        for (size_t i = 1; i < file.size(); i++) if (file[i] == '/') { std::string d = file.substr(0, i); mkdir(d.c_str(), 0755); }
    }
    struct Rec { int8_t endless, startLevel, aiLevel, tetriminos, slippery, map, ghost, pad; int16_t bg, bgm; };
    bool load(const std::string& p = path()) {
        FILE* f = fopen(p.c_str(), "rb"); if (!f) return false;
        char magic[4]; uint32_t ver = 0, mask = 0; bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "TPS1", 4) == 0 && fread(&ver, 4, 1, f) == 1 && (ver == 1 || ver == 2) && fread(&mask, 4, 1, f) == 1;
        if (ver == 1) mask = 0;  // v1 guardaba los 13 objetos activados por defecto: al migrar se dejan a OFF
        Settings tmp; if (ok) for (int i = 0; i < MODES && ok; i++) {
            Rec r; ok = fread(&r, sizeof r, 1, f) == 1; if (!ok) break; ModeSettings& s = tmp.m[i];
            s.endless = r.endless != 0; s.startLevel = clampi(r.startLevel, 0, 14); s.aiLevel = clampi(r.aiLevel, 0, 14); s.tetriminos = clampi(r.tetriminos, 0, 1); s.slippery = r.slippery != 0; s.map = r.map != 0; s.ghost = r.ghost != 0;
            s.bg = r.bg < 0 ? -1 : clampi(r.bg, 0, 15); s.bgm = r.bgm < 0 ? -1 : r.bgm == BGM_NONE ? BGM_NONE : clampi(r.bgm, BGM_FIRST, BGM_LAST);
        }
        fclose(f); if (!ok) return false; tmp.itemMask = mask & 0x7FFFF; *this = tmp; return true;
    }
    bool save(const std::string& p = path()) const {
        mkdirs(p); FILE* f = fopen(p.c_str(), "wb"); if (!f) return false;
        uint32_t ver = 2; fwrite("TPS1", 1, 4, f); fwrite(&ver, 4, 1, f); fwrite(&itemMask, 4, 1, f);
        for (int i = 0; i < MODES; i++) { const ModeSettings& s = m[i]; Rec r{(int8_t)s.endless, (int8_t)s.startLevel, (int8_t)s.aiLevel, (int8_t)s.tetriminos, (int8_t)s.slippery, (int8_t)s.map, (int8_t)s.ghost, 0, (int16_t)s.bg, (int16_t)s.bgm}; fwrite(&r, sizeof r, 1, f); }
        fclose(f); return true;
    }
    static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
};

}  // namespace game
