// Motor de juego de Tetris Party Deluxe reconstruido (clases te*: tePlayfield, teController, tePhase*).
// Cada función indica la del binario original de la que procede.
#pragma once
#include "tpf2.hpp"
#include "script.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <deque>
#include <string>
#include <functional>

namespace te {

// ---- Generador aleatorio (FUN_800d30e4 siembra, FUN_800d5098 extrae) -----------------------
struct Rng {
    uint16_t t[17]; int i0 = 0, i1 = 10;
    void seed(uint32_t s) {
        uint32_t v = s & 0xffff;
        for (int k = 0; k < 17; k++) { t[k] = (uint16_t)v; v = (((v & 0xffff) << 5) | (v >> 11)) & 0xffff; v = (v + 0x61) & 0xffff; }
        i0 = 0; i1 = 10;
        for (int k = 0; k < 100; k++) next();
    }
    uint32_t next() {  // devuelve 0..2047
        uint16_t a = t[i0], b = t[i1];
        uint32_t v = (((a & 0x1fff) << 3) | (a >> 13)) + (((b & 0x7ff) << 5) | (b >> 11));
        t[i0] = (uint16_t)v;
        i0 = i0 > 0 ? i0 - 1 : 16; i1 = i1 > 0 ? i1 - 1 : 16;
        return v & 0x7ff;
    }
};

// ---- Pieza (definición) -----------------------------------------------------------------
struct TetDef {
    const tpf::Tetrimino* src = nullptr;
    int marks = 0;             // marcas válidas (+0x32c)
    uint8_t rowmask[4] = {};   // filas con celdas por orientación (+0x32d)
    // FUN_800e98bc: celda (i,j) del grid 7x7 girado
    uint32_t cell(int i, int j, int dir) const {
        if (i < 0 || i > 6 || j < 0 || j > 6) return 0;
        // El fichero guarda la rejilla con x variando más deprisa: grid[y][x] (FUN_800e93bc). i = x, j = y.
        const auto& g = src->grid;
        switch (dir) { case 0: return g[j][i]; case 1: return g[6 - i][j]; case 2: return g[6 - j][6 - i]; case 3: return g[i][6 - j]; }
        return 0;
    }
    int mx(int dir, int k) const { return src->markx[dir][k]; }
    int my(int dir, int k) const { return src->marky[dir][k]; }
    int mz(int dir, int k) const { return src->markz[dir][k]; }
    void build(const tpf::Tetrimino& t, int ver) {
        src = &t;
        if (ver < 3) { marks = 0; for (int k = 0; k < 5; k++) if (t.markz[0][k] != 0xff) marks = k + 1; }
        else marks = t.marks;
        for (int d = 0; d < 4; d++) { rowmask[d] = 0; for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (cell(i, j, d)) { rowmask[d] |= 1 << j; break; } }
    }
};

struct Cell { uint32_t mino = 0; uint8_t flags = 0; uint8_t shadow = 0; };  // flags: 0x80 marcado, 0x40 muerto, 0x20 tocado, 0x01 fresco; shadow: color de silueta (modo Sombra, 0 = ninguna)

struct Input {  // teController (vtable de +0x1b0)
    virtual ~Input() {}
    virtual void poll() {}
    virtual bool left() = 0; virtual bool right() = 0; virtual bool rotA() = 0; virtual bool rotB() = 0;
    virtual bool soft() = 0; virtual bool hard() = 0; virtual bool hold() = 0;
};

struct Client {  // lo que el motor comunica hacia fuera (vtable de playfield+8 y FUN_8006ec9c)
    virtual ~Client() {}
    virtual void event(int code) {}                      // FUN_8006ec9c: 1 líneas, 3 hold, 8 move-bad, 0xc rotate-bad, 0xd hard, 0x10 spawn, 0x11 cambio, 0x12 nivel
    virtual void clientLine(const std::string& raw) {}   // líneas '!' de los scripts (estadísticas)
    virtual void win() {} virtual void lose() {} virtual void levelUp(int level) {}
    virtual void sound(const std::string& name) {}
    // marcador del glue (FUN_80067554): líneas borradas (0..4), spin (0 no, 1 completo, 2 mini), back-to-back, all clear, combo (nº de despejes seguidos - 1)
    virtual void clearEvent(int lines, int spin, bool b2b, bool allClear, int combo) {}
};

struct Piece {  // objeto pieza activa (playfield+0x198)
    const TetDef* tet = nullptr;
    int x = 0; float y = 0; int dir = 0;
    int lockResetY = 0;   // +0x28
    int moveCount = 0;    // +0x2c
    int mode = 0;         // +0x24: 1 normal, 2 soft, 3 hard
    bool rotated = false, touched = false, blockedL = false, blockedR = false, blockedA = false, blockedB = false;
    int floorY() const { int v = (int)y; if (y < 0.0f) v--; return v; }
};

enum Phase { P_NONE = 0, P_GEN = 1, P_FALL = 2, P_LOCK = 3, P_PATTERN = 4, P_ITER = 5, P_ANIM = 6, P_ELIM = 7, P_SOLID = 8, P_GRAV = 9, P_COMP = 10 };

class Game : public rules::ScriptHost {
public:
    const tpf::Rules* R = nullptr;
    Input* in = nullptr; Client* cl = nullptr;
    int w = 10, h = 20;
    std::vector<Cell> cells;
    std::vector<TetDef> tets;
    Rng rng;
    // bolsa (FUN_800e5bf4) y cola de siguientes (FUN_800dee10)
    struct BagEntry { uint32_t id; int count, def; };
    std::vector<BagEntry> bag; bool bagSticky = true;
    std::deque<uint32_t> queue; int previewLen = 5;
    // estado
    Piece piece; bool hasPiece = false;
    uint32_t heldId = 0; bool holdUsed = false; bool holdAllowed = true; bool holdPending = false;
    int level = 1, lines = 0, cascades = 0; int scoreVal = 0, gain = 0, goal = 0;
    int levelIndex = 0; bool levelUpPending = false; bool repatternPending = false;
    int state = 0;  // +0x3a8: 1 ganado, 2 perdido
    // ajustes del cliente por modo (client+0x578 tipo de juego)
    float fallOverride = 0;  // >0: segundos por fila de la caída normal (Corredor de etapas, piece ctrl +0x194)
    bool linesEnabled = true, holdEnabled = true; int lockCount = 0; int pieceBudget = 0;  // pieceBudget: 0 = sin límite (+0x25e4)
    // marcador visible del glue (P+0x7c, FUN_80067554) y estadísticas
    int visScore = 0, combo = 0, b2bCount = 0, allClears = 0, singles = 0, doubles = 0, triples = 0, tetrises = 0; bool lastDifficult = false, lockRotated = false;
    // ganchos de modo (Bombliss): puerta de aparicion (false = esperar), aparicion y fijado de pieza
    std::function<bool()> spawnGate; std::function<void()> onSpawn, onLock;
    int levelBase = 0;  // AJUSTES: NIV inicial (nivel inicial - 1); el nivel sube cada 10 líneas a partir del inicial [inferido: notes_modes_basic §3.4 "nivel = inicial + líneas/10"]
    int lineGoal = 0;  // G+0x574: meta de líneas (150 Maratón, 40 Sprint; 0 = sin meta)
    void addScore(int v) { if (v > 100000) v = 100000; visScore += v; if (visScore > 999999999) visScore = 999999999; }  // Sombra: sin borrado ni hold, cuenta de piezas fijadas (+0x25e0)
    float gameTime = 0, levelTime = 0, phaseTime = 0;
    float scratch[100] = {}; float phaseVars[100] = {}; float pfVars[100] = {}; float plVars[100] = {}; float clVars[100] = {};
    float minoVar[8] = {}; int minoX = 0, minoY = 0; const Cell* curMino = nullptr;
    int phase = P_NONE; float budget = 0;
    bool patternEnabled = false, gravityEnabled = false;
    // DAS (FUN_800d7fd0...): temporizadores en frames
    int dasFirst = 15, dasRepeat = 3, rotFirst = 7158, rotRepeat = 7158;
    int tL = 0, tR = 0, tA = 0, tB = 0; bool hL = false, hR = false, hA = false, hB = false; bool hardPrev = false; int hardPend = 0;  // caída dura: flanco de pulsación con memoria de 15 frames (se consume al aplicarse)
    float lockTimer = 0, genTimer = 0, fallAcc = 0;
    int lastHeight = 0;
    // scripts compilados
    rules::Script sGamesec, sLevelsec, sStart, sTopout, sPcev[12], sLvlStart, sLvlEnd;
    struct PhaseScripts { rules::Script begin, end; } ps[11];
    rules::Script sCollapse, sElimFall, sElimLand, sGravLand, sSolidIter, sIter[2];
    std::vector<rules::Script> sLevelBegin, sLevelEnd;
    rules::Expr eDelay, eStartX, eStartY, eFall, eSoft, eTol, eHard, eLockDelay, eExt, eElimFall, eGravFall, eHold, eHardDrop, eIterRun[2];
    std::string lastError;

