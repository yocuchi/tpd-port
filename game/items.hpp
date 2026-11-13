// Objetos (ítems) de Tetris VS: lógica independiente de la vista. Fuente: tetris-decomp/notes_vs.md §4 (marcas [inferido] donde la nota no lo resuelve).
//
//  * Cada 11ª pieza de un jugador lleva UNA celda-objeto (marca al azar de la pieza; flag de celda 0x10, la sigue al fijarse y al caer las filas).
//  * Al borrar una fila con celdas-objeto sale un objeto por celda, al azar entre los activados; cola FIFO de 3 ranuras (llena: se descarta el más antiguo).
//  * Se usa el de la ranura 0: botón Y del mando / tecla A (el B del port es "rotar inverso"); ver ItemSystem::input.
//  * 13 objetos (ids 1 Humo, 2 Aceleración, 3 Disparar, 4 Patear, 5 Cascada, 6 Esconder, 8 Parar el tiempo, 9 Cambio, 10 Escalador, 12 Cerrar,
//    15 Confundir, 16 Amañar, 18 Bomba) con las duraciones de §4.4.
//
// Enganche (sin cambios en engine.hpp: start() ENCADENA g.onSpawn / g.onLock delante de los que ya haya, p. ej. VersusState::onLock):
//   main_sdl.cpp: items.start(ptrs) tras vsState.start(); items.input(i, ...) y items.update(dt) cada frame de juego; g.update(items.dt(i, dt));
//   items.stop() al salir del VS. Las celdas-objeto de las líneas completas se cuentan en onLock (antes de que el motor las borre).
// Objetivo: los de efecto sobre rivales (1,2,6,8,10,12,15,16) y Disparar bloques (3) usan a los rivales vivos que no sean del mismo equipo
// (sameTeam; por defecto todos contra todos); Patear (4), Cascada (5), Cambio (9) y Bomba (18) son de efecto propio. Sin puntero de Wii, la zona
// "apuntada" se aproxima con la columna de la pieza activa de quien lo usa [inferido].
#pragma once
#include "versus.hpp"
#include <functional>
#include <algorithm>
#include <cstdint>

