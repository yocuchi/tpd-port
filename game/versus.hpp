// Partida multijugador local (Tetris VS, modo 10, 2-4 jugadores): jugadores vivos/eliminados, equipos, basura/penalización,
// hándicap, fin de partida y HUD por jugador (notes_vs.md §1.5, §3, §5). Sin dependencias de entrada ni de SDL de ventana:
// lo usan main_sdl.cpp (jugadores reales y CPU) y tools/vs_snap.cpp (capturas).
#pragma once
#include "../gfx/game_view.hpp"
#include "../gfx/telop.hpp"
#include "vs_config.hpp"
#include "map_patterns.hpp"  // AJUSTES: "Mapa"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <functional>

namespace game {

// Un registro de la cola de basura pendiente = una línea (notes_vs §3.4: registro {emisor, columnaHueco, fila, variante}).
struct GarbageItem { int from = -1; int col = 0; int row = -1; int color = 2; };  // row: fila del motor del atacante (solo Resbaladizo), color: id de bloque (equipo del emisor)

// ¿cabe la pieza `t` con orientación `dir` con su marca 0 en (x, y)? (misma geometría que te::Game::collide; no mira la pieza activa)
inline bool pieceFits(const te::Game& g, const te::TetDef& t, int dir, int x, int y) {
    int ax = t.marks ? t.mx(dir, 0) : 0, ay = t.marks ? t.my(dir, 0) : 0;
    for (int j = 0; j < 7; j++) {
        if (!(t.rowmask[dir] & (1 << j))) continue;
        int cy = j + y - ay;
        for (int i = 0; i < 7; i++) {
            if (!t.cell(i, j, dir)) continue;
            int cx = i + x - ax;
            if (cx < 0 || cx >= g.w || cy >= g.h) return false;
            if (cy >= 0 && g.cells[cx + cy * g.w].mino) return false;
        }
    }
    return true;
}

// Datos de un jugador común a la partida real y a las capturas. main_sdl.cpp deriva de él (Player: + Stats, PadInput, CpuInput, mando).
struct PlayerBase {
    te::Game g;                               // motor de esta partida
    view::GameView V;                         // layout de marco/campo de este jugador (recursos pesados compartidos vía GameRes)
    std::vector<GarbageItem> pendingGarbage;  // basura pendiente de entrar en el campo (la barra Penalty muestra su longitud)
    int slot = 0;       // índice k en VersusState::P (k-ésimo jugador activo = VSnPlayer0k del layout)
    int cfgSlot = 0;    // J1..J4 (0..3) de la ventana de jugadores
    int team = 0;       // color de equipo 0..3
    int handicap = 0;   // 0..5
    bool cpu = false;   // jugador controlado por la IA
    bool alive = true; int wins = 0; int outOrder = 0;  // outOrder: 1 = primero en ser eliminado (auto-hándicap)
    // estado de las reglas de ataque/recepción
    int runCount = -1, runCol = 0;         // racha de columna de hueco del receptor (G+0x2c58/0x2c5c)
    int insertTimer = 0;                   // pasos hasta insertar la siguiente línea de basura
    int lastLowestFull = -1, lastFullRows = 0;  // al fijar la pieza: fila (motor 0..H-1) de la línea completa más baja y nº de líneas completas
    int spinKind = 0; bool b2bChain = false;    // 0 no, 1 giro completo, 2 mini (de la última pieza fijada); cadena back-to-back
    int pieceId = 0;                       // id de la última pieza fijada (7 = T)
    int sent = 0, received = 0, cancelled = 0;  // estadísticas
    virtual ~PlayerBase() {}
};

inline std::string vsFmtTime(float sec) { int ms = (int)(sec * 1000); char b[32]; snprintf(b, sizeof b, "%02d'%02d\"%02d", ms / 60000, (ms / 1000) % 60, (ms / 10) % 100); return b; }

struct VersusState {
    std::vector<PlayerBase*> P; VsConfig cfg;
    int winner = -1;       // índice k del ganador (sin equipos) o -1
    int winnerCfgSlot = -1;// J (0..3) del ganador (para multi_win_anim%02d)
    int winTeam = -1;      // color del equipo ganador (con equipos) o -1
    bool teamMode = false, over = false, draw = false;
    std::function<void(int)> onSe;  // efectos de sonido (0x6D basura, 0x41 caída)
    int outCounter = 0;
    int n() const { return (int)P.size(); }
    static constexpr int SE_BLOCK_UP = 0x6D, SE_PLAYER_OUT = 0x41;
    static int teamMino(int team) { static const int T[MAX_PLAYERS] = {2, 4, 3, 6, 7, 5, 8, 1}; return T[team >= 0 && team < MAX_PLAYERS ? team : team & 3]; }  // rojo, azul, verde, amarillo (blocks00: 2 rojo, 3 verde, 4 azul, 6 amarillo) + VS 6: 7 cian, 5 naranja

