// Modo Bombliss (CFlowBomblissGame, flow 0x10, modo 9; client+0x578 = 0x13). Ver notes_modes_special.md §4.
// La explosion en cadena NO esta en los scripts TPF2 (los bloques 8044aa58/80466f08 no tienen comandos de bomba): es codigo nativo
// del motor "te", localizado en CSingleResultWindow.c (tramo mal cortado 0x801b3e64..0x801b5930):
//   FUN_801b4bb8 = tick de resolucion de bombas (llamado cada frame mientras no hay pieza activa, FUN_80071124);
//   FUN_801b3e64 / FUN_801b4358 / FUN_801b461c = etiquetado de grupos conexos, soporte y caida suave de los bloques flotantes;
//   FUN_801b4a2c = union de 2x2 bombas en bomba grande; FUN_801b4990 + tabla 0x804125f0 = area de explosion por nivel.
// Aqui se reproduce ese algoritmo con arrays paralelos al campo del motor (occ, col, bf, attr, tmr, off).
#pragma once
#include "../engine/engine.hpp"
#include "../gfx/game_view.hpp"
#include <fstream>
#include <random>
#include <functional>
#include <cstring>

namespace game {

struct BomblissData {
    uint8_t ph[30][200];  // 20 filas x 10 columnas, fila 0 arriba. 0 vacio, 1..0x14 color, 0x15 bomba, 0x16 bomba grande (ancla sup.izq.), 0x17 resto de bomba grande
    bool ok = false;
    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
        if (d.size() < 6000) return false; memcpy(ph, d.data(), 6000); ok = true; return true;
    }
};

class BomblissMode : public view::FieldExtra {
public:
    // area de explosion por nivel (0x804125f0): ancho, alto
    static constexpr int AREA[21][2] = {{10,10},{7,1},{7,3},{7,5},{7,7},{9,9},{9,9},{11,11},{11,11},{13,13},{13,13},{15,15},{15,15},{17,17},{17,17},{17,17},{17,17},{17,17},{17,17},{17,17},{17,17}};
    // evento -> sonido: 0x31 fila(s) completa(s) SE 137, 0x33 explosion SE 138, 0x36 union SE 139 (una vez por frame, FUN_800a53d0/5640/5a00)
    struct Fx { int kind; int x0, y0, x1, y1; float age; };
    te::Game* g = nullptr; const BomblissData* D = nullptr; int phase = 0, difficulty = 0, w = 10, h = 40, top = 20;
    std::vector<uint8_t> occ, bf; std::vector<uint32_t> attr; std::vector<float> tmr; std::vector<int16_t> off; std::vector<uint8_t> col;
    bool ready = false, falling = false, chainPending = false; int chain = 0, prevRows = 0, bombCounter = 0;
    int bombIdx = 0;  // 0 ninguna, 1..4 indice de celda con bomba, 0xff pieza entera
    bool cleared = false, failed = false, finished = false; float finishTime = 0; int finalPoints = 0;
    std::function<void(int)> onSe; std::vector<Fx> fx; bool seRow = false, seExp = false, seComb = false;
    std::mt19937 rng{12345};

    int points() const { int p = 99 - g->lockCount; return p < 0 ? 0 : p; }
    int idx(int x, int y) const { return x + y * w; }
    bool in(int x, int y) const { return x >= 0 && x < w && y >= 0 && y < h; }

    void start(te::Game& game, const BomblissData& data, int ph) {
        g = &game; D = &data; phase = ph % 30; w = g->w; h = g->h; top = h / 2; difficulty = phase < 6 ? 0 : phase < 16 ? 1 : 2;
        size_t n = (size_t)w * h; occ.assign(n, 0); bf.assign(n, 0); attr.assign(n, 0); tmr.assign(n, 0); off.assign(n, 0); col.assign(n, 0);
        g->linesEnabled = false; g->holdEnabled = false; g->lockCount = 0; g->pieceBudget = 0; g->state = 0;
        ready = falling = chainPending = false; chain = prevRows = bombCounter = 0; bombIdx = 0; cleared = failed = finished = false; fx.clear(); finishTime = 0; finalPoints = 0;
        for (int r = 0; r < 20; r++) for (int x = 0; x < 10 && x < w; x++) {  // FUN_801b5930: patron en las filas 0x14..0x27
            uint8_t v = D->ph[phase][r * 10 + x]; int i = idx(x, top + r); if (!v) continue;
            occ[i] = 1; if (v == 0x15) { bf[i] = 2; col[i] = 12; } else if (v == 0x16) { bf[i] = 6; col[i] = 12; } else if (v == 0x17) { bf[i] = 4; col[i] = 12; } else col[i] = v;
        }
        sync();
        g->spawnGate = [this]() { return ready && g->state == 0; };
        g->onSpawn = [this]() { assignBomb(); };
        g->onLock = [this]() { importPiece(); };
    }
    void sync() { for (size_t i = 0; i < col.size(); i++) g->cells[i].mino = col[i]; }