namespace game {

enum ItemId { IT_NONE = 0, IT_SMOKE = 1, IT_ACCEL = 2, IT_SHOT = 3, IT_KICK = 4, IT_CASCADE = 5, IT_HIDE = 6, IT_STOP = 8, IT_SHIFT = 9, IT_CLIMB = 10, IT_LOCK = 12, IT_CONFUSE = 15, IT_FIX = 16, IT_BOMB = 18 };

// Tabla de objetos: id, textos de 009_config (nombre / ayuda), textura de icono item_NN (item.arc), SE al usarlo (0x5E genérico va aparte), bit de la máscara (id-1).
struct ItemInfo { int id, nameMsg, descMsg, icon, seUse; const char* name; };
static const ItemInfo ITEM_INFO[13] = {
    {1, 49, 59, 0, 0x5F, "Humo"}, {2, 50, 60, 7, 0x61, "Aceleracion"}, {3, 51, 61, 1, 0x78, "Disparar bloques"}, {4, 52, 62, 3, 0x63, "Patear linea"},
    {5, 53, 63, 4, 0x64, "Cascada"}, {6, 146, 147, 8, 0x65, "Esconder"}, {8, 57, 67, 5, 0x68, "Parar el tiempo"}, {9, 56, 66, 2, 0x67, "Cambio"},
    {10, 55, 65, 9, 0x5C, "Escalador"}, {12, 54, 64, 6, 0x66, "Cerrar"}, {15, 358, 361, 11, 0x8D, "Confundir"}, {16, 357, 360, 12, 0x8E, "Amanar"}, {18, 359, 362, 13, 0x8C, "Bomba"}};
inline const ItemInfo* itemInfo(int id) { for (const ItemInfo& i : ITEM_INFO) if (i.id == id) return &i; return nullptr; }

static const int ITEM_SE_USE = 0x5E, ITEM_SE_GET = 0x6C;
static const uint8_t CELL_ITEM = 0x10, CELL_BOMB = 0x02;  // flags de celda: marca de objeto / celda-bomba (en el original 0x100000 y 0x2)

// Eventos para la vista (ItemView los consume por jugador): los efectos persistentes la vista los lee del estado.
struct ItemFx { enum Type { Get, Use, Hit, SmokeStart, SmokeWipe, Kick, Shot, BombSet, BombBlast, Crush, Shift } type; int player, a, b, c; };

struct ItemSystem {
    static const int SLOTS = 3, PERIOD = 11, MAXP = 4;
    struct St {
        int slot[SLOTS] = {0, 0, 0}; float slotT[SLOTS] = {0, 0, 0};  // cola; slotT = retardo de uso de la CPU (5/3/2 s según ranura al obtenerlo)
        int pieceCnt = 0, markR = -1, markC = -1;                       // contador de piezas y celda marcada de la pieza activa (fila/columna de la rejilla 7x7 original)
        float tAccel = 0, tHide = 0, tFreeze = 0, tLock = 0, tConfuse = 0, tFix = 0;   // efectos recibidos (s restantes)
        float tCascade = 0, tShot = 0, tBomb = 0, tShift = 0;                          // efectos propios / ventanas de puntería
        int perm[5] = {0, 1, 2, 3, 4}; uint32_t fixType = 0; bool bagSaved = false; std::vector<te::Game::BagEntry> bagBackup; bool accelOn = false, dead = false;
        int shots = 0; float shotCd = 0;
        int smoke = 0; bool cloud[7] = {}; float cloudPts[7] = {}; float cpuWipe = 0;  // humo: nubes vivas y puntos de agitación acumulados por nube
        bool clAlive = false; int clX = 0, clY = 0, clDir = 1, clFrom = -1; float tClimb = 0, clStep = 0; bool clMoving = false; int clFace = 1;  // escalador
        int got = 0, used = 0;
        bool busy() const { return tCascade > 0 || tShot > 0 || tBomb > 0 || tShift > 0; }
    };
    struct Wrap : te::Input {  // intermediario de entrada: confundir (permutación de ←,→,rotar A,rotar B,bajar) y cerrar (sin rotación)
        ItemSystem* sys = nullptr; int idx = 0; te::Input* inner = nullptr;
        void poll() override { if (inner) inner->poll(); }
        bool raw(int k) { if (!inner) return false; switch (k) { case 0: return inner->left(); case 1: return inner->right(); case 2: return inner->rotA(); case 3: return inner->rotB(); } return inner->soft(); }
        bool map(int k) { St& s = sys->S[idx]; return raw(s.tConfuse > 0 ? s.perm[k] : k); }
        bool left() override { return map(0); } bool right() override { return map(1); }
        bool rotA() override { return sys->S[idx].tLock <= 0 && map(2); } bool rotB() override { return sys->S[idx].tLock <= 0 && map(3); }
        bool soft() override { return map(4); } bool hard() override { return inner && inner->hard(); } bool hold() override { return inner && inner->hold(); }
    };

    std::vector<PlayerBase*> P; St S[MAXP]; Wrap W[MAXP];
    bool active = false;                                   // start()..stop(): los ganchos del motor solo actúan con la partida VS en curso
    bool enabled = true; uint32_t mask = 0x7FFFFu;       // bit b <=> objeto con id b+1 (igual que los 19 flags del original); por defecto todos los de menú activos
    bool cpu[MAXP] = {};                                  // jugadores controlados por la CPU: usan objetos solos y se quitan el humo solos
    std::function<void(int)> onSe;                         // efectos de sonido (ids del BRSAR)
    std::function<bool(int, int)> sameTeam;                // equipos (por defecto PlayerBase::team: mismo color = mismo equipo)
    std::function<void(int victim, int lines, int from)> onGarbage;  // escalador aplastado: basura al campo (main_sdl lo enlaza a VersusState::addGarbage; por defecto GarbageItem propios)
    std::vector<ItemFx> fx; uint32_t rng = 0x1234567u; int garbageSent[MAXP] = {};

