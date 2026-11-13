// Jugador CPU (CTetrisAI, notes_vs.md §6): al aparecer cada pieza enumera las colocaciones alcanzables (rotación + columna + caída), las puntúa con una
// de 7 estrategias según el nivel de IA (el original encadena filtros lexicográficos sobre ~11 métricas; aquí es una suma ponderada de líneas, huecos,
// rugosidad, alturas y pozos) con ruido decreciente con el nivel, y la ejecuta pulsando botones con un retardo por acción que depende del nivel
// (L0=20, L1=10, L2=5, L3=2, L4..7=1, L8..11=0, L12..14=-10 -> mínimo). Es un te::Input: el motor lo lee como si fuera un mando.
#pragma once
#include "versus.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace game {

struct CpuInput : te::Input {
    te::Game* g = nullptr; int level = 0;       // level 0..14 (Nivel IA 1..15)
    bool enabled = true;
    uint32_t seed = 12345;
    // estado del plan
    bool planned = false; int tdir = 0, tx = 0, actions = 0, cool = 0; bool bl = false, br = false, ra = false, rb = false, bh = false;
    uint32_t rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
    float frand() { return (rnd() & 0xffff) / 65535.0f; }

    bool left() override { return enabled && bl; } bool right() override { return enabled && br; } bool rotA() override { return enabled && ra; } bool rotB() override { return enabled && rb; }
    bool soft() override { return false; } bool hard() override { return enabled && bh; } bool hold() override { return false; }

    // FUN_80071124: retardo extra por acción según el nivel mostrado (pasos del temporizador del motor; aquí frames a 60 Hz)
    static int delayFor(int L) { static const int D[15] = {20, 10, 5, 2, 1, 1, 1, 1, 0, 0, 0, 0, -10, -10, -10}; return D[L < 0 ? 0 : L > 14 ? 14 : L]; }
    // estrategia según nivel+1 (FUN_80157xxx): {1,10,14,16}→0; {2,5,9,15}→1; {3,8,13}→2; {4,7,12}→3; 6→4; 11→5; resto→6
    static int strategyFor(int L) {
        int n = L + 1; if (n == 1 || n == 10 || n == 14 || n == 16) return 0; if (n == 2 || n == 5 || n == 9 || n == 15) return 1;
        if (n == 3 || n == 8 || n == 13) return 2; if (n == 4 || n == 7 || n == 12) return 3; if (n == 6) return 4; if (n == 11) return 5; return 6;
    }
    struct Wt { float line[5]; float hole, bump, agg, maxh, well, land; };
    static const Wt& weights(int s) {
        static const Wt T[7] = {
            {{0, -2, 0, 3, 14}, -7, -0.6f, -0.45f, -0.25f, -0.3f, -0.3f},   // 0: constructor de Tetris (evita sencillos con la pila baja)
            {{0, 1, 3, 5, 9},   -6, -1.2f, -0.4f,  -0.2f,  -0.4f, -0.2f},   // 1: superficie plana
            {{0, 3, 6, 9, 11},  -4, -0.5f, -0.5f,  -0.3f,  -0.2f, -0.4f},   // 2: despeja rápido
            {{0, 1, 2, 4, 8},   -6, -0.6f, -0.9f,  -0.8f,  -0.3f, -0.2f},   // 3: seguro (baja la pila)
            {{0, 1, 3, 5, 10}, -10, -0.5f, -0.4f,  -0.3f,  -0.3f, -0.3f},   // 4: evita huecos
            {{0, 0, 2, 5, 12},  -6, -0.8f, -0.6f,  -0.4f,  -0.5f, -0.3f},   // 5: mixta
            {{0, 2, 4, 6, 10},  -6, -0.6f, -0.5f,  -0.3f,  -0.3f, -0.3f}};  // 6: equilibrada (nivel 1 / defecto)
        return T[s < 0 ? 0 : s > 6 ? 6 : s];
    }

    void poll() override {
        bl = br = ra = rb = bh = false;
        if (!g || !enabled) return;
        if (!g->hasPiece || g->state) { planned = false; return; }
        if (g->phase != te::P_FALL && g->phase != te::P_LOCK) return;
        if (!planned) { plan(); planned = true; actions = 0; cool = std::max(0, delayFor(level)) / 2 + 1; }
        if (cool > 0) { cool--; return; }
        const te::Piece& p = g->piece; actions++;
        if (actions > 60) bh = true;  // atascado (rotación/movimiento bloqueados): cae donde esté
        else if (p.dir != tdir) { int d = (tdir - p.dir) & 3; if (d == 3) rb = true; else ra = true; }
        else if (p.x < tx) br = true; else if (p.x > tx) bl = true; else bh = true;
        cool = std::max(1, delayFor(level));  // al menos un frame con el botón suelto entre acciones (el motor detecta el flanco por pulsación)
    }