    // ---- inicialización (FUN_800d30e4 + carga de TPF: FUN_8005fda8) ----
    void init(const tpf::Rules& rules, uint32_t seed) {
        R = &rules; w = R->fldd.width; h = R->fldd.height * 2; cells.assign(w * h, Cell());  // FUN_800d2d64: alto x2 (zona oculta arriba)
        rng.seed(seed);
        tets.clear(); tets.reserve(R->tetr.size());
        int tver = 2; for (auto& t : R->tetr) { tets.emplace_back(); tets.back().build(t, tver); }
        // scripts
        sGamesec.compile(R->data.gamesec); sLevelsec.compile(R->data.levelsec); sStart.compile(R->data.start); sTopout.compile(R->data.topout);
        for (int i = 0; i < 12; i++) sPcev[i].compile(R->pcev.s[i]);
        const tpf::Phase* ph[11] = {nullptr, &R->pgen, &R->pfal, &R->plok, &R->ppat, &R->ptr8, &R->panm, &R->pelm, &R->psol, &R->pgvt, &R->pcmp};
        for (int i = 1; i <= 10; i++) { ps[i].begin.compile(ph[i]->begin); ps[i].end.compile(ph[i]->end); }
        sCollapse.compile(R->pelm.collapse); sElimFall.compile(R->pelm.fall); sElimLand.compile(R->pelm.land); sGravLand.compile(R->pgvt.land);
        sSolidIter.compile(R->psol.minoiterate); for (int i = 0; i < 2; i++) { sIter[i].compile(R->ptr8.it[i].minoiterate); if (R->ptr8.it[i].run) eIterRun[i].compile(R->ptr8.it[i].run_exp); }
        sLvlStart.compile(R->lvls.start); sLvlEnd.compile(R->lvls.end);
        sLevelBegin.clear(); sLevelEnd.clear(); for (auto& l : R->lvls.items) { sLevelBegin.emplace_back(l.begin); sLevelEnd.emplace_back(l.end); }
        eDelay.compile(R->pgen.delay); eStartX.compile(R->pgen.start_x); eStartY.compile(R->pgen.start_y);
        eFall.compile(R->pfal.fall_speed); eSoft.compile(R->pfal.soft_speed); eTol.compile(R->pfal.tolerance); eHard.compile(R->pfal.hard_speed);
        eLockDelay.compile(R->plok.delay); if (!R->plok.extended_count.empty()) eExt.compile(R->plok.extended_count);
        eElimFall.compile(R->pelm.fall_speed); eGravFall.compile(R->pgvt.fall_speed);
        if (!R->ctrl.enable_hold.empty()) eHold.compile(R->ctrl.enable_hold); if (!R->ctrl.enable_hard_drop.empty()) eHardDrop.compile(R->ctrl.enable_hard_drop);
        // controles (FUN_800e5590): segundos * 60 → frames
        dasFirst = (int)(60.0f * R->ctrl.f[0]); dasRepeat = (int)(60.0f * R->ctrl.f[1]); rotFirst = (int)(60.0f * R->ctrl.f[2]); rotRepeat = (int)(60.0f * R->ctrl.f[3]);
        // bolsa por defecto: las 7 piezas estándar con cuenta 1 (7-bag)
        bag.clear();
        for (auto& t : R->tetr) if (t.name.size() >= 11 && t.name[1] == ' ') bag.push_back({t.id, (int)t.flag, (int)t.flag});
        queue.clear();
        level = 1; levelBase = 0; lines = 0; scoreVal = 0; gain = 0; goal = 0; cascades = 0; levelIndex = 0; state = 0;
        gameTime = levelTime = phaseTime = 0; hasPiece = false; heldId = 0; holdUsed = false; holdAllowed = true; lockCount = 0; linesEnabled = true; holdEnabled = true; pieceBudget = 0; fallOverride = 0; visScore = combo = b2bCount = allClears = singles = doubles = triples = tetrises = 0; lastDifficult = lockRotated = false; lineGoal = 0;
        for (auto& v : pfVars) v = 0; for (auto& v : plVars) v = 0; for (auto& v : phaseVars) v = 0;
        // arranque (FUN_800d3b00 con param 0 → primer nivel)
        sStart.run(*this);
        sLvlStart.run(*this); if (!sLevelBegin.empty()) sLevelBegin[0].run(*this);
        levelTime = 0;
        setPhase(P_GEN);
    }