    // ---------- utilidades ----------
    int n() const { return (int)P.size(); }
    uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    void se(int id) { if (onSe) onSe(id); }
    void emit(ItemFx::Type t, int p, int a = 0, int b = 0, int c = 0) { if (fx.size() > 256) fx.erase(fx.begin(), fx.begin() + 128); fx.push_back({t, p, a, b, c}); }
    te::Game& G(int i) { return P[i]->g; }
    static uint32_t defaultMask() { return 0x7FFFFu; }
    std::vector<int> enabledIds() const { std::vector<int> v; if (!enabled) return v; for (const ItemInfo& i : ITEM_INFO) if (mask & (1u << (i.id - 1))) v.push_back(i.id); return v; }
    bool alive(int i) const { return i >= 0 && i < n() && P[i]->alive && P[i]->g.state == 0; }
    std::vector<int> rivals(int from) const { std::vector<int> v; for (int j = 0; j < n(); j++) if (j != from && alive(j) && !(sameTeam ? sameTeam(from, j) : P[from]->team == P[j]->team)) v.push_back(j); return v; }
    int pickRival(int from) { auto r = rivals(from); return r.empty() ? -1 : r[rnd() % r.size()]; }
    float dt(int i, float d) const { return (i >= 0 && i < MAXP && S[i].tFreeze > 0) ? 0.0f : d; }  // parar el tiempo: el motor del rival no avanza

    // ---------- ciclo de vida ----------
    void start(const std::vector<PlayerBase*>& players) {
        P = players; fx.clear(); active = true; rng ^= (uint32_t)(uintptr_t)this * 2654435761u; if (!rng) rng = 1;
        for (int i = 0; i < n() && i < MAXP; i++) {
            clearEffects(i); S[i] = St(); te::Game& g = G(i); g.fallOverride = 0; W[i].sys = this; W[i].idx = i; garbageSent[i] = 0;
            cpu[i] = P[i]->cpu; hook(g.onSpawn, i, false); hook(g.onLock, i, true);  // ganchos del motor, delante de los existentes
            ensureWrap(i);
        }
    }
    // Fin de la partida VS (o al empezar otro modo): quita efectos, devuelve la entrada original a los motores y desactiva los ganchos.
    void stop() { for (int i = 0; i < n() && i < MAXP; i++) { clearEffects(i); te::Game& g = G(i); if (g.in == &W[i]) g.in = W[i].inner; } active = false; fx.clear(); }
    struct Hook {  // g.onSpawn / g.onLock: primero este sistema y luego el gancho que hubiera (target<Hook>() evita encadenar dos veces)
        ItemSystem* s; int i; bool lock; std::function<void()> prev;
        void operator()() const { if (lock) s->onLock(i); else s->onSpawn(i); if (prev) prev(); }
    };
    void hook(std::function<void()>& f, int i, bool lock) { if (Hook* h = f.target<Hook>()) { h->s = this; h->i = i; h->lock = lock; return; } f = Hook{this, i, lock, f}; }
    void ensureWrap(int i) { te::Game& g = G(i); if (g.in != &W[i]) { W[i].inner = g.in; g.in = &W[i]; } }  // se re-engancha solo si otro código cambia g.in
    void clearEffects(int i) {
        if (i >= n()) return; St& s = S[i]; te::Game& g = G(i);
        if (s.bagSaved) { g.bag = s.bagBackup; s.bagSaved = false; }
        if (s.accelOn) { g.fallOverride = 0; s.accelOn = false; }
        s.tAccel = s.tHide = s.tFreeze = s.tLock = s.tConfuse = s.tFix = s.tCascade = s.tShot = s.tBomb = s.tShift = 0; s.smoke = 0; s.clAlive = false; s.tClimb = 0;
        for (int k = 0; k < 7; k++) s.cloud[k] = false;
    }

