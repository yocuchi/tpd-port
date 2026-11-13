// Modo Escalador (CFlowClimberGame, flow 4, modo 2) y objeto CClimber ("clibmer"): notes_modes_special.md §2.
// Funciones originales: FUN_800ab6b0 (IA), FUN_800aad20 (búsqueda), FUN_800ab080/ab330 (ruta / vagar), FUN_800aa720 (basura),
// FUN_800b45dc (banderas), FUN_800b0af0 / FUN_800b0260 (juego / meta).
#pragma once
#include "../engine/engine.hpp"
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <cstring>

namespace game {

struct ClimberData {  // data/climber.bin (tools/extract_climber.py)
    int32_t garbage[10]; uint8_t flagCount[10]; int32_t dirs[16]; float c[12]; uint8_t ft[4][2]; int32_t startX, startY, goalRow, flagRow;
    std::vector<std::string> anims; bool ok = false;
    // c: 0 correr ms (200), 1 escalar ms (1000), 2 espera base (500), 3 recarga máx (20000), 4 bonus bandera (10000), 5 oxígeno máx (40000),
    //    6 umbral 0.5, 7 factor caída 2.0, 8 rearme 201, 9/10 dt mín/máx ms, 11 100
    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
        if (d.size() < 8 + 40 + 10 + 64 + 48 + 8 + 16 + 1 || memcmp(d.data(), "CLMB", 4)) return false;
        size_t o = 8; memcpy(garbage, &d[o], 40); o += 40; memcpy(flagCount, &d[o], 10); o += 10; memcpy(dirs, &d[o], 64); o += 64; memcpy(c, &d[o], 48); o += 48;
        memcpy(ft, &d[o], 8); o += 8; int32_t v[4]; memcpy(v, &d[o], 16); o += 16; startX = v[0]; startY = v[1]; goalRow = v[2]; flagRow = v[3];
        int n = d[o++]; anims.clear(); for (int i = 0; i < n && o + 40 <= d.size(); i++) { anims.emplace_back((const char*)&d[o]); o += 40; }
        ok = true; return true;
    }
};

enum ClimberState { CS_STAND = 1, CS_RUN = 2, CS_FALL = 4, CS_CLIMB = 8, CS_FLY = 0x10, CS_DEAD = 0x1000000, CS_GOAL = 0x2000000 };
struct ClimberFlag { int x = 0, y = 0, state = 0; };  // state 0 libre, 1 activa, 2 cogida

class ClimberMode {
public:
    te::Game* g = nullptr; const ClimberData* D = nullptr; int stage = 0;
    // ---- objeto CClimber ----
    int state = CS_STAND; int cx = 5, cy = 39, tx = 5, ty = 39, fx = 5, fy = 39;  // +0x20/24 actual, +0x18/1c destino inmediato, +0x10/14 destino final
    bool pathMode = false, active = true, oxygenOn = false, wander = true;       // bits 30, 31, 26, 27 de +0x148
    float stun = 0, timer = 0, duration = 200, climbDur = 1000, recharge = 0, oxygen = 0, progress = 0;
    int record = 0;
    ClimberFlag flags[16];
    // ---- flujo ----
    bool allFlags = false, cleared = false, failed = false, finished = false; float stageMs = 0, accumMs = 0;
    int deadPhase = 0; float deadTimer = 0, deadVel = 0, deadRise = 0; bool deadAnimDone = false;  // vista de la muerte (FUN_800c9034)
    float goalTimer = 0; bool goalRunning = false;
    std::function<void(int)> onSe; std::function<void()> onFlagTaken;
    bool dangerOn = false;

    uint32_t rs = 12345; int rnd() { rs = rs * 0x41c64e6d + 0x3039; return (rs >> 16) & 0x7fff; }