    // FUN_8006e030: nivel fijo (Escalador: nivel = etapa+1)
    void setLevel(int lv) {
        if (lv < 1) lv = 1; int idx = lv - 1; if (idx >= (int)sLevelBegin.size()) idx = (int)sLevelBegin.size() - 1; if (idx < 0) return;
        levelIndex = idx; level = idx + 1; levelTime = 0; levelUpPending = false;
        sLvlStart.run(*this); sLevelBegin[levelIndex].run(*this);
    }

    // ---- tick por frame (FUN_800d36cc) ----
    void update(float dt) {
        if (state) return;
        budget += dt;
        if (in) in->poll();
        { bool h = in && in->hard(); if (h && !hardPrev) hardPend = 15; else if (hardPend > 0) hardPend--; hardPrev = h; }
        tickDas();
        int guard = 0;
        while (budget >= 1e-6f && state == 0 && guard++ < 64) {
            float used = phaseUpdate(budget);
            if (used < 0) used = 0; if (used > budget) used = budget;
            float prev = gameTime; gameTime += used; budget -= used;
            if ((int)prev < (int)gameTime) sGamesec.run(*this);
            prev = levelTime; levelTime += used; phaseTime += used;
            if ((int)prev < (int)levelTime) sLevelsec.run(*this);
            if (used <= 0 && budget < 1.0f / 60.0f) break;
        }
    }