    // ---------- obtención ----------
    // Ganchos del motor: aparición (cuenta de piezas, marca), fijado (la marca pasa a la celda) y fila borrada (objetos por celda marcada, bombas).
    void onSpawn(int i) {
        if (!active || !enabled || enabledIds().empty()) return; St& s = S[i]; s.markR = s.markC = -1;
        if (++s.pieceCnt < PERIOD) return; s.pieceCnt = 0; forceMark(i);
    }
    void forceMark(int i) {  // marca una celda al azar de la pieza activa (FUN_8006b018)
        te::Game& g = G(i); if (!g.hasPiece) return; std::vector<std::pair<int, int>> cs; const auto& grid = g.piece.tet->src->grid;
        for (int r = 0; r < 7; r++) for (int c = 0; c < 7; c++) if (grid[r][c]) cs.push_back({r, c});
        if (cs.empty()) return; auto pc = cs[rnd() % cs.size()]; S[i].markR = pc.first; S[i].markC = pc.second;
    }
    // celda (i,j) del grid girado donde cae la celda marcada de la pieza (inversa de TetDef::cell) y su posición en el campo
    bool pieceMark(int i, int& cx, int& cy) const {
        const St& s = S[i]; const te::Game& g = P[i]->g; if (s.markR < 0 || !g.hasPiece) return false;
        int r = s.markR, c = s.markC, gi = 0, gj = 0;
        switch (g.piece.dir & 3) { case 0: gi = c; gj = r; break; case 1: gi = 6 - r; gj = c; break; case 2: gi = 6 - c; gj = 6 - r; break; default: gi = r; gj = 6 - c; break; }
        const te::TetDef& t = *g.piece.tet; int ax = t.marks ? t.mx(g.piece.dir, 0) : 0, ay = t.marks ? t.my(g.piece.dir, 0) : 0;
        cx = gi + g.piece.x - ax; cy = gj + g.piece.floorY() - ay; return true;
    }
    void onLock(int i) {  // pieza recién fijada: la marca pasa a la celda, Cascada fragmenta, y las líneas completas dan objetos / explotan bombas (el motor las borra después)
        if (!active) return; St& s = S[i]; te::Game& g = G(i); int mx, my;
        if (pieceMark(i, mx, my) && mx >= 0 && mx < g.w && my >= 0 && my < g.h) g.cells[mx + my * g.w].flags |= CELL_ITEM;
        s.markR = s.markC = -1;
        if (s.tCascade > 0) cascade(g);
        int marks = 0; std::vector<std::pair<int, int>> bombs;
        for (int y = 0; y < g.h; y++) {
            bool full = true; for (int x = 0; x < g.w && full; x++) if (!g.cells[x + y * g.w].mino) full = false; if (!full) continue;
            for (int x = 0; x < g.w; x++) { const te::Cell& c = g.cells[x + y * g.w]; if (c.flags & CELL_ITEM) marks++; if (c.flags & CELL_BOMB) bombs.push_back({x, y}); }
        }
        for (int k = 0; k < marks; k++) grant(i);   // una celda marcada por objeto (§4.2)
        for (auto& b : bombs) explode(i, b.first, b.second);
    }
    void push(int i, int id) {  // FUN_800781ac: al hueco libre; llena: se descarta el más antiguo
        St& s = S[i]; int k = 0; while (k < SLOTS && s.slot[k]) k++;
        if (k == SLOTS) { for (int q = 0; q < SLOTS - 1; q++) { s.slot[q] = s.slot[q + 1]; s.slotT[q] = s.slotT[q + 1]; } k = SLOTS - 1; }
        s.slot[k] = id; s.slotT[k] = k == 0 ? 5.0f : k == 1 ? 3.0f : 2.0f; s.got++;
        se(ITEM_SE_GET); emit(ItemFx::Get, i, k, id);
    }
    int grant(int i) { auto ids = enabledIds(); if (ids.empty()) return 0; int id = ids[rnd() % ids.size()]; push(i, id); return id; }
    int give(int i, int id) { push(i, id); return id; }