    uint32_t rs = 0x2545F491u;
    uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

    // players: los jugadores activos en orden J1..J4 (cfg.type != VT_OFF). Asigna slot/equipo/hándicap/CPU y aplica el hándicap inicial (§1.5).
    void start(const std::vector<PlayerBase*>& players, const VsConfig& c = VsConfig(), uint32_t seed = 1) {
        P = players; cfg = c; winner = winnerCfgSlot = winTeam = -1; over = draw = false; outCounter = 0; rs = seed * 2654435761u + 0x9E3779B9u; if (!rs) rs = 1;
        std::vector<int> act; for (int j = 0; j < MAX_PLAYERS; j++) if (cfg.type[j] != VT_OFF) act.push_back(j);
        teamMode = cfg.hasTeams();
        for (int i = 0; i < (int)P.size(); i++) {
            PlayerBase& p = *P[i]; p.slot = i; p.alive = true; p.outOrder = 0; p.pendingGarbage.clear();
            int j = i < (int)act.size() ? act[i] : i; p.cfgSlot = j; p.team = cfg.team[j] >= 0 && cfg.team[j] < MAX_PLAYERS ? cfg.team[j] : cfg.team[j] & 3; p.handicap = cfg.handicap[j]; p.cpu = cfg.type[j] == VT_CPU;
            p.runCount = -1; p.runCol = 0; p.insertTimer = 0; p.lastLowestFull = -1; p.lastFullRows = 0; p.spinKind = 0; p.b2bChain = false; p.pieceId = 0; p.sent = p.received = p.cancelled = 0;
            if (cfg.mapPattern >= 0) applyMap(p, cfg.mapPattern); else applyHandicap(p);  // AJUSTES: "Mapa" sustituye al hándicap
        }
    }
    int aliveCount() const { int c = 0; for (auto* p : P) if (p->alive) c++; return c; }

    // "Mapa" (FUN_801d4fc4): copia el patrón m (20 filas x 10 columnas) a las 20 filas visibles del campo (filas 20..39 del motor)
    void applyMap(PlayerBase& p, int m) {
        te::Game& g = p.g; m = ((m % 20) + 20) % 20;
        for (int y = 0; y < 20; y++) for (int x = 0; x < g.w && x < 10; x++) { int c = MAP_PATTERNS[m][y][x] - '0'; int gy = g.h - 20 + y; if (c > 0 && gy >= 0) { te::Cell cell; cell.mino = c; g.cells[x + gy * g.w] = cell; } }
    }
    // Hándicap (FUN_80062a5c): las 2·h filas inferiores se rellenan de bloques de color rand%6+2 y en cada una se vacían 3..6 celdas al azar.
    void applyHandicap(PlayerBase& p) {
        int h = std::max(0, std::min(5, p.handicap)); te::Game& g = p.g;
        for (int r = 0; r < 2 * h && r < g.h / 2; r++) {
            int y = g.h - 1 - r;
            for (int x = 0; x < g.w; x++) { te::Cell& c = g.cells[x + y * g.w]; if (!c.mino) { c = te::Cell(); c.mino = rnd() % 6 + 2; } }
            int holes = (int)(rnd() % 4) + 3; for (int k = 0; k < holes; k++) g.cells[(int)(rnd() % g.w) + y * g.w] = te::Cell();
        }
    }

    // ===== Hooks del motor de cada jugador (main_sdl.cpp / vs_snap los conectan: g.onLock, g.spawnGate, Client::clearEvent) =====