    // ---------- inicio de etapa (FUN_800b2440 / b5388 / b5228 / b45dc / b44d4) ----------
    void start(te::Game& game, const ClimberData& data, int st, uint32_t seed, bool withOxygen = false) {
        g = &game; D = &data; stage = st < 0 ? 0 : (st > 9 ? 9 : st); rs = seed * 2654435761u + 1;
        g->setLevel(stage + 1); g->linesEnabled = true;
        state = CS_STAND; cx = tx = fx = D->startX; cy = ty = fy = D->startY; pathMode = false; active = true; wander = true; stun = 0; timer = 0; duration = D->c[0]; climbDur = D->c[1];
        progress = 0; record = 0; recharge = 0; oxygenOn = withOxygen; oxygen = withOxygen ? D->c[5] : 0; dangerOn = false;
        cleared = failed = finished = false; allFlags = false; stageMs = 0; deadPhase = 0; deadTimer = deadVel = deadRise = 0; goalRunning = false; goalTimer = 0; lastPx = -99;
        placeGarbage(); placeFlags();
    }
    // FUN_800aa720: por columna (salvo la del escalador) (nivel-1) + rand % (2*(nivel-1)+4) filas desde abajo, color rand%6+2
    void placeGarbage() {
        int lv = D->garbage[stage]; if (lv == 0) return; int base = lv - 1, mod = base * 2 + 4;
        for (int x = 0; x < g->w; x++) {
            if (x == D->startX) continue; int n = base + rnd() % mod;
            for (int i = 0; i < n; i++) { int y = g->h - i - 1; if (y < 0) break; te::Cell& c = g->cells[x + y * g->w]; c.mino = rnd() % 6 + 2; c.flags = 0; }
        }
    }
    // FUN_800b45dc (individual): nº por etapa, tipo barajado 50 veces, x por tipo, y = rand%10 + 5 (+0x14)
    void placeFlags() {
        for (auto& f : flags) f = ClimberFlag(); int n = D->flagCount[stage]; int perm[4] = {0, 1, 2, 3};
        for (int i = 0; i < 50; i++) { int k = rnd() % 3 + 1; std::swap(perm[0], perm[k]); }
        for (int i = 0; i < n; i++) { int t = perm[i]; flags[i].x = D->ft[t][0] + rnd() % D->ft[t][1]; flags[i].y = rnd() % 10 + D->flagRow + g->h / 2; flags[i].state = 1; }
    }

    // ---------- consultas de celdas ----------
    bool valid(int x, int y) const { return x >= 0 && x < g->w && y >= 0 && y < g->h; }
    std::vector<char> pc;  // celdas de la pieza activa (flag 0x10)
    void buildPiece() {
        pc.assign(g->w * g->h, 0); if (!g->hasPiece) return;
        const te::TetDef& t = *g->piece.tet; int ax = t.marks ? t.mx(g->piece.dir, 0) : 0, ay = t.marks ? t.my(g->piece.dir, 0) : 0, fy_ = g->piece.floorY();
        for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (t.cell(i, j, g->piece.dir)) { int x = i + g->piece.x - ax, y = j + fy_ - ay; if (valid(x, y)) pc[x + y * g->w] = 1; }
    }
    bool pieceCell(int x, int y) const { return valid(x, y) && !pc.empty() && pc[x + y * g->w]; }
    bool blk(int x, int y) const { return valid(x, y) && (g->cells[x + y * g->w].mino || pieceCell(x, y)); }   // flags & 0x38 / 0x58 / 0x78
    bool freeC(int x, int y) const { return valid(x, y) && !blk(x, y); }                                        // celda existente y sin bloque
    bool fresh(int x, int y) const { return valid(x, y) && (g->cells[x + y * g->w].flags & 1); }                  // 0x20000000