    // ---- consultas para el frontal ----
    const Cell& at(int x, int y) const { static Cell none; if (x < 0 || x >= w || y < 0 || y >= h) return none; return cells[x + y * w]; }
    int ghostY() const { if (!hasPiece) return 0; int dy = 0; while (collide(0, dy + 1, -1) == 0 && dy < h + 4) dy++; return piece.floorY() + dy; }
    // ---- consultas/acciones para el Corredor de etapas ----
    int pieceTopRow() const { if (!hasPiece) return h; const TetDef& t = *piece.tet; int ay = t.marks ? t.my(piece.dir, 0) : 0, best = h; for (int j = 0; j < 7; j++) if (t.rowmask[piece.dir] & (1 << j)) { int cy = j + piece.floorY() - ay; if (cy < best) best = cy; } return best; }
    int pieceBottomRow() const { return hasPiece ? lowestRow() : -1; }
    bool pieceOnBlock() const { return hasPiece && collide(0, 1, -1) == 1; }
    bool pieceOverlaps() const { return hasPiece && collide(0, 0, -1) == 1; }
    void pushPieceUp() { if (!hasPiece) return; piece.y -= 1.0f; if (cl) cl->event(0x11); }  // el scroll empuja la pieza (FUN_80062e78)
    bool tryMoveUp() {  // FUN_800ec0dc(client,0,0,-1): sube la pieza una fila y vuelve a la fase de caída
        if (!hasPiece || (phase != P_FALL && phase != P_LOCK) || collide(0, -1, -1) != 0) return false;
        piece.y -= 1.0f; piece.touched = false; if (cl) cl->event(0x11); if (phase == P_LOCK) setPhase(P_FALL); return true;
    }
    const TetDef* tetById(uint32_t id) const { for (auto& t : tets) if (t.src->id == id) return &t; return nullptr; }

    // ---- VarProvider / ScriptHost ----
    float get(char kind, int idx) override {
        switch (kind) {
        case 's': return idx < 100 ? scratch[idx] : 0;
        case 'p': return idx < 100 ? phaseVars[idx] : 0;
        case 'f': if (idx < 100) return pfVars[idx]; if (idx == 100) return (float)level; if (idx == 101) return (float)lines; if (idx == 102) return (float)stackHeight(); if (idx == 103) return heldId ? (float)heldId : 0; if (idx == 104) return (float)cascades; return 0;
        case 'u': if (idx < 100) return plVars[idx]; if (idx == 100) return (float)scoreVal; if (idx == 101) return (float)goal; if (idx == 102) return (float)gain; return 0;
        case 't': return idx == 0 ? gameTime : idx == 1 ? levelTime : phaseTime;
        case 'P': if (!hasPiece) return 0; switch (idx) { case 0: return (float)piece.x; case 1: return piece.y; case 2: return (float)ghostY(); case 3: return (float)piece.dir; case 4: return (float)piece.tet->src->id; case 5: return piece.rotated ? 1.0f : 0.0f; } return 0;
        case 'm': if (idx < 100) return (curMino && curMino->mino) ? minoF(curMino->mino, idx) : 0; if (idx == 100) return (float)minoX; if (idx == 101) return (float)minoY; if (idx == 102) return curMino ? ((curMino->flags & 0x08) ? 1.0f : 0.0f) : 0; if (idx == 103) return curMino ? ((curMino->flags & 0x04) ? 1.0f : 0.0f) : 0; if (idx == 104) return curMino ? ((curMino->flags & 0x02) ? 1.0f : 0.0f) : 0; if (idx == 105) return curMino ? (float)curMino->mino : 0; return 0;
        case 'c': return idx < 100 ? clVars[idx] : 0;
        case 'x': return (float)(rng.next() & 0x7ff) / 2048.0f;
        case 'S': return 0;  // estadísticas del cliente: las lleva el cliente
        }
        return 0;
    }
    void setVar(char kind, int idx, float v) override {
        if (idx >= 100) return;
        switch (kind) { case 's': scratch[idx] = v; break; case 'p': phaseVars[idx] = v; break; case 'f': pfVars[idx] = v; break; case 'u': plVars[idx] = v; break; case 'c': clVars[idx] = v; break; }
    }
    void setGoal(float v) override { goal = (int)v; }
    void score(float v) override { scoreVal += (int)v; gain += (int)v; }
    void scoreReset() override { scoreVal = 0; }
    void win() override { state = 1; if (cl) cl->win(); }
    void lose() override { state = 2; if (cl) cl->lose(); }
    void nextLevel() override { if (lines / 10 < level - levelBase) return; levelUpPending = true; }  // FUN_800d3a6c
    void clientLine(const std::string& raw) override { if (cl) cl->clientLine(raw); }
    void command(rules::Cmd c, const std::string& name, float a, float b, int n) override {
        using namespace rules;
        switch (c) {
        case SOUND: case LOOP_SOUND: case MUSIC: case LOOP_MUSIC: if (cl) cl->sound(name); break;
        case SET_PATTERN: patternEnabled = (a != 0); break;
        case SET_GRAVITY: gravityEnabled = (a != 0); break;
        case HOLD: holdPending = true; break;
        case HOLD_RESET: doHold(); break;
        case KILL_MINO: if (curMino) { const_cast<Cell*>(curMino)->flags |= 0x40; repatternPending = true; } break;
        case MARK_MINO: if (curMino) const_cast<Cell*>(curMino)->flags |= 0x08; break;
        case MARK2_MINO: if (curMino) const_cast<Cell*>(curMino)->flags |= 0x04; break;
        case MARK_RESET: for (auto& cc : cells) cc.flags &= ~0x08; break;
        case MARK2_RESET: for (auto& cc : cells) cc.flags &= ~0x04; break;
        case ITERATE: iterateRequested = true; break;
        case SET_BAG: { int keep = (int)a; if (keep < 0) keep = 0; while ((int)queue.size() > keep) queue.pop_back(); fillQueue(); break; }
        default: break;  // comandos de otros modos (garbage, trash, shapes…) aún no implementados
        }
    }

private:
    bool iterateRequested = false;