    // ---- numeracion de las 4 celdas de cada tetromino (DAT_803ec2c0: valores 1..4 fijos por celda al rotar) ----
    static const char* tbl(int p, int r) {
        static const char* T[7][4] = {
            {".1.. .2.. .3.. .4..", ".... .... 1234 ....", "..4. ..3. ..2. ..1.", ".... 4321 .... ...."},
            {"13.. 24.. .... ....", "34.. 12.. .... ....", "42.. 31.. .... ....", "21.. 43.. .... ...."},
            {"1... 23.. .4.. ....", ".34. 12.. .... ....", ".4.. .32. ..1. ....", ".21. 43.. .... ...."},
            {".1.. 32.. 4... ....", ".... 12.. .34. ....", ".4.. 23.. 1... ....", "43.. .21. .... ...."},
            {".4.. .3.. 12.. ....", ".... 432. ..1. ....", ".21. .3.. .4.. ....", "1... 234. .... ...."},
            {"12.. .3.. .4.. ....", ".... 234. 1... ....", ".4.. .3.. .21. ....", "..1. 432. .... ...."},
            {".4.. 13.. .2.. ....", ".... 432. .1.. ....", ".2.. .31. .4.. ....", ".1.. 234. .... ...."}};
        return T[p][r];
    }
    // numero 1..4 de la celda (i,j) del grid 7x7 de la pieza activa; 0 si no es celda
    int cellNumber(const te::Game& gg, int ci, int cj) const {
        if (!gg.hasPiece) return 0; const te::TetDef& t = *gg.piece.tet; int dir = gg.piece.dir;
        int pi[4], pj[4], n = 0; for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (t.cell(i, j, dir) && n < 4) { pi[n] = i; pj[n] = j; n++; }
        if (n != 4) return 0; int mi = 9, mj = 9; for (int k = 0; k < 4; k++) { mi = std::min(mi, pi[k]); mj = std::min(mj, pj[k]); }
        int my = -1; for (int k = 0; k < 4; k++) if (pi[k] == ci && pj[k] == cj) my = k; if (my < 0) return 0;
        for (int pass = 0; pass < 2; pass++) for (int p = 0; p < 7; p++) for (int rr = 0; rr < 4; rr++) {
            if (pass == 0 && rr != dir) continue;
            const char* s = tbl(p, rr); int num[4][4] = {}; int cnt = 0, rowv = 0, colv = 0; for (const char* c = s; *c; c++) { if (*c == ' ') { rowv++; colv = 0; continue; } if (*c != '.') { num[rowv][colv] = *c - '0'; cnt++; } colv++; }
            int si = 9, sj = 9; for (int a = 0; a < 4; a++) for (int b = 0; b < 4; b++) if (num[a][b]) { si = std::min(si, b); sj = std::min(sj, a); }
            bool ok = cnt == 4; for (int k = 0; k < 4 && ok; k++) { int a = pj[k] - mj + sj, b = pi[k] - mi + si; if (a < 0 || a > 3 || b < 0 || b > 3 || !num[a][b]) ok = false; }
            if (ok) return num[cj - mj + sj][ci - mi + si];
        }
        return 0;
    }
    bool pieceBomb_(const te::Game& gg, int i, int j) const { if (!bombIdx) return false; if (bombIdx == 0xff) return true; return cellNumber(gg, i, j) == bombIdx; }
    bool pieceBomb(const te::Game& gg, int i, int j) override { return pieceBomb_(gg, i, j); }
    uint32_t pieceMino(uint32_t) override { return 12; }  // todas las piezas son de color 0xc (FUN_8006a7fc)