    // ---------- búsqueda (FUN_800aad20) ----------
    std::vector<char> vis;
    int search(int x, int y, int& nx, int& ny, int& gx, int& gy, int& gs, int steps) {
        if (!valid(x, y)) return g->h;
        vis[x + y * g->w] = 1;
        if (y < gy || (gy == y && steps < gs)) { gx = x; gy = y; gs = steps; }
        int bnx = nx, bny = ny, top = y - 1; if (freeC(x, top)) top--;
        int best = y, bestSteps = steps;
        for (int side = -1; side < 2 && y > 0x14; side += 2) {
            int xx = x + side, r9 = top;
            do { r9++; if (r9 >= g->h) break; } while (!freeC(xx, r9));
            if (r9 >= g->h) continue;
            for (int r11 = r9; r11 < g->h + 1 && r9 <= y; r11++) {
                if (!freeC(xx, r11)) {
                    if (valid(xx, r11 - 1) && !vis[xx + (r11 - 1) * g->w]) {
                        int r = search(xx, r11 - 1, nx, ny, gx, gy, gs, steps + 1);
                        if (r < best || (r == best && gs < bestSteps)) { bestSteps = gs; bnx = xx; bny = r11 - 1; best = r; }
                    }
                    break;
                }
            }
        }
        nx = bnx; ny = bny; return best;
    }
    // FUN_80157d0c: ¿hay una bandera activa siguiendo el contorno del suelo hacia un lado? (izquierda primero)  [inferido en detalle]
    bool flagHint(int& outX) const {
        auto marked = [&](int x, int y) { for (auto& f : flags) if (f.state == 1 && f.x == x && f.y == y) return true; return false; };
        auto occ = [&](int x, int y) { return valid(x, y) && g->cells[x + y * g->w].mino; };
        for (int dir = -1; dir <= 1; dir += 2) {
            int y = cy;
            for (int x = cx; x >= 0 && x < g->w; x += dir) {
                int nx = x + dir;
                bool a = valid(nx, y - 1) && !occ(nx, y - 1), b = valid(nx, y), bo = occ(nx, y), c = valid(x, y - 1) && !occ(x, y - 1);
                if (a && c && (!b || bo)) { y--; if (marked(nx, y - 0) || marked(x, y)) { outX = nx; return true; } continue; }
                if (b && !bo && (!valid(nx, y + 1) || occ(nx, y + 1))) { if (marked(nx, y)) { outX = nx; return true; } continue; }
                if (valid(nx, y + 1) && !occ(nx, y + 1) && valid(nx, y + 2) == false) break;
                if (valid(nx, y + 1) && !occ(nx, y + 1) && (!valid(nx, y + 2) || occ(nx, y + 2)) && b && !bo) { y++; if (marked(nx, y) || marked(nx, y - 1)) { outX = nx; return true; } continue; }
                break;
            }
        }
        return false;
    }
    // FUN_800ab080: continuar una ruta de varios pasos
    bool continuePath() {
        if (!pathMode || fx == cx || state == CS_FALL) return false;
        if (valid(cx, cy + 1) && !blk(cx, cy + 1)) return false;
        int dir = cx < fx ? 1 : -1; int hx = 0; bool near = false; if (flagHint(hx)) near = (dir > 0 && cx < hx) || (dir < 0 && hx < cx);
        bool found = false; int ux = 0, uy = 0;
        for (int y = cy - (near ? 1 : 0); !found && y < cy + 2; y++) if (freeC(cx + dir, y) && (!valid(cx + dir, y + 1) || blk(cx + dir, y + 1))) { found = true; ux = cx + dir; uy = y; }
        if (found && !(freeC(ux, uy - 1) && freeC(ux, uy) && (!valid(ux, uy + 1) || blk(ux, uy + 1)))) found = false;
        if (found) { if (uy < cy) { tx = ux; ty = uy; state = CS_RUN; duration = climbDur; } else { tx = ux; ty = cy; state = CS_RUN; } return true; }
        timer = 0; duration = D->c[0]; tx = fx = cx; ty = fy = cy; state = CS_STAND; pathMode = false; return false;
    }
    // FUN_800ab330: sin mejor destino -> caminar hacia una bandera / al azar, o esperar 500-700 ms
    void doWander() {
        if (!wander) return;
        if (blk(cx + 1, cy) == false && freeC(cx + 1, cy)) {} // (se mantiene el orden del original: celdas vecinas)
        auto bl = [&](int x, int y) { return !valid(x, y) || blk(x, y); };
        if (bl(cx + 1, cy) && bl(cx, cy + 1) && bl(cx - 1, cy) && bl(cx, cy - 1)) { timer = 0; duration = D->c[0]; state = CS_STAND; tx = fx = cx; ty = fy = cy; pathMode = false; return; }
        int dir; bool hint = false; int hx = 0;
        if (flagHint(hx)) { hint = true; dir = cx < hx ? 1 : -1; } else dir = (rnd() & 1) ? -1 : 1;
        int x = cx, row = -2, cur = cy;
        while (dir != 0) {
            bool ok = false;
            for (;;) {
                x += dir; if (x < 0 || x >= g->w) break; bool step = false;
                for (int r = cur - 1; r < cur + 2; r++) {
                    if ((r != cur - 1 || freeC(x - dir, r)) && freeC(x, r) && (!valid(x, r + 1) || blk(x, r + 1))) { step = true; row = r; cur = r; }
                }
                if (!step) break; ok = true;
            }
            if (row != -2 || hint) {
                if (x < 0) x = 0; if (x >= g->w) x = g->w - 1;
                fx = x; fy = row; tx = cx + dir; ty = row < cy ? row : cy; pathMode = true; return;
            }
            dir = 0; (void)ok;
        }
        tx = fx = cx; ty = fy = cy; duration = D->c[0] * (rnd() & 1) + D->c[2]; pathMode = true;
    }