    // ---------- uso ----------
    // Botón de objeto (flanco): con ventana de Disparar/Bomba abierta dispara/coloca; si no, gasta el objeto de la ranura 0.
    bool use(int i) {
        if (!active || !enabled || !alive(i)) return false; St& s = S[i];
        if (s.tShot > 0) return fire(i); if (s.tBomb > 0) return placeBomb(i);
        if (s.busy() || s.tFreeze > 0 || !s.slot[0]) return false;
        int id = s.slot[0]; std::vector<int> rv = rivals(i);
        bool needRival = id == IT_SMOKE || id == IT_ACCEL || id == IT_HIDE || id == IT_STOP || id == IT_CLIMB || id == IT_LOCK || id == IT_CONFUSE || id == IT_FIX || id == IT_SHOT;
        if (needRival && rv.empty()) return false;
        for (int q = 0; q < SLOTS - 1; q++) { s.slot[q] = s.slot[q + 1]; s.slotT[q] = s.slotT[q + 1]; } s.slot[SLOTS - 1] = 0; s.slotT[SLOTS - 1] = 0; s.used++;
        se(ITEM_SE_USE); if (const ItemInfo* in = itemInfo(id)) se(in->seUse); emit(ItemFx::Use, i, id);
        apply(i, id, rv); return true;
    }
    void apply(int i, int id, const std::vector<int>& rv) {
        St& s = S[i];
        switch (id) {
        case IT_SMOKE: { int t = rv[rnd() % rv.size()]; St& v = S[t]; v.smoke = 7; for (int k = 0; k < 7; k++) { v.cloud[k] = true; v.cloudPts[k] = 0; } v.cpuWipe = 0; emit(ItemFx::SmokeStart, t, i); emit(ItemFx::Hit, t, id, i); break; }
        case IT_ACCEL: for (int t : rv) { S[t].tAccel = 5.0f; emit(ItemFx::Hit, t, id, i); } break;                       // 5 s a 0,02 s/fila
        case IT_SHOT: s.tShot = 5.0f; s.shots = 16; s.shotCd = 0; break;                                                  // ventana 5 s, hasta 16 bloques
        case IT_KICK: kickLine(i); break;
        case IT_CASCADE: s.tCascade = 7.5f; break;
        case IT_HIDE: { int t = rv[rnd() % rv.size()]; S[t].tHide = 10.0f; emit(ItemFx::Hit, t, id, i); break; }
        case IT_STOP: for (int t : rv) { S[t].tFreeze = 5.0f; emit(ItemFx::Hit, t, id, i); } break;
        case IT_SHIFT: s.tShift = 0.5f; break;                                                                             // ventana 0,5 s para elegir el lado (←/→)
        case IT_CLIMB: for (int t : rv) startClimber(t, i); break;
        case IT_LOCK: for (int t : rv) { S[t].tLock = 5.0f; emit(ItemFx::Hit, t, id, i); } break;
        case IT_CONFUSE: for (int t : rv) { St& v = S[t]; v.tConfuse = 10.0f; do { for (int k = 4; k > 0; k--) std::swap(v.perm[k], v.perm[rnd() % (k + 1)]); } while (isIdentity(v.perm)); emit(ItemFx::Hit, t, id, i); } break;
        case IT_FIX: for (int t : rv) fixPieces(t, i); break;
        case IT_BOMB: s.tBomb = 5.0f; break;                                                                               // ventana 5 s
        }
    }
    static bool isIdentity(const int* p) { for (int k = 0; k < 5; k++) if (p[k] != k) return false; return true; }

