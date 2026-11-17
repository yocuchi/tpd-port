// Tabla de modos de un jugador (notes_modes_basic.md §1, §3, §5): reglas TPF2, HUD, fondo, música, metas.
#pragma once
#include <string>

namespace game {

enum class Kind { Marathon, Master, Sprint, Beginner, AllClear, Shadow, Drop, Climber, Bombliss, Versus, Other };

struct ModeSpec {
    Kind kind; const char* rules; const char* cfgLayout; const char* fieldGroup; const char* hudAnim; int bg; int bgm;
    int lineGoal; bool fixedLevel; bool ghost; int allClearGoal;
};

inline ModeSpec modeSpec(int mode) {
    switch (mode) {
    case 0:  return {Kind::Marathon, "8044aa58.tpf2", "config_00.brlyt",   "Single", "frame00a_00.brlan",        15, 46, 150, false, true, 0};   // Maratón: BGM_GAME_C
    case 8:  return {Kind::Master,   "8045f538.tpf2", "config_00.brlyt",   "Single", "frame00a_00.brlan",        11, 58, 150, false, false, 0};  // Maestro: 20G, sin fantasma, BGM_GAME_N
    case 7:  return {Kind::Sprint,   "8044aa58.tpf2", "config_sprint.brlyt","Single", "frame00a_24.brlan",       10, 48, 40, true, true, 0};     // Sprint: 40 líneas, BGM_GAME_E
    case 5:  return {Kind::Beginner, "8044cb78.tpf2", "config_mini.brlyt", "Single", "frame00a_18.brlan",         6, 47, 150, false, true, 0};   // Principiantes: campo 5x10, BGM_GAME_D
    case 6:  return {Kind::AllClear, "8044cb78.tpf2", "config_mini.brlyt", "Single", "frame00a_all_clear.brlan",  6, 45, 0, true, true, 20};     // All Clear Sprint: 20 despejes, BGM_GAME_B
    case 2:  return {Kind::Climber,  "8044aa58.tpf2", "config_climb.brlyt", "Single", "frame00a_01.brlan",        14, 56, 0, true, true, 0};     // Escalador: bg_14, BGM 0x38 (FUN_800801d4/8008008c)
    case 4:  return {Kind::Drop,     "80466f08.tpf2", "config_drop.brlyt", "Single", "frame00a_02.brlan",        6, 46, 0, true, true, 0};      // Corredor de etapas: bg/BGM [inferido]
    case 9:  return {Kind::Bombliss, "8044aa58.tpf2", "config_00.brlyt",   "Single", "frame00a_23.brlan",        15, 46, 0, true, true, 0};      // Bombliss: sin líneas ni hold
    case 1:  return {Kind::Versus,   "8044aa58.tpf2", "config_00.brlyt",   "VS2",    "frame00a_2_1P_03.brlan",    15, 46, 0, false, true, 0};     // Tetris vs. IA (VSIA): 2 campos (J1 + IA), bg_15, BGM_GAME_C; marcos frame00a_2_1P_03 / frame00a_3_2P_17
    case 10: return {Kind::Versus,   "8044aa58.tpf2", "config_00.brlyt",   "VS2",    "frame00a_2_1P_07.brlan",    15, 46, 0, false, true, 0};     // Tetris VS (2-4 jugadores locales): marcos VS por nº de jugadores (versus.hpp), bg_15, BGM_GAME_C
    case 27: return {Kind::Versus,   "8044aa58.tpf2", "config_00.brlyt",   "VS2",    "frame00a_2_1P_07.brlan",    15, 46, 0, false, true, 0};     // Tetris VS 6 (6 jugadores locales en fila, marco por código): mismas reglas/bg/bgm que el 10
    case 28: return {Kind::Versus,   "8044aa58.tpf2", "config_00.brlyt",   "VS2",    "frame00a_2_1P_07.brlan",    15, 46, 0, false, true, 0};     // Tetris VS 8 (8 jugadores locales en dos filas, marco por código): mismas reglas/bg/bgm que el 10
    case 3:  return {Kind::Shadow,   "80466f08.tpf2", "config_00.brlyt",   "Single", "frame00a_12.brlan",        15, 46, 0, true, true, 0};
    }
    return {Kind::Other, "80466f08.tpf2", "config_00.brlyt", "Single", "frame00a_00.brlan", 15, 46, 0, false, true, 0};
}

} // namespace game