    float minoF(uint32_t id, int k) const { for (auto& m : R->mino.items) if (m.id == id) return k < 10 ? m.f[k] : 0; return 0; }
    int stackHeight() const { for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (cells[x + y * w].mino) return h - y; return 0; }  // FUN_800d6f8c

    // ---- fases (FUN_800d3e54 + enter/exit) ----
    void setPhase(int p) {
        if (p == phase) return;
        if (phase) ps[phase].end.run(*this);
        phase = p; phaseTime = 0;
        ps[phase].begin.run(*this);
        phaseEnter();
    }
    void phaseEnter() {
        switch (phase) {
        case P_GEN: genTimer = 0; break;                                       // FUN_800db7ac: holdAllowed = 1
        case P_FALL: piece.mode = 1; fallAcc = 0; break;                       // FUN_800dbbdc
        case P_LOCK: lockTimer = 0; break;
        case P_ELIM: elimBegin(); break;                                       // FUN_800dc134 → FUN_800dc2ec
        case P_COMP: compBegin(); break;                                       // FUN_800dc534
        default: break;
        }
        if (phase == P_GEN) holdAllowed = true;
    }
    float phaseUpdate(float dt) {
        switch (phase) {
        case P_GEN: return genUpdate(dt);
        case P_FALL: return fallUpdate(dt);
        case P_LOCK: return lockUpdate(dt);
        case P_PATTERN: setPhase(P_ITER); return 0;                            // FUN_800da61c (patrones desactivados)
        case P_ITER: iterUpdate(); setPhase(P_ANIM); return 0;                 // FUN_800dae50
        case P_ANIM: setPhase(P_ELIM); return 0;                               // FUN_800dc714
        case P_ELIM: return elimUpdate(dt);                                    // FUN_800dc1b0
        case P_SOLID: setPhase(P_GRAV); return 0;                              // FUN_800da0cc (solo calcula adyacencias)
        case P_GRAV: if (!repatternPending) setPhase(P_COMP); else { repatternPending = false; setPhase(P_PATTERN); } return 0;  // FUN_800db218
        case P_COMP: return 0;
        }
        return dt;
    }

    // ---- generación (FUN_800db4dc) ----
    float genUpdate(float dt) {
        if (pieceBudget > 0 && lockCount >= pieceBudget) return dt;  // presupuesto agotado (Sombra): no nacen más piezas
        if (spawnGate && !spawnGate()) return dt;
        float delay = eDelay.eval(*this);
        if (delay > genTimer + dt) { genTimer += dt; return dt; }
        float used = delay - genTimer; if (used < 0) used = 0;
        float sx = eStartX.eval(*this), sy = eStartY.eval(*this);
        spawn((int)(0.5f + sx), (int)sy + h / 2);  // FUN_800db4dc: y = start_y + alto/2
        if (cl) cl->event(0x10);
        if (onSpawn) onSpawn();
        // subir si choca (FUN_800d9864 código 2/3 ajusta en x; aquí se sube en y)
        int tries = 0, c;
        while ((c = collide(0, 0, -1)) != 0 && tries < 8) { if (c == 2) piece.x++; else if (c == 3) piece.x--; else if (c == 1) piece.y -= 1.0f; else break; tries++; }
        piece.lockResetY = piece.floorY(); piece.moveCount = 0;
        if (collide(0, 0, -1) != 0) { lose(); return used; }
        sPcev[0].run(*this);  // generated
        setPhase(P_FALL);
        return used;
    }
    void spawn(int x, int y) {
        uint32_t id = nextFromQueue();
        piece = Piece(); piece.tet = tetById(id); if (!piece.tet) piece.tet = &tets[0];
        piece.x = x; piece.y = (float)y; piece.dir = 0; hasPiece = true;
        holdUsed = false;
    }

    // ---- bolsa (FUN_800e5bf4) y cola (FUN_800dee10) ----
    uint32_t drawFromBag() {
        int sum = 0; for (auto& e : bag) sum += e.count;
        if (sum <= 0) { for (auto& e : bag) e.count = e.def; sum = 0; for (auto& e : bag) sum += e.count; if (sum <= 0) return bag.empty() ? 0 : bag[0].id; }
        int r = (int)(rng.next() % (uint32_t)sum);
        for (auto& e : bag) { r -= e.count; if (r < 0) { if (bagSticky) e.count = e.count > 0 ? e.count - 1 : 0; return e.id; } }
        return bag[0].id;
    }
    void fillQueue() { while ((int)queue.size() < previewLen) queue.push_back(drawFromBag()); }
    uint32_t nextFromQueue() { fillQueue(); uint32_t id = queue.front(); queue.pop_front(); fillQueue(); return id; }