    // ---------- efectos con lógica de campo ----------
    static int surface(const te::Game& g, int x) { for (int y = 0; y < g.h; y++) if (g.cells[x + y * g.w].mino) return y - 1; return g.h - 1; }  // fila libre sobre la pila (-1: columna llena)
    static int pieceCenterCol(const te::Game& g) {  // columna central de la pieza activa (aproxima el puntero IR) [inferido]
        if (!g.hasPiece) return g.w / 2; const te::TetDef& t = *g.piece.tet; int ax = t.marks ? t.mx(g.piece.dir, 0) : 0, sum = 0, cnt = 0;
        for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (t.cell(i, j, g.piece.dir)) { sum += i + g.piece.x - ax; cnt++; }
        int c = cnt ? (int)((float)sum / cnt + 0.5f) : g.w / 2; return std::max(0, std::min(g.w - 1, c));
    }
    static void freePiece(te::Game& g) { for (int k = 0; k < 6 && g.hasPiece && g.pieceOverlaps(); k++) g.piece.y -= 1.0f; }  // si el campo recibe un bloque bajo la pieza, se la sube
    void startClimber(int t, int from) {
        St& v = S[t]; te::Game& g = G(t); v.clAlive = true; v.tClimb = 20.0f; v.clFrom = from; v.clX = (int)(rnd() % g.w); v.clDir = (rnd() & 1) ? 1 : -1; v.clStep = 0.4f; v.clFace = v.clDir;
        v.clY = std::max(g.h / 2, surface(g, v.clX)); emit(ItemFx::Hit, t, IT_CLIMB, from);
    }
    void fixPieces(int t, int from) {  // Amañar: la cola solo da un tipo de pieza
        St& v = S[t]; te::Game& g = G(t); if (!v.bagSaved) { v.bagBackup = g.bag; v.bagSaved = true; }
        const auto& src = v.bagBackup; if (src.empty()) return; uint32_t id = src[rnd() % src.size()].id; v.fixType = id; v.tFix = 10.0f;
        g.bag.clear(); g.bag.push_back({id, 1000000, 1000000}); for (auto& q : g.queue) q = id; emit(ItemFx::Hit, t, IT_FIX, from, (int)id);
    }
    void cascade(te::Game& g) {  // Cascada: las celdas de la pieza recién fijada caen sueltas y rellenan huecos (teGravity) [inferido: caída por columna]
        const te::TetDef& t = *g.piece.tet; int ax = t.marks ? t.mx(g.piece.dir, 0) : 0, ay = t.marks ? t.my(g.piece.dir, 0) : 0, fy = g.piece.floorY();
        std::vector<std::pair<int, int>> cs;
        for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (t.cell(i, j, g.piece.dir)) { int cx = i + g.piece.x - ax, cy = j + fy - ay; if (cx >= 0 && cx < g.w && cy >= 0 && cy < g.h) cs.push_back({cx, cy}); }
        std::sort(cs.begin(), cs.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.second > b.second; });
        for (auto& c : cs) { int x = c.first, y = c.second; while (y + 1 < g.h && !g.cells[x + (y + 1) * g.w].mino) { g.cells[x + (y + 1) * g.w] = g.cells[x + y * g.w]; g.cells[x + y * g.w] = te::Cell(); y++; } }
    }
    bool fire(int i) {  // Disparar bloques: un bloque gris sobre la pila de un rival, en la columna de la pieza activa (hasta 16 por ventana)
        St& s = S[i]; if (s.shots <= 0 || s.tShot <= 0) return false; int t = pickRival(i); if (t < 0) return false;
        te::Game& g = G(t); int col = pieceCenterCol(G(i)), y = surface(g, col); if (y < 0) return false;
        te::Cell c; c.mino = 1; c.flags = 0x01; g.cells[col + y * g.w] = c; freePiece(g); s.shots--; if (s.shots <= 0) s.tShot = 0;
        emit(ItemFx::Shot, t, col, y, i); se(0x78); return true;
    }
    bool placeBomb(int i) {  // Bomba: celda-bomba sobre la pila del propio campo; al borrarse su fila explota (3x3 alrededor) [inferido: área]
        St& s = S[i]; te::Game& g = G(i); int col = pieceCenterCol(g), y = surface(g, col); if (y < 0) return false;
        te::Cell c; c.mino = 9; c.flags = 0x01 | CELL_BOMB; g.cells[col + y * g.w] = c; freePiece(g); s.tBomb = 0; emit(ItemFx::BombSet, i, col, y); se(0x8C); return true;
    }
    void explode(int i, int bx, int y) {
        te::Game& g = G(i); for (int dy = -1; dy <= 1; dy++) { if (dy == 0) continue; int yy = y + dy; if (yy < 0 || yy >= g.h) continue; for (int dx = -1; dx <= 1; dx++) { int xx = bx + dx; if (xx >= 0 && xx < g.w) g.cells[xx + yy * g.w] = te::Cell(); } }
        emit(ItemFx::BombBlast, i, bx, y); se(0x89); se(0x8A);
    }
    static void dropRow(te::Game& g, int row) { for (int y = row; y > 0; y--) for (int x = 0; x < g.w; x++) g.cells[x + y * g.w] = g.cells[x + (y - 1) * g.w]; for (int x = 0; x < g.w; x++) g.cells[x] = te::Cell(); }
    void kickLine(int i) {  // Patear línea: borra la fila inferior no vacía del propio campo (sin puntero: §4.4) [inferido: propio]
        te::Game& g = G(i); int row = -1; for (int y = g.h - 1; y >= 0 && row < 0; y--) for (int x = 0; x < g.w; x++) if (g.cells[x + y * g.w].mino) { row = y; break; }
        if (row < 0) return; int marks = 0; for (int x = 0; x < g.w; x++) if (g.cells[x + row * g.w].mino && (g.cells[x + row * g.w].flags & CELL_ITEM)) marks++;
        dropRow(g, row); freePiece(g); emit(ItemFx::Kick, i, row - g.h / 2); for (int k = 0; k < marks; k++) grant(i);
    }
    void shiftField(int i, int dir) {  // Cambio: desplaza toda la matriz una columna (con vuelta: no se pierden bloques) [inferido]
        te::Game& g = G(i); for (int y = 0; y < g.h; y++) { te::Cell* r = &g.cells[y * g.w]; if (dir > 0) std::rotate(r, r + g.w - 1, r + g.w); else std::rotate(r, r + 1, r + g.w); }
        freePiece(g); S[i].tShift = 0; emit(ItemFx::Shift, i, dir);
    }

