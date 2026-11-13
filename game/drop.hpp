// Modo Corredor de etapas (CFlowDropScrollGame, flow 8; tipo de motor 2): el campo sube (scroll) insertando filas de obstáculos
// por abajo; la pieza cae/se mueve entre ellos. Reglas: notes_modes_special.md §3 (FUN_80062e78, FUN_80062ca4, FUN_800b9bf8, FUN_800ba678).
#pragma once
// [inferido] / sin resolver: reglas TPF2 80466f08 (80462078 es del tipo 8 = campo ancho 15, no de este modo); fondo/BGM; umbrales 28/16 aplicados a la fila
// superior de la pieza (el original usa la caja 4x4); sin límite inferior de dt (el original lo fija a 16.68 ms por ser 60 fps); eventos 0x38 (cartel cada
// 40 filas en Sin fin), CPU/versus (tablas de soluciones 0x80414C18..) y opción de curso (+0xc6) sin menú; dureza hard drop desactivada.
#include "../engine/engine.hpp"
#include <fstream>
#include <cmath>

namespace game {

struct DropData {  // data/drop.bin (tools/extract_drop.py)
    uint16_t obs[3 * 40 * 40]; uint8_t stages[30]; bool ok = false;  // obs[curso*1600 + patrón*40 + fila], bit i = columna i
    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
        if (d.size() < 4 + 4 + 9600 + 4 + 30 || memcmp(d.data(), "DROP", 4)) return false;
        memcpy(obs, &d[8], 9600); memcpy(stages, &d[8 + 9600 + 4], 30); ok = true; return true;  // little endian
    }
};

class DropMode {
public:
    te::Game* g = nullptr; const DropData* D = nullptr; std::function<void(int)> onSe;
    int stage = 0, courseOpt = 0, param = 0; bool endless = false, mirror = false;
    int curso = 0;            // param % 3
    int pattern = 0, patRow = 0;  // +0x58c (-1 = hueco/meta), +0x590
    int scrolled = 0;         // filas insertadas (+0x59c + 20)
    int rows = 0;             // +0x68: filas recorridas (las "líneas" del modo)
    float speed = 1.0f;       // +0x594: 0.25 / 1 / 4
    float phase = 0, rowMs = 500;  // fase del scroll (ms, negativa hasta 0) y duración de una fila
    bool upEnabled = false, cleared = false, failed = false, finished = false;
    uint32_t lcg = 1;

    static int paramOf(const DropData& d, int st, int opt) { return d.stages[(st % 10) * 3 + (opt ? 1 : 0)]; }
    static bool mirrorOf(const DropData& d, int st) { return d.stages[(st % 10) * 3 + 2] != 0; }

    // FUN_80062ca4 (configurar) + FUN_800bb4fc (reintentar)
    void start(te::Game& game, const DropData& data, int st, bool endlessMode, int opt, uint32_t seed) {
        g = &game; D = &data; stage = st; endless = endlessMode; courseOpt = opt; param = paramOf(data, st, opt); mirror = mirrorOf(data, st); curso = param % 3;
        pattern = param < 3 ? 10 : param < 6 ? 0 : param < 9 ? 20 : param < 12 ? 0 : 30;
        patRow = 0; scrolled = 0; rows = 0; speed = 1.0f; phase = 0; rowMs = 500; upEnabled = false; cleared = failed = finished = false; lcg = seed;
        g->holdEnabled = false; g->fallOverride = 0.2f;
    }
    int rand15() { lcg = lcg * 0x41c64e6du + 0x3039u; return (lcg >> 16) & 0x7fff; }

    // FUN_800b9bf8: velocidad por entrada (abajo = 4, arriba = 0.25, nada = 1); arriba pulsado = subir la pieza (SE 0x3c / 0x3d)
    void input(bool down, bool up, bool upPressed) {
        speed = down ? 4.0f : up ? 0.25f : 1.0f;
        if (upPressed && !finished) { bool ok = upEnabled && g->tryMoveUp(); if (onSe) onSe(ok ? 0x3c : 0x3d); }
    }
    // desplazamiento visual en celdas (>= 0): las filas recién subidas aparecen una celda más abajo y suben hasta 0
    float scrollY() const { return phase < 0 && rowMs > 0 ? -phase / rowMs : 0; }