    // ---- búsqueda de colocaciones alcanzables y evaluación ----
    void plan() {
        const te::Game& G = *g; const te::TetDef& t = *G.piece.tet; int W = G.w, H = G.h, x0 = G.piece.x, y0 = G.piece.floorY();
        const Wt& wt = weights(strategyFor(level));
        std::vector<uint8_t> occ(W * H); int maxTop = H; for (int i = 0; i < W * H; i++) { occ[i] = G.cells[i].mino ? 1 : 0; if (occ[i] && i / W < maxTop) maxTop = i / W; }
        int stackH = H - maxTop; bool danger = stackH > 14;
        float best = -1e9f; int bd = G.piece.dir, bx = x0; float noise = (14 - std::min(level, 14)) * 0.55f;
        for (int dir = 0; dir < 4; dir++) {
            for (int x = x0 - 8; x <= x0 + 8; x++) {
                if (!pieceFits(G, t, dir, x, y0)) continue;
                bool path = true; for (int xx = std::min(x, x0); xx <= std::max(x, x0) && path; xx++) if (!pieceFits(G, t, dir, xx, y0)) path = false;
                if (!path) continue;
                int y = y0; while (pieceFits(G, t, dir, x, y + 1)) y++;
                float sc = evaluate(G, t, dir, x, y, occ, wt, danger) + (frand() - 0.5f) * 2.0f * noise;
                if (sc > best) { best = sc; bd = dir; bx = x; }
            }
        }
        tdir = bd; tx = bx;
    }
    float evaluate(const te::Game& G, const te::TetDef& t, int dir, int x, int y, std::vector<uint8_t> occ, const Wt& wt, bool danger) {
        int W = G.w, H = G.h, ax = t.marks ? t.mx(dir, 0) : 0, ay = t.marks ? t.my(dir, 0) : 0; int lowest = -1;
        for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (t.cell(i, j, dir)) { int cx = i + x - ax, cy = j + y - ay; if (cx >= 0 && cx < W && cy >= 0 && cy < H) { occ[cx + cy * W] = 1; if (cy > lowest) lowest = cy; } }
        int lines = 0; std::vector<uint8_t> o2; o2.reserve(W * H);
        std::vector<uint8_t> rows; for (int yy = 0; yy < H; yy++) { bool full = true; for (int xx = 0; xx < W; xx++) if (!occ[xx + yy * W]) { full = false; break; } if (full) lines++; }
        // compacta quitando las filas completas
        std::vector<uint8_t> grid(W * H, 0); int dst = H - 1; for (int yy = H - 1; yy >= 0; yy--) { bool full = true; for (int xx = 0; xx < W; xx++) if (!occ[xx + yy * W]) { full = false; break; } if (full) continue; for (int xx = 0; xx < W; xx++) grid[xx + dst * W] = occ[xx + yy * W]; dst--; }
        int hgt[16] = {}, holes = 0; for (int xx = 0; xx < W; xx++) { int top = -1; for (int yy = 0; yy < H; yy++) if (grid[xx + yy * W]) { top = yy; break; } hgt[xx] = top < 0 ? 0 : H - top; if (top >= 0) for (int yy = top; yy < H; yy++) if (!grid[xx + yy * W]) holes++; }
        int agg = 0, mx = 0; float bump = 0, well = 0; for (int xx = 0; xx < W; xx++) { agg += hgt[xx]; mx = std::max(mx, hgt[xx]); if (xx + 1 < W) bump += (float)std::abs(hgt[xx] - hgt[xx + 1]); }
        for (int xx = 0; xx < W; xx++) { int l = xx > 0 ? hgt[xx - 1] : 99, r = xx + 1 < W ? hgt[xx + 1] : 99, d = std::min(l, r) - hgt[xx]; if (d > 0 && d < 90) well += (float)d * d * 0.5f; }
        float lv = wt.line[std::min(lines, 4)]; float landing = (float)(H - lowest);
        if (danger) lv = lines > 0 ? lv + 3.0f * lines : lv;  // con la pila alta prioriza despejar y bajar
        // el motor conserva la altura del campo oculto; la altura se mide en filas desde el suelo
        float sc = lv * 10.0f * 0.5f + wt.hole * holes + wt.bump * bump + wt.agg * agg + wt.maxh * mx + wt.well * well + wt.land * landing;
        if (danger) sc += -0.6f * mx - 2.0f * holes;
        return sc;
    }
};

} // namespace game