    // ---------- IA por frame (FUN_800ab6b0) ----------
    void step(float dt) {
        if (state & (CS_DEAD | CS_GOAL)) return;
        recharge += dt; if (recharge > D->c[3]) recharge = D->c[3]; if (stun >= 0) stun -= dt;
        int d[16]; memcpy(d, D->dirs, 64);
        if (rnd() & 1) { d[2] = -1; d[4] = 1; } else { d[2] = 1; d[4] = -1; }
        if (rnd() & 1) { d[6] = -1; d[8] = 1; } else { d[6] = 1; d[8] = -1; }
        if (rnd() & 1) { d[12] = -1; d[14] = 1; } else { d[12] = 1; d[14] = -1; }
        if (oxygenOn) { oxygen -= dt; if (oxygen < 0) { state = CS_DEAD; return; } }
        // aplastado: destino y celda actual ocupados -> teletransporte a una celda libre vecina
        if (valid(tx, ty) && blk(tx, ty) && !fresh(tx, ty) && (state & 0x1f) && valid(cx, cy) && blk(cx, cy)) {
            for (int i = 0; i < 8; i++) { int nx = cx + d[i * 2], ny = cy + d[i * 2 + 1]; if (freeC(nx, ny)) { if (ny < 0) ny = 0; cx = tx = nx; cy = ty = ny; timer = 0; duration = D->c[0]; state = CS_STAND; break; } }
        }
        // banderas
        for (auto& f : flags) if (f.state == 1 && f.x == cx && f.y == cy) { f.state = 2; if (oxygenOn) { oxygen += D->c[4]; if (oxygen > D->c[5]) oxygen = D->c[5]; } }
        // escalada interrumpida por techo
        if ((state & CS_CLIMB) && timer <= duration * D->c[6] && freeC(cx, cy) && (!valid(cx, cy - 1) || blk(cx, cy - 1))) { timer = 0; duration = D->c[0]; state = CS_STAND; tx = fx = cx; ty = fy = cy; pathMode = false; }
        timer += (state & CS_FALL) ? D->c[7] * dt : dt;
        // caer
        if (state != CS_FALL && state != CS_CLIMB && valid(cx, cy + 1) && !blk(cx, cy + 1)) { ty = cy + 1; timer = 0; duration = D->c[0]; state = CS_FALL; }
        // lanzado por la pieza activa que se mueve bajo el escalador  [inferido: dirección = movimiento lateral de la pieza]
        if (pieceCell(cx, cy + 1) && pieceDx != 0 && freeC(cx + pieceDx, cy)) { cx += pieceDx; tx = cx; state = CS_FLY; timer = 0; duration = D->c[0]; }
        if ((state & 0xb) && !(valid(tx, ty) && !blk(tx, ty))) { tx = cx; ty = cy; state = CS_STAND; timer = 0; }
        if (pathMode && (state == CS_STAND || state == CS_RUN) && ty < cy && (tx + 1 == cx || tx - 1 == cx)) { if (!valid(tx, ty + 1) || blk(tx, ty + 1)) state = CS_CLIMB; }
        if (duration <= timer) {
            timer = 0; duration = D->c[0]; state = CS_STAND; cx = tx; cy = ty; bool skip = false;
            if (pathMode) { if (tx == fx) pathMode = false; else if (continuePath()) skip = true; }
            if (!skip && active && !(valid(cx, cy + 1) && !blk(cx, cy + 1))) {
                vis.assign(g->w * g->h, 0); int nx = cx, ny = cy, gx = cx, gy = cy, gs = 0;
                search(cx, cy, nx, ny, gx, gy, gs, 0);
                if (nx == cx && ny == cy) doWander();
                else { tx = nx; if (ny < cy) ty = ny; if (cx != nx) { if (ny < cy) { state = CS_CLIMB; duration = climbDur; } else state = CS_RUN; } }
            }
        }
        // empotrado en bloques sin línea completa cerca: buscar celda libre; si no hay, muere
        if ((state & 3) && tx == cx && ty == cy && valid(cx, cy) && blk(cx, cy)) {
            bool full = false;
            for (int r = -1; r < 1 && !full; r++) { int n = 0; for (int x = 0; x < g->w; x++) if (valid(x, cy + r) && (blk(x, cy + r) || fresh(x, cy + r))) n++; full = (n == g->w); }
            if (!full) {
                int i = 0; for (; i < 8; i++) { int nx = cx + d[i * 2], ny = cy + d[i * 2 + 1]; if (freeC(nx, ny)) { cx = tx = nx; cy = ty = ny; timer = 0; break; } }
                if (i == 8 && stun <= 0) state = CS_DEAD;
            }
        }
        progress = timer / duration;
        int hgt = (g->h - cy) - 1; if (hgt > 9999) hgt = 9999; if (record < hgt) record = hgt;
        active = state != CS_CLIMB;  // FUN_800c9034 (vista): bit 31 solo fuera de la escalada
    }
    int pieceDx = 0, lastPx = -99; uint32_t lastPiece = 0;