    // Al fijar una pieza (te::Game::onLock; las celdas ya están escritas, la pieza activa sigue disponible): anota qué giro ha sido (T-spin / mini / giro
    // inmóvil), qué líneas están completas (la más baja es la fila de ataque del modo Resbaladizo) y avisa de las celdas-objeto de esas líneas.
    void onLock(int k) {
        PlayerBase& p = *P[k]; te::Game& g = p.g; if (!g.hasPiece) return;
        const te::TetDef& t = *g.piece.tet; int dir = g.piece.dir, px = g.piece.x, py = g.piece.floorY(); p.pieceId = (int)t.src->id;
        p.lastLowestFull = -1; p.lastFullRows = 0;
        for (int y = 0; y < g.h; y++) { bool full = true; for (int x = 0; x < g.w && full; x++) if (!g.cells[x + y * g.w].mino) full = false; if (full) { p.lastFullRows++; p.lastLowestFull = y; } }
        p.spinKind = 0;
        if (g.lockRotated) {
            // las celdas de la propia pieza se quitan un momento para probar movimientos / esquinas sobre el campo sin ella
            int ax = t.marks ? t.mx(dir, 0) : 0, ay = t.marks ? t.my(dir, 0) : 0; std::vector<std::pair<int, te::Cell>> saved;
            for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) if (t.cell(i, j, dir)) { int cx = i + px - ax, cy = j + py - ay; if (cx >= 0 && cx < g.w && cy >= 0 && cy < g.h) { saved.push_back({cx + cy * g.w, g.cells[cx + cy * g.w]}); g.cells[cx + cy * g.w] = te::Cell(); } }
            if (t.src->id == 7) {  // T: regla de las 3 esquinas alrededor del centro (marca 0) [inferido: el original decide giro/mini en el motor, param_4/param_5 de FUN_80067554]
                auto occ = [&](int x, int y) { return x < 0 || x >= g.w || y >= g.h || (y >= 0 && g.cells[x + y * g.w].mino); };
                int dx[4] = {1, -1, 1, -1}, dy[4] = {1, 1, -1, -1}, filled = 0; bool c[4]; for (int i = 0; i < 4; i++) { c[i] = occ(px + dx[i], py + dy[i]); filled += c[i]; }
                if (filled >= 3) {
                    // brazo que falta = lado plano; las esquinas del lado opuesto son las "delanteras"
                    int arm[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}, mx = 0, my = 0;
                    for (auto& a : arm) { int cx = px + a[0], cy = py + a[1]; bool own = false; for (auto& s : saved) if (s.first == cx + cy * g.w) own = true; if (!own) { mx = a[0]; my = a[1]; } }
                    int front = 0; for (int i = 0; i < 4; i++) if (c[i] && (dx[i] * -mx + dy[i] * -my) > 0) front++;
                    p.spinKind = front >= 2 ? 1 : 2;
                }
            } else {  // otras piezas: giro si no puede moverse a izquierda, derecha ni arriba
                if (!pieceFits(g, t, dir, px - 1, py) && !pieceFits(g, t, dir, px + 1, py) && !pieceFits(g, t, dir, px, py - 1)) p.spinKind = 1;
            }
            for (auto& s : saved) g.cells[s.first] = s.second;
        }
        // ---- GANCHO DE OBJETOS (otro agente: game/items.hpp) ----
        // Cada 11ª pieza lleva una celda marcada (flag de celda-objeto). Al despejar una línea completa que la contenga, por cada celda marcada hay que
        // generar un objeto (notes_vs §4.2). Aquí se conoce ya qué filas están completas (arriba); el que implemente los objetos recorre esas filas y
        // llama a onItemCell(k, fila, x) o lo hace directamente en este punto. `itemCellMask` = bit de Cell::flags que identifica la celda-objeto (0 = ninguno).
        if (onItemCell && itemCellMask) for (int y = 0; y < g.h; y++) { bool full = true; for (int x = 0; x < g.w && full; x++) if (!g.cells[x + y * g.w].mino) full = false; if (!full) continue; for (int x = 0; x < g.w; x++) if (g.cells[x + y * g.w].flags & itemCellMask) onItemCell(k, y, x); }
    }
    std::function<void(int player, int row, int x)> onItemCell;  // gancho de objetos: celda-objeto en una línea que se va a borrar
    uint8_t itemCellMask = 0;