    // FUN_8006b170: bomba por pieza (bandera global r13-0x7e60 = 1 por defecto: aleatorio; cada 8/11/15 piezas una pieza entera de bombas)
    void assignBomb() {
        bombCounter++;
        int thr = difficulty == 0 ? 7 : difficulty == 1 ? 10 : 14;
        if (bombCounter > thr) { bombCounter = 0; bombIdx = 0xff; } else bombIdx = (int)(rng() % 4) + 1;
    }
    // FUN_80065854: al fijar, la celda cuyo indice coincide recibe flag 2; el bloque pasa a color 0xc
    void importPiece() {
        const te::Piece& pc = g->piece; const te::TetDef& t = *pc.tet; int ax = t.marks ? t.mx(pc.dir, 0) : 0, ay = t.marks ? t.my(pc.dir, 0) : 0, fy = pc.floorY();
        for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) {
            if (!t.cell(i, j, pc.dir)) continue; int cx = i + pc.x - ax, cy = j + fy - ay; if (!in(cx, cy)) continue;
            int k = idx(cx, cy); occ[k] = 1; col[k] = 12; bf[k] = pieceBomb_(*g, i, j) ? 2 : 0; attr[k] = 0; tmr[k] = 0; off[k] = 0;
        }
        bombIdx = 0; ready = false; sync();
    }

    // ---- utilidades de eventos ----
    void ev(int type, int x, int y, int a = 0, int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0) {
        if (type == 0x31 && !seRow) { seRow = true; if (onSe) onSe(137); }
        if (type == 0x33) { if (!seExp) { seExp = true; if (onSe) onSe(138); } fx.push_back({1, x1, y1, x2, y2, 0}); }
        if (type == 0x36) { if (!seComb) { seComb = true; if (onSe) onSe(139); } fx.push_back({3, x, y, x + 2, y + 2, 0}); }
        if (type == 0x34 && y >= top) fx.push_back({2, x, y, x + 1, y + 1, 0});
        if (type == 0x31) for (int yy = top; yy < h; yy++) { bool full = true; for (int xx = 0; xx < w; xx++) if (!occ[idx(xx, yy)]) { full = false; break; } if (full) fx.push_back({0, 0, yy, w, yy + 1, 0}); }
        (void)a;
    }
    static void area(int level, int& ax, int& ay, int& aw, int& ah) {  // FUN_801b4990
        if (level < 0) level = 0; if (level > 0x14) level = 0x14; aw = AREA[level][0]; ah = AREA[level][1];
        if (level == 0) { ax = -4; ay = -4; } else { ax = -(aw / 2); ay = -(ah / 2); }
    }
    // FUN_801b4a2c: 2x2 bombas pequenas -> bomba grande
    void combine() {
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int i = idx(x, y); if (!occ[i] || (bf[i] & 4) || !(bf[i] & 2)) continue;
            if (x + 1 >= w || y + 1 >= h) continue; int a = idx(x + 1, y), b = idx(x, y + 1), c = idx(x + 1, y + 1);
            auto small = [&](int k) { return occ[k] && (bf[k] & 6) == 2; };
            if (small(a) && small(b) && small(c)) { bf[i] |= 6; for (int k : {a, b, c}) { bf[k] &= ~6; bf[k] |= 4; } ev(0x36, x, y); }
        }
    }
    // ancla de una bomba grande entre (x-1,y), (x,y-1), (x-1,y-1) con flag 2 (o 6 si exact6)
    int findAnchor(int x, int y, bool exact6) const {
        static const int o[3][2] = {{-1, 0}, {0, -1}, {-1, -1}};
        for (auto& d : o) { int xx = x + d[0], yy = y + d[1]; if (!in(xx, yy)) continue; int k = idx(xx, yy); if (exact6 ? (bf[k] & 6) == 6 : (bf[k] & 2)) return k; }
        return -1;
    }

    // ---- FUN_801b3e64 (modo 0) + FUN_801b4358: etiquetado de grupos conexos; devuelve nº de grupos flotantes ----
    int label(int startOff) {
        for (size_t i = 0; i < attr.size(); i++) { attr[i] &= 0xffffff; off[i] = 0; }
        std::vector<int> stack; int groups = 0; int L = 1;
        auto flood = [&](int seed) { stack.assign(1, seed); attr[seed] |= (uint32_t)L << 24; if (L > 1) off[seed] = (int16_t)startOff;
            while (!stack.empty()) { int k = stack.back(); stack.pop_back(); int x = k % w, y = k / w; static const int d[4][2] = {{0,1},{1,0},{-1,0},{0,-1}};
                for (auto& q : d) { int xx = x + q[0], yy = y + q[1]; if (!in(xx, yy)) continue; int n = idx(xx, yy); if (occ[n] && !(attr[n] >> 24)) { attr[n] |= (uint32_t)L << 24; if (L > 1) off[n] = (int16_t)startOff; stack.push_back(n); } } } };
        for (int x = 0; x < w; x++) { int k = idx(x, h - 1); if (occ[k] && !(attr[k] >> 24)) flood(k); }
        for (int y = h - 1; y >= 0; y--) for (int x = 0; x < w; x++) { int k = idx(x, y); if (occ[k] && !(attr[k] >> 24)) { L++; if (L > 0xff) return groups; flood(k); groups++; } }
        return groups;
    }
    // FUN_801b4358: los grupos apoyados (suelo o etiqueta 1) pasan a etiqueta 1; los demas reciben desplazamiento newOff. Devuelve 1 si queda alguno flotando.
    int supports(int newOff) {
        int maxL = 0; for (uint32_t a : attr) maxL = std::max<int>(maxL, a >> 24);
        std::vector<char> done(maxL + 2, 0); int floating = 0; bool changed;
        do {
            changed = false;
            for (int L = 2; L <= maxL; L++) {
                if (done[L]) continue; bool found = false, sup = false;
                for (int x = 0; x < w && !sup; x++) for (int y = h - 1; y >= 0; y--) { int k = idx(x, y); if (occ[k] && (int)(attr[k] >> 24) == L) { found = true; if (y + 1 >= h) sup = true; else { int b = idx(x, y + 1); if ((attr[b] >> 24) == 1 && occ[b]) sup = true; } break; } }
                if (!found) continue;
                if (sup) { done[L] = 1; changed = true; for (size_t k = 0; k < attr.size(); k++) if ((int)(attr[k] >> 24) == L) { attr[k] = (attr[k] & 0xffffff) | (1u << 24); off[k] = 0; } }
            }
        } while (changed);
        for (size_t k = 0; k < attr.size(); k++) if ((attr[k] >> 24) > 1 && occ[k]) { off[k] = (int16_t)newOff; floating = 1; }
        return floating;
    }
    // FUN_801b461c: un paso de caida; true = terminado
    bool fallStep(float dtms) {
        int first = -1; for (int y = h - 2; y >= 0 && first < 0; y--) for (int x = 0; x < w; x++) { int k = idx(x, y); if ((attr[k] >> 24) > 1) { first = k; break; } }
        if (first < 0) { falling = false; return true; }
        int o = off[first]; o -= (int)(128.0f * dtms / 45.0f);
        if (o < -0x100) {
            int remaining;
            do {
                for (int y = h - 2; y >= 0; y--) for (int x = 0; x < w; x++) {
                    int s = idx(x, y), d = idx(x, y + 1); if ((attr[s] >> 24) > 1 && occ[s]) {
                        occ[d] = occ[s]; bf[d] = bf[s]; col[d] = col[s]; attr[d] = (attr[s] & 0xff000000) | (attr[d] & 0xffffff); tmr[d] = 0;
                        occ[s] = 0; bf[s] = 0; col[s] = 0; off[s] = 0; tmr[s] = 0; attr[s] &= 0xffffff; }
                }
                remaining = supports(o + 0x100); o += 0x100;
            } while (remaining && o < -0x100 + 0);
            if (!remaining) { falling = false; return true; }
            return false;
        }
        for (size_t k = 0; k < attr.size(); k++) if ((attr[k] >> 24) > 1) off[k] = (int16_t)o;
        return false;
    }

    // ---- FUN_801b4bb8: tick de resolucion. true = terminado (puede aparecer la pieza siguiente) ----
    bool tick(float dtms) {
        if (falling && !fallStep(dtms)) return false;
        size_t N = occ.size();
        // 1) bomba en explosion (attr 0x40000): al agotarse el temporizador se borra el area
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int i = idx(x, y); if (!(attr[i] & 0x40000)) continue;
            tmr[i] -= dtms; uint32_t lvl = attr[i] & 0x1f; int ax, ay, aw, ah;
            if (!(attr[i] & 0x20)) { lvl += chain; if (lvl > 0x14) lvl = 0x14; area((int)lvl, ax, ay, aw, ah); }
            else { for (auto dd : {std::pair<int,int>{1, 0}, {0, 1}, {1, 1}}) { int xx = x + dd.first, yy = y + dd.second; if (in(xx, yy)) { occ[idx(xx, yy)] = 0; bf[idx(xx, yy)] = 0; } } area(0, ax, ay, aw, ah); }
            if (tmr[i] <= 0) {
                bf[i] &= ~6; attr[i] &= 0xfff1ffc0;
                for (int yy = y + ay; yy < y + ay + ah; yy++) for (int xx = x + ax; xx < x + ax + aw; xx++) {
                    if (!in(xx, yy)) continue; int k = idx(xx, yy);
                    if (!(attr[k] & 0x60000)) { attr[k] = (attr[k] & 0xfff1ffc0) | 0x80000 | lvl; col[k] = 0; tmr[k] = 200; ev(0x34, xx, yy); }
                }
            }
            return false;
        }
        // 2) marcas de area (attr 0x80000): al agotarse, el bloque desaparece o la bomba atrapada se enciende (reaccion en cadena)
        bool any = false;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int i = idx(x, y); if (!(attr[i] & 0x80000)) continue; any = true; tmr[i] -= dtms; if (tmr[i] > 0) continue;
            tmr[i] = 0; attr[i] &= 0xfff3ffdf;
            if (!(bf[i] & 2)) {
                if (!(bf[i] & 4)) { attr[i] &= ~0x1fu; occ[i] = 0; bf[i] = 0; col[i] = 0; off[i] = 0; }
                else {
                    int a = findAnchor(x, y, true);
                    if (a >= 0 && !(attr[a] & 0x20000)) { attr[a] |= 0x20020; tmr[a] = 150; col[a] = 0; ev(0x32, a % w, a / w, 1); }
                    if (a < 0) { occ[i] = 0; bf[i] = 0; col[i] = 0; tmr[i] = 0; }
                    attr[i] &= ~0x1fu;
                }
            } else { attr[i] |= 0x20000; if (bf[i] & 4) attr[i] |= 0x20; tmr[i] = 150; col[i] = 0; ev(0x32, x, y, (bf[i] >> 2) & 1); }
        }
        if (any) { prevRows = 0; return false; }
        // 3) mechas (attr 0x20000) y temporizadores de fila
        bool busy = false;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int i = idx(x, y);
            if (!(attr[i] & 0x20000)) { if (tmr[i] >= 0.1f) { tmr[i] -= dtms; busy = true; if (tmr[i] <= 0) tmr[i] = 0; } }
            else { busy = true; tmr[i] -= dtms; if (tmr[i] <= 0) {
                attr[i] = (attr[i] & ~0x20000u) | 0x40000; tmr[i] = 30; uint32_t lvl = attr[i] & 0x1f; int ax, ay, aw, ah; bool big = attr[i] & 0x20;
                if (!big) { lvl += chain; if (lvl > 0x14) lvl = 0x14; area((int)lvl, ax, ay, aw, ah); } else area(0, ax, ay, aw, ah);
                ev(0x33, x, y, big, x + ax, y + ay, x + ax + aw, y + ay + ah); } }
        }
        if (busy) return false;
        // 4) tras la oleada: los bloques sin apoyo caen y sube el bonus de cadena
        if (chainPending) { if (label(0) > 0) { falling = true; } else falling = true; chainPending = false; chain++; return false; }
        // 5) filas completas: las bombas de esas filas se encienden con nivel = nº de filas completas
        int cnt = 0, bombRows = 0;
        for (int y = 0; y < h; y++) { bool full = true, bomb = false; for (int x = 0; x < w; x++) { int k = idx(x, y); if (!occ[k]) { full = false; break; } if (bf[k] & 6) bomb = true; } if (full) { cnt++; if (bomb) bombRows++; } }
        if (cnt > 0x14) cnt = 0x14;
        bool fired = false;
        if (cnt != 0) {
            if (cnt <= prevRows && bombRows == 0) { combine(); return true; }
            prevRows = cnt; ev(0x31, 0, 0, cnt);
            for (int y = 0; y < h; y++) {
                bool full = true; for (int x = 0; x < w; x++) if (!occ[idx(x, y)]) { full = false; break; } if (!full) continue;
                for (int x = 0; x < w; x++) {
                    int k = idx(x, y); tmr[k] = 500;
                    if (bf[k] & 2) { attr[k] = (attr[k] & ~0x1fu) | 0x20000; if (bf[k] & 4) attr[k] |= 0x20; fired = true; attr[k] |= cnt & 0x1f; col[k] = 0; chainPending = true; ev(0x32, x, y, (bf[k] >> 2) & 1); }
                    else if (bf[k] & 4) {
                        int a = findAnchor(x, y, false);
                        if (a >= 0 && !(attr[a] & 0x20000)) {
                            attr[a] = (attr[a] & ~0x1fu) | 0x20020 | (cnt & 0x1f); fired = true; tmr[a] = 500; chainPending = true;
                            int ax_ = a % w, ay_ = a / w; for (auto dd : {std::pair<int,int>{0, 0}, {1, 0}, {0, 1}, {1, 1}}) if (in(ax_ + dd.first, ay_ + dd.second)) col[idx(ax_ + dd.first, ay_ + dd.second)] = 0;
                            ev(0x32, ax_, ay_, 1);
                        }
                    }
                }
            }
            if (fired) return false;
        }
        chain = 0; combine();
        bool empty = true; for (size_t i = 0; i < N; i++) if (occ[i]) { empty = false; break; }
        if (empty) { cleared = true; g->state = 1; }   // evento 0x35: fase superada
        return true;
    }

    void update(float dt) {
        if (finished) return; seRow = seExp = seComb = false;
        if (g->state == 2) { failed = true; finishTime = g->gameTime; finished = true; return; }
        if (!g->hasPiece && g->state == 0) {
            float ms = std::min(dt, 0.05f) * 1000.0f; ready = tick(ms);
            if (g->state == 1) { cleared = true; finalPoints = points(); finishTime = g->gameTime; finished = true; sync(); return; }
            if (ready && points() <= 0) { failed = true; finishTime = g->gameTime; g->state = 2; finished = true; }  // sin piezas: fin [inferido]
        }
        sync();
    }

    // ---- dibujo (FUN_8009eb58): bloques, bombas pequeñas/grandes, caida suave y efectos ----
    void drawField(view::GameView& v, float fx0, float fy0, float bs, int topRow, float dt) override {
        for (int y = topRow; y < h; y++) for (int x = 0; x < w; x++) {
            int i = idx(x, y); if (!occ[i]) continue; float px = fx0 + x * bs, py = fy0 + (y - topRow) * bs - off[i] / 256.0f * bs;
            bool lit = (attr[i] & 0x60000) != 0 || (tmr[i] > 0.1f && !col[i]); bool blink = lit && ((int)(tmr[i] / 60.0f) & 1);
            Uint8 al = 255; SDL_Rect s;
            if (bf[i] & 4) {  // bomba grande: 4 cuartos de (192,0,32,32)
                int qx = 0, qy = 0; if ((bf[i] & 6) != 6) { int a = findAnchor(x, y, true); if (a < 0) continue; qx = x - a % w; qy = y - a / w; }
                s = {192 + qx * 16, qy * 16, 16, 16}; if (blink) al = 140; v.blit(v.atlas, s, px, py, bs, bs, al); continue;
            }
            if (bf[i] & 2) { s = {176, 0, 16, 16}; if (blink) al = 140; v.blit(v.atlas, s, px, py, bs, bs, al); continue; }
            if (!col[i]) continue;
            v.minoRect(((col[i] - 1) % 18) + 1, s); v.blit(v.atlas, s, px, py, bs, bs);
        }
        for (size_t k = 0; k < fx.size();) {
            Fx& f = fx[k]; f.age += dt; float life = f.kind == 1 ? 0.35f : f.kind == 0 ? 0.5f : 0.25f;
            if (f.age > life) { fx.erase(fx.begin() + k); continue; }
            float a = 1.0f - f.age / life; int x0 = std::max(0, f.x0), x1 = std::min(w, f.x1), y0 = std::max(topRow, f.y0), y1 = std::min(h, f.y1);
            if (x1 > x0 && y1 > y0) { if (f.kind == 1) v.fillRect(fx0 + x0 * bs, fy0 + (y0 - topRow) * bs, (x1 - x0) * bs, (y1 - y0) * bs, 255, 150, 40, (Uint8)(120 * a));
                else if (f.kind == 2) v.fillRect(fx0 + x0 * bs, fy0 + (y0 - topRow) * bs, bs, bs, 255, 255, 220, (Uint8)(200 * a));
                else if (f.kind == 0) v.fillRect(fx0 + x0 * bs, fy0 + (y0 - topRow) * bs, (x1 - x0) * bs, bs, 255, 255, 255, (Uint8)(90 * a));
                else v.fillRect(fx0 + x0 * bs, fy0 + (y0 - topRow) * bs, 2 * bs, 2 * bs, 255, 255, 255, (Uint8)(150 * a)); }
            k++;
        }
    }
};

} // namespace game