    // ---------- PlayUpdate (FUN_800b0af0 / FUN_800b0260) ----------
    void update(float dtSec) {
        if (finished) return;
        float dt = dtSec * 1000.0f; if (dt < D->c[9]) dt = D->c[9]; if (dt > D->c[10]) dt = D->c[10];
        buildPiece();
        if (g->hasPiece) { pieceDx = lastPx == -99 || lastPiece != (uint32_t)(uintptr_t)g->piece.tet->src ? 0 : g->piece.x - lastPx; lastPx = g->piece.x; lastPiece = (uint32_t)(uintptr_t)g->piece.tet->src; } else { pieceDx = 0; lastPx = -99; }
        if (state == CS_DEAD) { deathStep(dtSec); return; }
        if (goalRunning) { goalTimer += dtSec; if (goalTimer > 3.0f) { cleared = true; finished = true; } return; }
        if (g->state == 2) { failed = true; finished = true; return; }
        stageMs += dtSec * 1000.0f; step(dt);
        if (state == CS_DEAD) return;
        // meta (1P): parado, suelo sólido, todas las banderas y fila < 0x16
        if (state == CS_STAND && valid(cx, cy + 1) && blk(cx, cy + 1) && allFlags && cy < D->goalRow) {
            if (onSe) onSe(0x7f); state = CS_GOAL; goalRunning = true; goalTimer = 0; accumMs += stageMs; return;
        }
        // banderas: cogidas -> sonido y se liberan; si no queda ninguna activa -> todas cogidas
        bool anyActive = false;
        for (auto& f : flags) { if (f.state == 2) { if (onSe) onSe(0x73); if (onFlagTaken) onFlagTaken(); f.state = 0; } else if (f.state == 1) anyActive = true; }
        if (!anyActive) allFlags = true;
        if (oxygenOn) { bool d = oxygenPercent() < 31; if (d && !dangerOn && onSe) onSe(0x50); dangerOn = d; }
    }
    float oxygenPercent() const { if (!oxygenOn) return 0; float v = 100.0f * oxygen / D->c[5]; return v < 0 ? 0 : v > 100 ? 100 : v; }
    int flagsLeft() const { int n = 0; for (auto& f : flags) if (f.state == 1) n++; return n; }
    // vista de la muerte: dead00 -> dead01 -> asciende (vel += 1, y += 2 + vel/10 por frame hasta 300) -> fin de partida
    void deathStep(float dtSec) {
        deadTimer += dtSec;
        if (deadPhase == 0) { deadPhase = 1; deadTimer = 0; }
        else if (deadPhase == 1) { if (deadAnimDone || deadTimer > 2.0f) { deadPhase = 2; deadTimer = 0; deadAnimDone = false; } }
        else if (deadPhase == 2) { deadAccum += dtSec * 60.0f; while (deadAccum >= 1.0f) { deadAccum -= 1.0f; deadVel += 1.0f; deadRise += 2.0f + deadVel / 10.0f; } if (deadRise > 300.0f) { deadPhase = 3; failed = true; finished = true; } }
    }
    float deadAccum = 0;
};

} // namespace game