    // Cada despeje (Client::clearEvent del jugador k): calcula la basura (§3.1), cancela la pendiente propia (§3.2) y envía el resto a un objetivo
    // aleatorio vivo de otro equipo (§3.3). `spin`/`b2b` del motor se ignoran: se usan spinKind/b2bChain de onLock (T-spin/mini/giro inmóvil).
    void onLinesCleared(int k, int lines, int spin, bool b2b, bool allClear, int combo) {
        (void)spin; (void)b2b; if (k < 0 || k >= n()) return;
        PlayerBase& p = *P[k]; p.g.levelUpPending = false;  // nivel fijo 1 en VS (§2.3)
        if (lines <= 0 || over) return;
        int g = garbageFor(p, lines, allClear, combo);
        // GANCHO DE OBJETOS: aquí (tras el cálculo de basura) iría la entrega de los objetos ganados en esta línea (ver onLock / onItemCell).
        if (g <= 0) return;
        bool canceled = false;
        while (g > 0 && !p.pendingGarbage.empty()) { p.pendingGarbage.erase(p.pendingGarbage.begin()); g--; p.cancelled++; canceled = true; }  // evento 0x2E
        if (canceled && onSe) onSe(SE_BLOCK_UP);
        if (g <= 0) return;
        int t = pickTarget(k); if (t < 0) return;
        for (int i = 0; i < g; i++) addGarbage(*P[t], p);
        p.sent += g;
    }
    // g = bonusCombo + bonusAllClear + base (+1 si back-to-back) (FUN_80060dfc)
    int garbageFor(PlayerBase& p, int lines, bool allClear, int combo) {
        int kind = p.spinKind, base;
        if (kind == 0) { static const int B[5] = {0, 0, 1, 2, 4}; base = B[std::min(lines, 4)]; }
        else if (kind == 2) { static const int B[5] = {0, 0, 1, 1, 1}; base = B[std::min(lines, 4)]; }
        else if (p.pieceId == 7) { static const int B[5] = {0, 2, 4, 6, 6}; base = B[std::min(lines, 4)]; }
        else { static const int B[5] = {0, 1, 2, 3, 3}; base = B[std::min(lines, 4)]; }
        bool difficult = kind != 0 || lines >= 4, chain = difficult && p.b2bChain; p.b2bChain = difficult;
        int kk = std::max(0, combo), cb = kk <= 1 ? 0 : kk <= 3 ? 1 : kk <= 5 ? 2 : kk <= 7 ? 3 : kk <= 10 ? 4 : 5;
        int g = cb + (allClear ? 6 : 0) + base; if (chain && g != 0) g++;
        return g;
    }
    // FUN_800a1630: un objetivo al azar entre los vivos de otro equipo (una vez por golpe)
    int pickTarget(int k) {
        std::vector<int> c; for (int j = 0; j < n(); j++) if (j != k && P[j]->alive && P[j]->team != P[k]->team) c.push_back(j);
        return c.empty() ? -1 : c[rnd() % c.size()];
    }
    // FUN_800615ac: añade una línea a la cola pendiente del receptor; columna del hueco con racha compartida por 2..9 líneas, columnas 0 y 9 con prob. doble
    void addGarbage(PlayerBase& r, PlayerBase& from) {
        int W = r.g.w;
        if (r.runCount < 0) { r.runCount = (int)(rnd() % 8) + 1; int c = (int)(rnd() % (uint32_t)(W + 2)); if (c == W) c = 0; else if (c == W + 1) c = W - 1; r.runCol = c; }
        GarbageItem it; it.from = from.slot; it.col = r.runCol; r.runCount--;
        it.row = cfg.slippery ? from.lastLowestFull : -1; it.color = teamMino(from.team);
        r.pendingGarbage.push_back(it); r.received++;
    }
    // Puerta de aparición del jugador k (te::Game::spawnGate): sin pieza activa, tras fijar y borrar, la basura entra UNA línea por paso hasta vaciar la cola
    // y solo entonces aparece la siguiente pieza (FUN_80061de4). Devuelve true cuando se puede generar la pieza.
    bool spawnGate(int k) {
        PlayerBase& p = *P[k]; if (!p.alive || p.g.state) return true;
        if (p.pendingGarbage.empty()) { p.insertTimer = 0; return true; }
        if (++p.insertTimer < 4) return false;
        p.insertTimer = 0; insertOne(p); return false;
    }
    // Inserta la cabeza de la cola: las filas por encima de la de inserción suben 1 y la nueva se rellena salvo el hueco.
    // Normal: fila H-1 (suelo). Resbaladizo: la fila del atacante, recortada al interior de la pila del receptor [inferido, ver notes §3.5].
    void insertOne(PlayerBase& r) {
        te::Game& g = r.g; GarbageItem it = r.pendingGarbage.front(); r.pendingGarbage.erase(r.pendingGarbage.begin());
        int top = g.h; for (int y = 0; y < g.h && top == g.h; y++) for (int x = 0; x < g.w; x++) if (g.cells[x + y * g.w].mino) { top = y; break; }
        int row = g.h - 1; if (it.row >= 0) { row = std::min(it.row, g.h - 1); if (top < g.h) row = std::max(row, top); }
        for (int x = 0; x < g.w; x++) if (g.cells[x].mino) { g.lose(); break; }  // la pila rebasa el techo del campo
        for (int y = 0; y < row; y++) for (int x = 0; x < g.w; x++) g.cells[x + y * g.w] = g.cells[x + (y + 1) * g.w];
        for (int x = 0; x < g.w; x++) { te::Cell c; if (x != it.col) c.mino = it.color < 1 ? 1 : it.color; g.cells[x + row * g.w] = c; }
        if (onSe) onSe(SE_BLOCK_UP);
    }

