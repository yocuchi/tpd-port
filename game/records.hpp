// RECORDS: récords persistentes por modo (y etapa): top-5 de puntuación (desc.) y top-5 de tiempos (asc.), notes_modes_basic.md §2.4.
// Fichero binario: "TPR1", u32 nº de entradas, y por entrada: u32 clave, 5 x i32 puntuaciones, 5 x f32 tiempos (s). Vacío = 0 / -1.
//   Mac: ~/Library/Application Support/tetris-port/records.bin (o ./records.bin si no se puede crear); Switch: sdmc:/switch/tetris-port/records.bin.
#pragma once
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <sys/stat.h>

namespace game {

struct RecEntry {
    int score[5]; float time[5];
    RecEntry() { for (int i = 0; i < 5; i++) { score[i] = 0; time[i] = -1; } }
};

class Records {
public:
    std::map<int, RecEntry> m; std::string file; bool dirty = false;
    static int key(int mode, int stage) { return (mode & 63) * 64 + (stage & 63); }
    int bestScore(int k) const { auto it = m.find(k); return it == m.end() ? 0 : it->second.score[0]; }
    float bestTime(int k) const { auto it = m.find(k); return it == m.end() ? -1.0f : it->second.time[0]; }
    // Inserta una puntuación: devuelve el puesto 0..4 o -1 si no entra (puntuación <= 0 no cuenta)
    int addScore(int k, int s) {
        if (s <= 0) return -1; RecEntry& e = m[k]; int r = 0; while (r < 5 && e.score[r] >= s) r++; if (r >= 5) return -1;
        for (int i = 4; i > r; i--) e.score[i] = e.score[i - 1]; e.score[r] = s; dirty = true; return r;
    }
    // Inserta un tiempo (menor es mejor): devuelve el puesto o -1
    int addTime(int k, float t) {
        if (t <= 0) return -1; RecEntry& e = m[k]; int r = 0; while (r < 5 && e.time[r] >= 0 && e.time[r] <= t) r++; if (r >= 5) return -1;
        for (int i = 4; i > r; i--) e.time[i] = e.time[i - 1]; e.time[r] = t; dirty = true; return r;
    }
    static std::string defaultPath() {
#ifdef __SWITCH__
        mkdir("sdmc:/switch", 0777); mkdir("sdmc:/switch/tetris-port", 0777); return "sdmc:/switch/tetris-port/records.bin";
#else
        const char* h = getenv("HOME");
        if (h && *h) {
            std::string d = std::string(h) + "/Library/Application Support"; mkdir(d.c_str(), 0777); d += "/tetris-port"; mkdir(d.c_str(), 0777);
            struct stat st; if (stat(d.c_str(), &st) == 0 && (st.st_mode & S_IFDIR)) return d + "/records.bin";
        }
        return "records.bin";
#endif
    }
    bool load(const std::string& path = std::string()) {
        file = path.empty() ? defaultPath() : path; m.clear(); dirty = false;
        FILE* f = fopen(file.c_str(), "rb"); if (!f) return false;
        char mg[4]; uint32_t n = 0; bool ok = fread(mg, 1, 4, f) == 4 && !memcmp(mg, "TPR1", 4) && fread(&n, 4, 1, f) == 1 && n < 4096;
        for (uint32_t i = 0; ok && i < n; i++) { uint32_t k; RecEntry e; ok = fread(&k, 4, 1, f) == 1 && fread(e.score, 4, 5, f) == 5 && fread(e.time, 4, 5, f) == 5; if (ok) m[(int)k] = e; }
        fclose(f); return ok;
    }
    bool save() {
        if (file.empty()) file = defaultPath();
        FILE* f = fopen(file.c_str(), "wb"); if (!f && file != "records.bin") { file = "records.bin"; f = fopen(file.c_str(), "wb"); } if (!f) return false;
        uint32_t n = (uint32_t)m.size(); fwrite("TPR1", 1, 4, f); fwrite(&n, 4, 1, f);
        for (auto& kv : m) { uint32_t k = (uint32_t)kv.first; fwrite(&k, 4, 1, f); fwrite(kv.second.score, 4, 5, f); fwrite(kv.second.time, 4, 5, f); }
        fclose(f); dirty = false; return true;
    }
};

} // namespace game