    // ---------- entrada y avance ----------
    // use: botón de objeto (flanco); mash: cualquier botón de acción (flanco; para agitar el humo); left/right: estado de ←/→ (Cambio elige lado).
    void input(int i, bool useBtn, bool mash, bool left, bool right) {
        if (!active || i < 0 || i >= n() || !alive(i)) return; St& s = S[i];
        if (s.smoke > 0 && !cpu[i]) { if (mash || useBtn) wipeStep(i, 3.0f); return; }  // humo: lo primero es agitar
        if (s.tShift > 0 && (left || right)) { shiftField(i, left ? -1 : 1); return; }
        if (useBtn) use(i);
    }
    void wipeStep(int i, float pts) {  // +3 puntos por pulsación a la nube actual; > 4 la retira (SE 0x60)
        St& s = S[i]; for (int k = 0; k < 7; k++) if (s.cloud[k]) { s.cloudPts[k] += pts; if (s.cloudPts[k] > 4.0f) { s.cloud[k] = false; s.smoke--; emit(ItemFx::SmokeWipe, i, k); se(0x60); } return; }
    }
    void update(float dts) {
        if (!active) return;
        for (int i = 0; i < n() && i < MAXP; i++) {
            St& s = S[i]; te::Game& g = G(i); ensureWrap(i);
            if (!alive(i)) { if (!s.dead) { clearEffects(i); s.dead = true; } continue; }
            s.dead = false; auto tick = [&](float& t) { if (t > 0) { t -= dts; if (t < 0) t = 0; } };
            tick(s.tHide); tick(s.tFreeze); tick(s.tLock); tick(s.tCascade);
            bool wasCf = s.tConfuse > 0; tick(s.tConfuse); (void)wasCf;
            tick(s.tAccel); if (s.tAccel > 0) { g.fallOverride = 0.02f; s.accelOn = true; } else if (s.accelOn) { g.fallOverride = 0; s.accelOn = false; }
            if (s.tFix > 0) { tick(s.tFix); if (s.tFix <= 0 && s.bagSaved) { g.bag = s.bagBackup; s.bagSaved = false; s.fixType = 0; } }
            if (s.tShot > 0) { tick(s.tShot); s.shotCd -= dts; if (cpu[i] && s.shotCd <= 0) { fire(i); s.shotCd = 0.3f; } }
            if (s.tBomb > 0) { tick(s.tBomb); if (cpu[i]) placeBomb(i); }
            if (s.tShift > 0) { tick(s.tShift); if (s.tShift <= 0) shiftField(i, (rnd() & 1) ? 1 : -1); }  // sin lado elegido: al azar
            for (int k = 0; k < SLOTS; k++) if (s.slotT[k] > 0) s.slotT[k] -= dts;
            if (cpu[i]) {  // IA: nunca usa 4, 5, 9, 18; quita una nube cada ~0,5 s (§4.5)
                if (s.smoke > 0) { s.cpuWipe += dts; if (s.cpuWipe >= 0.5f) { s.cpuWipe = 0; wipeStep(i, 5.0f); } }
                else if (s.slot[0] && s.slotT[0] <= 0 && !s.busy() && s.tFreeze <= 0) { int id = s.slot[0]; if (id != IT_KICK && id != IT_CASCADE && id != IT_SHIFT && id != IT_BOMB) use(i); }
            }
            if (s.clAlive) updateClimber(i, dts);
        }
    }
    void updateClimber(int i, float dts) {  // Escalador de rival: recorre la superficie; si una pieza fijada lo aplasta, 5 líneas de basura al campo (FUN_800619fc)
        St& s = S[i]; te::Game& g = G(i); s.tClimb -= dts; if (s.tClimb <= 0) { s.clAlive = false; return; }
        if (g.cells[s.clX + s.clY * g.w].mino) { s.clAlive = false; crush(i); return; }
        s.clStep -= dts; if (s.clStep > 0) return; s.clStep = 0.4f;
        int nx = s.clX + s.clDir; if (nx < 0 || nx >= g.w) { s.clDir = -s.clDir; nx = s.clX + s.clDir; }
        int sy = surface(g, nx), cur = surface(g, s.clX);
        if (sy >= 0 && sy >= cur - 1) { s.clX = nx; s.clFace = s.clDir; s.clY = sy; } else { s.clDir = -s.clDir; s.clY = std::max(0, cur); }
        if (rnd() % 5 == 0) s.clDir = -s.clDir;
    }
    void crush(int i) {
        St& s = S[i]; emit(ItemFx::Crush, i, s.clX, s.clY); se(0x5B);
        if (onGarbage) onGarbage(i, 5, s.clFrom); else { int col = (int)(rnd() % G(i).w); for (int k = 0; k < 5; k++) { GarbageItem gi; gi.from = s.clFrom; gi.col = col; P[i]->pendingGarbage.push_back(gi); } }
        garbageSent[i] += 5;
    }
};

} // namespace game