    // ===== Fin de partida (§5.1) =====
    // Tras actualizar los motores: elimina al que haya perdido (g.state == 2: campo congelado) y decide el ganador: último vivo, o último equipo con vivos
    // (con equipos). Sin vivos (caída simultánea) = empate. Devuelve true cuando la partida ha terminado.
    bool update() {
        if (over) return true;
        for (auto* p : P) if (p->alive && p->g.state == 2) { p->alive = false; p->outOrder = ++outCounter; if (onSe) onSe(SE_PLAYER_OUT); }
        int c = aliveCount(); bool teamsLeft[MAX_PLAYERS] = {}; int nt = 0;
        for (auto* p : P) if (p->alive && !teamsLeft[p->team % MAX_PLAYERS]) { teamsLeft[p->team % MAX_PLAYERS] = true; nt++; }
        if (nt <= 1) {
            over = true; draw = c == 0;
            for (auto* p : P) if (p->alive) { if (teamMode) winTeam = p->team; else { winner = p->slot; winnerCfgSlot = p->cfgSlot; } p->wins++; }
            if (teamMode && winTeam >= 0) { for (auto* p : P) if (p->alive) { winner = p->slot; winnerCfgSlot = p->cfgSlot; break; } }
        }
        return over;
    }

    // Telop de fin (game/telop.arc + telop_lang.arc), uno solo en el centro de la pantalla: draw_anim01 (empate), multi_win_anim%02d (jugador+1) o
    // team_win_anim%02d (equipo+1). TL: un Telop (comparte los arcs de `base`).
    void playEndTelops(std::vector<view::Telop>& TL, const view::Telop& base) const {
        TL.resize(1); view::Telop& t = TL[0]; t.stop(); t.share(base); char an[48];
        if (draw || (winner < 0 && winTeam < 0)) { t.play("draw.brlyt", "draw_anim01.brlan"); return; }
        if ((teamMode && winTeam >= 4) || (!(teamMode && winTeam >= 0) && winnerCfgSlot >= 4)) { t.play("win.brlyt", "win_anim01.brlan"); return; }  // VS 6/8: no hay telop del 5.º..8.º jugador/equipo: "WIN!" genérico
        if (teamMode && winTeam >= 0) { snprintf(an, sizeof an, "team_win_anim%02d.brlan", winTeam + 1); t.play("team_win.brlyt", an); }
        else { snprintf(an, sizeof an, "multi_win_anim%02d.brlan", std::max(0, winnerCfgSlot) + 1); t.play("multi_win.brlyt", an); }
    }

    // Textos del marco VS de un jugador (el marco tiene sus propios TextBox: TextBox_time, TextBox_lines…; los que la anim deja ocultos no se dibujan).
    static void hud(PlayerBase& p) {
        p.V.vs6Wins = p.wins; char b[32]; snprintf(b, sizeof b, "%d", std::min(9999, p.g.lines));
        std::string t = vsFmtTime(p.g.gameTime);
        for (const char* n : {"TextBox_lines", "TextBox_lines_01"}) p.V.setTextS(n, b);
        for (const char* n : {"TextBox_time", "TextBox_time_00", "TextBox_time_01"}) p.V.setTextS(n, t);
    }
    // Barra Penalty (panes `VSnPk_Penalty00..19`, PenaltyNum celdas de abajo a arriba): una celda por línea pendiente, del color del emisor.
    // Se dibuja con V.drawTop (sobre el marco, que trae la franja negra `left_00`).
    static void drawPenalty(PlayerBase& p, view::GameView& V) {
        int nn = std::min((int)p.pendingGarbage.size(), std::min(V.pc.penaltyNum, (int)V.pc.penaltyPos.size())); if (nn <= 0 || !V.atlas) return;
        float bs = V.pc.penaltySize > 0 ? V.pc.penaltySize : V.pc.blockSize * V.pc.blockScale;
        for (int i = 0; i < nn; i++) { SDL_Rect s; V.minoRect(p.pendingGarbage[i].color, s); V.blit(V.atlas, s, V.pc.penaltyPos[i].first - bs / 2, V.pc.penaltyPos[i].second - bs / 2, bs, bs); }
    }
};

} // namespace game