    void update(float dt) {
        if (finished || !g || g->state) return;
        // duración de la fila (sin fin: se acelera cada 40 filas, tope 2.5x)
        if (!endless) rowMs = 500; else { float f = (float)(rows / 40) * 0.1f + 1.0f; if (f > 2.5f) f = 2.5f; rowMs = 500.0f / f; }
        if (g->hasPiece) {  // gravedad y muerte por arriba (cabecera de FUN_80062e78)
            int top = g->pieceTopRow();
            g->fallOverride = top < 28 ? 0.2f : 1000.0f;
            if (top < 16) { failed = finished = true; g->state = 2; if (g->cl) g->cl->lose(); return; }  // 0x40000000
        }
        bool busy = false;
        if (phase < 0) { float ms = std::min(dt * 1000.0f, 40.1f); phase += speed * ms; busy = phase <= 0; }
        if (!busy) step();
    }

private:
    void step() {  // tramo final de FUN_80062e78
        scrolled++;
        int r31 = rows;
        if (g->hasPiece) { r31 = scrolled + g->pieceBottomRow() - 40; if (r31 < 0) r31 = 0; }
        if (r31 > rows) rows = r31;
        g->addScore((int)(100.0f * speed));
        if (!endless && rows >= 400) { cleared = finished = true; g->state = 1; if (onSe) onSe(0x7f); return; }  // meta: 400 filas
        // desplaza el campo una fila hacia arriba (solo celdas ocupadas)
        int w = g->w, h = g->h;
        for (int y = 0; y < h - 1; y++) for (int x = 0; x < w; x++) { te::Cell& d = g->cells[x + y * w]; const te::Cell& s = g->cells[x + (y + 1) * w]; if (s.mino) d = s; else d = te::Cell(); }
        for (int x = 0; x < w; x++) {  // fila nueva abajo
            te::Cell& c = g->cells[x + (h - 1) * w]; c = te::Cell();
            if (pattern < 0) { if (patRow >= 1) { c.mino = 9; c.flags = 0x40; } }  // muro gris de meta (flag 0x48: ocupado + marcado)
            else {
                int bit = mirror ? w - 1 - x : x;
                if (D->obs[curso * 1600 + pattern * 40 + patRow] >> bit & 1) { int col = pattern % 10 + 2; c.mino = col == 10 ? 12 : col; c.flags = 0x20; }
            }
        }
        phase = -rowMs;
        if (g->pieceOverlaps()) g->pushPieceUp();  // el obstáculo empuja la pieza
        patRow++;
        if (pattern == -1 && patRow > 19) { failed = finished = true; g->state = 2; if (g->cl) g->cl->lose(); return; }  // 0x40000000 tras 20 filas de muro
        if (patRow > 39) {  // siguiente patrón
            if (!endless) { pattern++; if (pattern % 10 == 0) pattern = -1; }
            else {
                int n; do {
                    if (param >= 3 && param <= 5) { n = rand15() % 20; if (n > 9) n += 10; }
                    else if (param >= 12 && param <= 14) n = rand15() % 10 + 30;
                    else if (param < 3) n = rand15() % 20 + 10;
                    else { n = rand15() % 20; if (n > 9) n += 20; }
                } while (n == pattern);
                pattern = n;
            }
            patRow = 0;
        }
        if (patRow > 2) upEnabled = true;
    }
public:
    // Fila visible (frontera inferior, en filas de 0..20) de la línea de meta (frame00f); <0 = oculta. Solo durante el muro final.
    float goalBoundary() const { if (endless || pattern != -1) return -1; float b = (float)(20 - patRow); return b >= 0 && b <= 20 ? b : -1; }
};

} // namespace game