    // ---- colisión (FUN_800d9864): 0 libre, 1 bloque, 2 pared izq, 3 pared der, 4 suelo ----
    int collide(int dx, int dy, int dir) const {
        if (!hasPiece) return 0;
        const TetDef& t = *piece.tet; if (dir < 0) dir = piece.dir;
        int ax = t.marks ? t.mx(dir, 0) : 0, ay = t.marks ? t.my(dir, 0) : 0;
        int fy = piece.floorY();
        for (int j = 0; j < 7; j++) {
            if (!(t.rowmask[dir] & (1 << j))) continue;
            int cy = j + dy + fy - ay;
            for (int i = 0; i < 7; i++) {
                if (!t.cell(i, j, dir)) continue;
                int cx = i + dx + piece.x - ax;
                if (cx < 0) return 2; if (cx >= w) return 3; if (cy >= h) return 4;
                if (cy >= 0 && cells[cx + cy * w].mino) return 1;
            }
        }
        return 0;
    }
    int lowestRow() const {  // FUN_800d9740: fila más baja ocupada por la pieza
        const TetDef& t = *piece.tet; int ay = t.marks ? t.my(piece.dir, 0) : 0; int best = -1000;
        for (int j = 0; j < 7; j++) if (t.rowmask[piece.dir] & (1 << j)) { int cy = j + piece.floorY() - ay; if (cy > best) best = cy; }
        return best;
    }
    int visibleTop() const { return h / 2; }

    // ---- mando con DAS (FUN_800d7f40, FUN_800d7fd0…) ----
    void tickDas() { if (tL > 0) tL--; if (tR > 0) tR--; if (tA > 0) tA--; if (tB > 0) tB--; }
    bool repeatBtn(bool pressed, int& timer, bool& held, int first, int rep) {
        if (!pressed) { timer = 0; held = false; return false; }
        if (timer >= 1) return false;
        if (!held) { timer = first; held = true; } else timer = rep;
        return true;
    }
    bool btnLeft() { return in && hasPiece && repeatBtn(in->left(), tL, hL, dasFirst, dasRepeat); }
    bool btnRight() { return in && repeatBtn(in->right(), tR, hR, dasFirst, dasRepeat); }
    bool btnRotA() { return in && repeatBtn(in->rotA(), tA, hA, rotFirst, rotRepeat); }
    bool btnRotB() { return in && repeatBtn(in->rotB(), tB, hB, rotFirst, rotRepeat); }
    bool btnHard() { return in && hardPend > 0 && (eHardDrop.empty() || eHardDrop.eval(*this) != 0); }  // FUN_800d8278
    bool btnHold() { return in && holdAllowed && in->hold() && (eHold.empty() || eHold.eval(*this) != 0); } // FUN_800d82dc

    // ---- movimiento (FUN_800d89e0 / FUN_800d8b6c) ----
    bool moveX(int dx, bool useTol, bool& blockedFlag) {
        bool ok = collide(dx, 0, -1) == 0;
        if (ok && useTol) {
            float frac = piece.y - (float)piece.floorY();
            if (eTol.eval(*this) < frac && collide(dx, 1, -1) != 0) ok = false;
        }
        if (ok) {
            piece.touched = false; piece.x += dx; piece.rotated = false; piece.moveCount++;
            sPcev[1].run(*this); if (cl) cl->event(0x11);
            return true;
        }
        if (!blockedFlag) { if (cl) cl->event(8); sPcev[2].run(*this); blockedFlag = true; }
        return false;
    }
    // ---- rotación con marcas (FUN_800d901c) ----
    bool rotate(int nd, bool& blockedFlag) {
        const TetDef& t = *piece.tet; int od = piece.dir;
        int ox = t.marks ? t.mx(od, 0) : 0, oy = t.marks ? t.my(od, 0) : 0, nx = t.marks ? t.mx(nd, 0) : 0, ny = t.marks ? t.my(nd, 0) : 0;
        for (int k = 0; k < t.marks; k++) {
            int z = t.mz(od, k), dx = 0, dy = 0;
            if (z != 0 && z < t.marks) { dx = (t.mx(od, z) - ox) + (nx - t.mx(nd, z)); dy = (t.my(od, z) - oy) + (ny - t.my(nd, z)); }
            if (collide(dx, dy, nd) == 0) {
                piece.x += dx; piece.y += (float)dy;
                int fy = (int)piece.y; if (fy > piece.lockResetY) { piece.lockResetY = fy; piece.moveCount = 0; }
                if (cl) cl->event(0x11);
                piece.dir = nd; piece.rotated = true; piece.touched = false;
                phaseVars[0] = (float)z; phaseVars[1] = (float)dx; phaseVars[2] = (float)dy;
                sPcev[3].run(*this); piece.moveCount++;
                return true;
            }
        }
        if (!blockedFlag) { if (cl) cl->event(0xc); sPcev[4].run(*this); blockedFlag = true; }
        return false;
    }

