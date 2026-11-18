// Configuración de una partida de Tetris VS (modo 10), la que edita la ventana de jugadores del menú (menu2_win, notes_vs.md §1).
// Sin dependencias: la usan gfx/menu.hpp (ventana), game/versus.hpp (reglas) y main_sdl.cpp / tools/vs_snap.cpp.
#pragma once

namespace game {

constexpr int MAX_PLAYERS = 8;  // Tetris VS AUTO 5-8 (modo 27): 5..8 jugadores locales; los modos 10/25 siguen usando 2-4

enum VsType { VT_OFF = 0, VT_HUMAN = 1 /* PER */, VT_GUEST = 2 /* INVITADO */, VT_CPU = 3 /* IA */ };

struct VsConfig {
    int type[MAX_PLAYERS] = {VT_HUMAN, VT_CPU, VT_OFF, VT_OFF, VT_OFF, VT_OFF, VT_OFF, VT_OFF};  // J1..J4 (J1 siempre humano)
    int team[MAX_PLAYERS] = {0, 1, 2, 3, 4, 5, 6, 7};                      // 0 rojo, 1 azul, 2 verde, 3 amarillo, 4 cian, 5 naranja (todos distintos = todos contra todos)
    int handicap[MAX_PLAYERS] = {0, 0, 0, 0, 0, 0, 0, 0};                  // 0..5: 2·h filas de basura inicial con 3..6 huecos (§1.5)
    int aiLevel = 0;                                    // "Nivel IA" 1..15 → 0..14 (cfg+0xb4), global para todas las IA
    bool slippery = false;                              // Penalización: Normal (false) / Resbaladizo (true) (§3.5)
    int mapPattern = -1;                                // "Mapa" activado: patrón 0..19 de game/map_patterns.hpp para todos los jugadores (sustituye al hándicap); -1 = sin mapa
    bool autoHandicap = false;                          // modo 0x19 (rating ±2 por puesto); no implementado, se guarda por si se añade
    int activeCount() const { int n = 0; for (int i = 0; i < MAX_PLAYERS; i++) if (type[i] != VT_OFF) n++; return n; }
    // Hay equipos si dos o más jugadores activos comparten color (§1.4)
    bool hasTeams() const { for (int i = 0; i < MAX_PLAYERS; i++) for (int j = i + 1; j < MAX_PLAYERS; j++) if (type[i] != VT_OFF && type[j] != VT_OFF && team[i] == team[j]) return true; return false; }
    // Validación (FUN_801d49f4): >=2 jugadores y algún activo de otro equipo que J1
    bool valid() const {
        if (activeCount() < 2) return false;
        for (int i = 1; i < MAX_PLAYERS; i++) if (type[i] != VT_OFF && team[i] != team[0]) return true;
        return false;
    }
};

} // namespace game