    // ---- caída (FUN_800dbc40 + FUN_800d92d4) ----
    bool handleInput(bool inLock) {
        bool moved = false;
        if (btnHold()) { doHold(); return true; }
        if (btnLeft()) moved |= moveX(-1, !inLock, piece.blockedL); else piece.blockedL = false;
        if (btnRight()) moved |= moveX(1, !inLock, piece.blockedR); else piece.blockedR = false;
        if (btnRotA()) moved |= rotate((piece.dir + 1) & 3, piece.blockedA); else piece.blockedA = false;
        if (btnRotB()) moved |= rotate((piece.dir + 3) & 3, piece.blockedB); else piece.blockedB = false;
        return moved;
    }
    float fallUpdate(float dt) {
        if (!hasPiece) { setPhase(P_GEN); return 0; }
        if (piece.mode != 3) {
            if (handleInput(false) && !hasPiece) return 0;  // hold cambió de fase
            if (phase != P_FALL) return 0;
            int mode = 1; if (in && in->soft()) mode = 2;
            if (btnHard()) { hardPend = 0; mode = 3; }
            piece.mode = mode;
        }
        float speed = piece.mode == 1 ? (fallOverride > 0 ? fallOverride : eFall.eval(*this)) : piece.mode == 2 ? eSoft.eval(*this) : eHard.eval(*this);
        if (piece.mode != lastMode) { if (piece.mode == 2) sPcev[6].run(*this); else if (piece.mode == 3) { sPcev[8].run(*this); if (cl) cl->event(0xd); } lastMode = piece.mode; }
        float used = dt; bool landed = gravityStep(speed, used);
        if (landed) {
            if (piece.mode == 3) { lockPiece(); setPhase(P_PATTERN); }
            else setPhase(P_LOCK);
        }
        return used;
    }
    int lastMode = 0;
    // FUN_800d92d4: avanza y en subpasos menores que una fila; devuelve si ha aterrizado
    bool gravityStep(float secPerRow, float& dt) {
        if (secPerRow <= 0.0f) secPerRow = 0.0001f;
        float rows = dt / secPerRow;
        int n = 1; while (rows / (float)n >= 1.0f) n++;
        float step = rows / (float)n, subdt = dt / (float)n;
        for (int k = 0; k < n; k++) {
            int before = piece.floorY();
            piece.y += step;
            int fy = (int)piece.y; if (fy > piece.lockResetY) { piece.lockResetY = fy; piece.moveCount = 0; }
            if (collide(0, 0, -1) != 0) { piece.y = (float)(piece.floorY() - 1); if (piece.floorY() > piece.lockResetY) { piece.lockResetY = piece.floorY(); piece.moveCount = 0; } }
            if (piece.floorY() > before) { if (piece.mode == 2) addScore(piece.floorY() - before); else if (piece.mode == 3) addScore(2 * (piece.floorY() - before)); piece.rotated = false; sPcev[piece.mode == 1 ? 5 : piece.mode == 2 ? 7 : 9].run(*this); if (cl) cl->event(0x11); }
            if (collide(0, 1, -1) != 0) { piece.y = (float)piece.floorY(); dt = subdt * (float)(k + 1); if (!piece.touched) { piece.touched = true; sPcev[10].run(*this); } return true; }
        }
        return false;
    }
    // ---- bloqueo (FUN_800da8b4) ----
    float lockUpdate(float dt) {
        bool moved = handleInput(true);
        if (phase != P_LOCK) return 0;
        bool forced = false;
        if (btnHard()) { hardPend = 0; forced = true; if (cl) cl->event(0xd); }
        if (R->plok.type == 1 && !eExt.empty() && (float)piece.moveCount >= eExt.eval(*this)) forced = true;
        if (moved) {
            if (collide(0, 1, -1) == 0) { setPhase(P_FALL); return 0; }   // vuelve a caer
            if (R->plok.type != 0) lockTimer = 0;                           // reinicio del lock delay (FUN_800d3f28)
        }
        float delay = eLockDelay.eval(*this);
        if (delay <= lockTimer + dt || forced) {
            float used = forced ? 0 : (delay - lockTimer); if (used < 0) used = 0;
            lockPiece(); setPhase(P_PATTERN); return used;
        }
        lockTimer += dt; return dt;
    }
    // FUN_800d7ec8 / FUN_800d9c30: fija la pieza en el campo
    void lockPiece() {
        if (!hasPiece) return;
        const TetDef& t = *piece.tet; int ax = t.marks ? t.mx(piece.dir, 0) : 0, ay = t.marks ? t.my(piece.dir, 0) : 0; int fy = piece.floorY();
        for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) { uint32_t m = t.cell(i, j, piece.dir); if (!m) continue; int cx = i + piece.x - ax, cy = j + fy - ay; if (cx >= 0 && cx < w && cy >= 0 && cy < h) { cells[cx + cy * w].mino = m; cells[cx + cy * w].flags = 0x01; } }
        lockRotated = piece.rotated; sPcev[11].run(*this); lockCount++; if (onLock) onLock();
        int low = lowestRow();
        if (low < h / 2) sTopout.run(*this);  // FUN_800d3a00
        hasPiece = false;
        if (low < h / 2) lose();              // lock out: la pieza queda entera en la zona oculta (regla del cliente)
    }
    // ---- hold (FUN_800d7028) ----
    void doHold() {
        if (!hasPiece || holdUsed || !holdEnabled) return;
        if (cl) cl->event(3);
        uint32_t cur = piece.tet->src->id;
        if (heldId == 0) { heldId = cur; hasPiece = false; setPhase(P_GEN); genTimer = 1e9f; }
        else { uint32_t id = heldId; heldId = cur; queue.push_front(id); hasPiece = false; setPhase(P_GEN); genTimer = 1e9f; }
        holdAllowed = false; holdUsed = true;
        sPcev[5].run(*this);
    }
    // ---- iterar (FUN_800dae50): ejecuta minoiterate sobre cada mino ----
    void iterUpdate() {
        for (int k = 0; k < 2; k++) {
            if (sIter[k].empty()) continue;
            if (R->ptr8.it[k].run && !eIterRun[k].empty() && eIterRun[k].eval(*this) == 0) continue;
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { Cell& c = cells[x + y * w]; if (!c.mino) continue; curMino = &c; minoX = x; minoY = y; sIter[k].run(*this); }
            curMino = nullptr;
        }
    }
    // ---- eliminar (FUN_800dc2ec): filas completas ----
    void elimBegin() {
        if (cl) cl->event(1);
        bool any = false; int cleared = 0;
        for (int y = 0; y < h; y++) {
            bool full = true, anyCell = false;
            for (int x = 0; x < w; x++) { Cell& c = cells[x + y * w]; if (!c.mino) full = false; else { anyCell = true; if (c.flags & 0x40) { c.mino = 0; c.flags = 0; full = false; } } }
            if (anyCell && full && linesEnabled) { removeRow(y); lines++; cleared++; sCollapse.run(*this); any = true; }
        }
        if (any) cascades++;
        for (auto& c : cells) c.flags &= ~0x01;
        scoreLines(cleared);
    }
    // FUN_80067554: puntuación por despeje (base 100/300/500/800; giro 400/800/1200/1600; B2B x1.5; all clear +800..+2000; x nivel; combo +50·nivel·(n-1))
    void scoreLines(int n) {
        if (n <= 0) { combo = 0; if (lockRotated) { if (cl) cl->clearEvent(0, 1, false, false, 0); } return; }
        static const int base[5] = {0, 100, 300, 500, 800}, spinB[4] = {400, 800, 1200, 1600};
        bool spin = lockRotated; int b = spin ? spinB[n - 1 < 3 ? n - 1 : 3] : base[n > 4 ? 4 : n];
        bool difficult = spin || n >= 4; bool b2b = difficult && lastDifficult; if (b2b) { b = b * 3 / 2; b2bCount++; }
        lastDifficult = difficult;
        bool allClear = true; for (auto& c : cells) if (c.mino) { allClear = false; break; }
        if (allClear) { static const int ac[5] = {0, 800, 1200, 1600, 2000}; b += ac[n > 4 ? 4 : n]; allClears++; }
        addScore(b * level);
        combo++; if (combo > 1) addScore(50 * level * (combo - 1));
        if (n == 1) singles++; else if (n == 2) doubles++; else if (n == 3) triples++; else tetrises++;
        if (cl) cl->clearEvent(n, spin ? 1 : 0, b2b, allClear, combo - 1);
    }
    void removeRow(int row) {  // FUN_800d5110: baja todo lo de arriba una fila
        for (int y = row; y > 0; y--) for (int x = 0; x < w; x++) cells[x + y * w] = cells[x + (y - 1) * w];
        for (int x = 0; x < w; x++) cells[x] = Cell();
    }
    float elimUpdate(float dt) { setPhase(P_SOLID); return 0; }  // fall_speed = 0: caída instantánea
    // ---- completado (FUN_800dc534) ----
    void compBegin() {
        cascades = 0;
        if (levelUpPending) { levelUpPending = false; doLevelUp(); if (cl) cl->event(0x12); }
        if (state == 0) setPhase(P_GEN);
    }
    void doLevelUp() {  // FUN_800d3b00
        if (levelIndex < (int)sLevelEnd.size()) sLevelEnd[levelIndex].run(*this);
        sLvlEnd.run(*this);
        if (state) return;
        levelIndex++; level++;
        if (levelIndex >= (int)sLevelBegin.size()) { win(); return; }
        if (cl) cl->levelUp(level);
        levelTime = 0;
        sLvlStart.run(*this); sLevelBegin[levelIndex].run(*this);
    }
};

} // namespace te
