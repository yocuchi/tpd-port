// Frontal SDL2 con los recursos originales. Compila en Mac y en Switch (devkitPro/libnx).
#include "gfx/game_view.hpp"
#include "gfx/title_view.hpp"
#include "gfx/msg.hpp"
#include "gfx/menu.hpp"
#include "gfx/telop.hpp"
#include "gfx/dedication.hpp"
#include "snd/audio.hpp"
#include "game/shadow.hpp"
#include "game/bombliss.hpp"
#include "game/drop.hpp"
#include "game/modes.hpp"
#include "game/climber.hpp"
#include "gfx/climber_view.hpp"
#include "game/versus.hpp"
#include "gfx/item_view.hpp"  // ITEMS: objetos de Tetris VS (game/items.hpp + vista)
#include "game/vs_ai.hpp"
#include "game/settings.hpp"  // AJUSTES
#include "game/vs_ia.hpp"    // VSIA
#include "game/records.hpp"       // RECORDS: top-5 de puntuación/tiempo persistentes
#include "gfx/result_window.hpp"  // RECORDS: NEW RECORD, ventana de resultados, menú CONTINUE/RETRY/QUIT
#include "gfx/pause.hpp"          // PAUSA: menú de pausa
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <ctime>
#include <string>

#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>
#ifdef NSP_BUILD
#define ASSET_DIR "romfs:/assets/"
#define RULES_DIR "romfs:/data/"
#else
#define ASSET_DIR "sdmc:/switch/tetris-port/assets/"
#define RULES_DIR "sdmc:/switch/tetris-port/data/"
#endif
#else
#define ASSET_DIR "assets_src/"
#define RULES_DIR "data/"
#endif

#define SCREEN_W 1280
#define SCREEN_H 720

enum { JB_A = 0, JB_B = 1, JB_X = 2, JB_Y = 3, JB_L = 6, JB_R = 7, JB_ZL = 8, JB_ZR = 9, JB_PLUS = 10, JB_LEFT = 12, JB_UP = 13, JB_RIGHT = 14, JB_DOWN = 15 };

// Botones lógicos (mando o teclado)
enum Btn { B_LEFT, B_RIGHT, B_UP, B_DOWN, B_A, B_B, B_X, B_Y, B_L, B_R, B_PLUS, B_COUNT };
struct Pad {
    bool down[B_COUNT] = {}, pressed[B_COUNT] = {};
    void set(int b, bool d) { if (b < 0) return; if (d && !down[b]) pressed[b] = true; down[b] = d; }
    void endFrame() { for (bool& p : pressed) p = false; }
    static int fromJoy(int b) { switch (b) { case JB_LEFT: return B_LEFT; case JB_RIGHT: return B_RIGHT; case JB_UP: return B_UP; case JB_DOWN: return B_DOWN; case JB_A: return B_A; case JB_B: return B_B; case JB_X: return B_X; case JB_Y: return B_Y; case JB_L: case JB_ZL: return B_L; case JB_R: case JB_ZR: return B_R; case JB_PLUS: return B_PLUS; } return -1; }
    static int fromKey(SDL_Keycode k) { switch (k) { case SDLK_LEFT: return B_LEFT; case SDLK_RIGHT: return B_RIGHT; case SDLK_UP: return B_UP; case SDLK_DOWN: return B_DOWN; case SDLK_RETURN: case SDLK_x: return B_A; case SDLK_z: case SDLK_BACKSPACE: return B_B; case SDLK_SPACE: return B_X; case SDLK_a: return B_Y; case SDLK_c: case SDLK_LSHIFT: return B_L; case SDLK_v: return B_R; case SDLK_ESCAPE: return B_PLUS; } return -1; }
};
struct PadInput : te::Input {  // mapeo de partida: ←→ mover, A rotar, B rotar inverso, ↓ suave, X caída dura, L/R hold
    Pad* p; bool enabled = true; bool drop = false;  // drop: Corredor de etapas (arriba = subir pieza/frenar, abajo = acelerar; sin caída fuerte)
    bool left() override { return enabled && p->down[B_LEFT]; } bool right() override { return enabled && p->down[B_RIGHT]; } bool rotA() override { return enabled && p->down[B_A]; } bool rotB() override { return enabled && p->down[B_B]; }
    bool soft() override { return enabled && p->down[B_DOWN]; } bool hard() override { return enabled && !drop && p->down[B_X]; } bool hold() override { return enabled && (p->down[B_L] || p->down[B_R]); }
};
// Cliente del motor: estadísticas de los scripts y efectos de sonido por evento (FUN_80096f5c)
struct Stats : te::Client {
    float st[10] = {}; const te::Game* g = nullptr; int state = 0; snd::SoundSystem* snd = nullptr;
    game::VersusState* vs = nullptr; int slot = 0;  // partida VS: los despejes se reenvían al punto de enganche de ataque (VersusState::onLinesCleared)
    void event(int code) override {
        if (!snd) return;
        switch (code) { case 3: snd->playSe(snd::SE_HOLD); break; case 0xc: snd->playSe(snd::SE_SPIN_FAILURE); break; case 0xd: snd->playSe(snd::SE_HARDDROP); break; case 0x12: snd->playSe(snd::SE_LEVELUP); break; }
    }
    void clearEvent(int lines, int spin, bool b2b, bool allClear, int combo) override {
        if (vs) vs->onLinesCleared(slot, lines, spin, b2b, allClear, combo);
        if (!snd) return;
        if (lines > 0) { if (spin) snd->playSe(snd::SE_TSPIN_EZ + lines); else snd->playSe(snd::SE_LINE_SINGLE + (lines > 4 ? 3 : lines - 1)); }
        else if (spin) snd->playSe(snd::SE_TSPIN_EZ);
        if (b2b) snd->playSe(snd::SE_B2B);
        if (allClear) snd->playSe(snd::SE_ALLCLEAR);
        if (combo >= 1) snd->playSe(129 + (combo > 4 ? 3 : combo - 1));  // SE_70..73_EXCITE_COMBO
    }
    void clientLine(const std::string& raw) override {
        size_t p = raw.find("st0.d"); if (p == std::string::npos) return; int n = raw[p + 5] - '0'; if (n < 0 || n > 9) return;
        size_t t = raw.find(" to "); if (t == std::string::npos) return; std::string e = raw.substr(t + 4);
        if (e == "0") st[n] = 0; else if (e.find("+ 1") != std::string::npos) st[n] += 1; else if (e == "score" && g) st[n] = (float)g->visScore; else if (e == "game time" && g) st[n] = g->gameTime;
    }
    void win() override { state = 1; } void lose() override { state = 2; }
};

// Un jugador de la partida: motor + estadísticas/sonido + entrada + vista (layout propio; recursos pesados compartidos en view::GameRes).
// PlayerBase (versus.hpp) aporta g, V, pendingGarbage, slot, alive, wins.
struct Player : game::PlayerBase {
    Stats st; PadInput in; game::CpuInput cpu; Pad* pad = nullptr;  // in: mando (humano) / cpu: IA (jugador CPU de Tetris VS)
    bool practice = false; float outTimer = 0;  // FANTASMA: eliminado en VS que sigue practicando en su campo (sin atacar ni recibir)
    Player() { g.in = &in; g.cl = &st; st.g = &g; cpu.g = &g; }
};

static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
static std::string fmtTime(float sec) { int ms = (int)(sec * 1000); char b[32]; snprintf(b, sizeof b, "%02d'%02d\"%02d", ms / 60000, (ms / 1000) % 60, (ms / 10) % 100); return b; }

enum class Scene { Dedication, Title, Menu, Game };
enum class GamePhase { Countdown, Play, Ending, Result, Summary /* RECORDS: ventana de resultados */ };

#ifdef __SWITCH__
#include <exception>
static void dbg(const char* m) { printf("[tetris] %s\n", m); fflush(stdout); fprintf(stderr, "[tetris] %s\n", m); fflush(stderr); }
#else
static void dbg(const char*) {}
#endif
int realMain(int argc, char** argv);
int main(int argc, char** argv) {
#ifdef __SWITCH__
    socketInitializeDefault(); nxlinkStdio();  // salida por nxlink -s para depurar en la consola
    mkdir("sdmc:/switch", 0777); mkdir("sdmc:/switch/tetris-port", 0777); freopen("sdmc:/switch/tetris-port/log.txt", "w", stderr);  // registro en la SD
    dbg("arranque"); Result rc = romfsInit(); if (R_FAILED(rc)) { printf("[tetris] romfsInit fallo %08x\n", rc); }
    int r = 1;
    try { r = realMain(argc, argv); } catch (const std::exception& e) { printf("[tetris] EXCEPCION: %s\n", e.what()); fflush(stdout); } catch (...) { printf("[tetris] EXCEPCION desconocida\n"); fflush(stdout); }
    romfsExit(); socketExit(); return r;
#else
    return realMain(argc, argv);
#endif
}
static bool bigVs(int m) { return m == 27 || m == 28; }  // Tetris VS 6 / VS 8: pantalla completa 16:9, sin objetos ni hándicap
static int bigVsN(int m) { return m == 28 ? 8 : 6; }
int realMain(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) < 0) { dbg("SDL_Init fallo"); return 1; } dbg("SDL ok");
    snd::SoundSystem SND; if (!SND.load(std::string(ASSET_DIR) + "sound.brsar")) fprintf(stderr, "sin sonido\n"); dbg("sonido cargado");
#ifdef __SWITCH__
    SDL_DisplayMode dm; int winW = SCREEN_W, winH = SCREEN_H; if (SDL_GetCurrentDisplayMode(0, &dm) == 0 && dm.w > 0) { winW = dm.w; winH = dm.h; }
    SDL_Window* win = SDL_CreateWindow("Tetris Party Deluxe (port)", 0, 0, winW, winH, SDL_WINDOW_FULLSCREEN);
#else
    SDL_Window* win = SDL_CreateWindow("Tetris Party Deluxe (port)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
#endif
    SDL_Renderer* rn = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); if (!win || !rn) { printf("[tetris] ventana/renderer fallo: %s\n", SDL_GetError()); return 1; } dbg("renderer ok");
    // Mandos por orden de conexión: jugador 1 = teclado + primer mando, jugadores 2-4 = mandos siguientes (en Switch cada Joy-Con/Pro Controller es un joystick).
    std::vector<SDL_JoystickID> joyIds;
    auto addJoy = [&](int devIndex) { SDL_Joystick* j = SDL_JoystickOpen(devIndex); if (!j) return; SDL_JoystickID id = SDL_JoystickInstanceID(j); if (std::find(joyIds.begin(), joyIds.end(), id) == joyIds.end()) joyIds.push_back(id); };
    for (int i = 0; i < SDL_NumJoysticks(); i++) addJoy(i);
    // 4:3 centrado en 16:9
    int outW = SCREEN_W, outH = SCREEN_H; SDL_GetRendererOutputSize(rn, &outW, &outH);
    int vh = outH, vw = vh * 4 / 3; SDL_Rect vp = {(outW - vw) / 2, 0, vw, vh};
    lyt::Renderer LR(rn); LR.screenW = (float)vw; LR.screenH = (float)vh;
    bool wideNow = false;  // VS 6: toda la pantalla (16:9) en vez del recuadro 4:3 centrado
    auto refreshViewport = [&](bool wide) { int w = outW, h = outH; SDL_GetRendererOutputSize(rn, &w, &h); if (w != outW || h != outH || wide != wideNow) { outW = w; outH = h; wideNow = wide; vh = outH; vw = wide ? outW : vh * 4 / 3; vp = {(outW - vw) / 2, 0, vw, vh}; LR.screenW = (float)vw; LR.screenH = (float)vh; LR.baseW = wide ? 480.0f * vw / vh : 640.0f; } };
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string(ASSET_DIR) + f); if (!d.empty()) LR.loadFont(f, d.data(), d.size()); }
    msg::Catalog M; M.dir = std::string(ASSET_DIR) + "mess/";
    dbg("fuentes/textos"); view::TitleView T; bool title = T.load(&LR, ASSET_DIR, M.get(1, 1)); dbg("titulo cargado");
    view::Dedication DED; bool dedication = DED.load(&LR, std::string(ASSET_DIR) + "dedicatoria.txt");
    view::GameRes RES; bool assets = RES.load(&LR, ASSET_DIR);  // arcs, atlas y texturas: una sola vez, compartidos por todas las vistas de jugador
    dbg("vista de juego cargada"); view::Telop TL; TL.load(&LR, ASSET_DIR); dbg("telops cargados");
    menu::MenuSystem MN; bool hasMenu = MN.load(&LR, &M, ASSET_DIR); dbg("menu cargado"); { char b[96]; snprintf(b, sizeof b, "texturas: %zu cargadas, %d fallos; salida %dx%d", LR.textures.size(), LR.texFailures, outW, outH); dbg(b); } MN.onSe = [&](int id) { SND.playSe(id); };
    game::Settings AJ; AJ.load(); MN.cfg = &AJ; MN.onSettingsLeave = [&]() { AJ.save(); };  // AJUSTES: ajustes por modo (menú Ajustes = botón X en la selección de modo), persistidos en settings.bin
    if (!assets) fprintf(stderr, "sin recursos originales en %s\n", ASSET_DIR);

    Pad pads[game::MAX_PLAYERS]; std::vector<std::unique_ptr<Player>> PL;
    bool kb[B_COUNT] = {};  // teclado (se combina con el mando del jugador 1)
    bool padsFresh = false;  // tras resetPads: el primer sondeo toma el estado sin generar pulsaciones (un botón que ya estaba pulsado no cuenta)
  // hasta 4 jugadores; los modos de 1 jugador usan PL[0]
    for (int i = 0; i < game::MAX_PLAYERS; i++) { PL.push_back(std::make_unique<Player>()); Player& p = *PL[i]; p.pad = &pads[i]; p.in.p = &pads[i]; p.st.snd = &SND; p.st.slot = i; p.slot = i; }
    Player& P0 = *PL[0]; Pad& pad = pads[0]; Stats& st = P0.st; te::Game& g = P0.g; PadInput& in = P0.in; view::GameView& V = P0.V;  // alias de los modos de 1 jugador
    if (assets) V.init(&RES);
    game::VersusState vsState; bool inVs = false, vsLayout = false; int vsN = 2; std::vector<view::Telop> TLv; view::Telop BT[2]; int resSel = 0;  // partida VS (modo 10); BT = botones CONTINUE/QUIT del menú de resultado
    game::ItemSystem items; view::ItemView IV; if (assets) IV.load(&LR, ASSET_DIR); items.onSe = [&](int id) { SND.playSe(id); };  // ITEMS
    items.onGarbage = [&](int v, int n, int f) { for (int k = 0; k < n; k++) vsState.addGarbage(*PL[v], *PL[f >= 0 ? f : v]); };  // ITEMS: escalador aplastado = 5 líneas de basura
    bool vsIa = false; game::VsIaMenu VM;  // VSIA: Tetris vs. IA (modo 1) = partida VS de 2 campos (J1 + IA); VM = menú de resultado RETRY/CONTINUE/QUIT
    const bool testGhost = getenv("TPD_TEST_GHOST") != nullptr; bool testShot = false; int tf = 0, testCont = 1;
    int vsRating[4] = {0, 0, 0, 0};  // AUTOH: rating por puesto (notes_vs §1.5): h = clamp(h + rating/2, 0, 5); a 0 al elegir el modo en el menú
    game::VsConfig vsRun;  // configuración de la partida VS en curso (se copia de MN.vsCfg al empezar; CONTINUE la reutiliza)
    tpf::Rules R; int curMode = 0; game::ModeSpec spec = game::modeSpec(0);
    game::ClimberData climberData; climberData.load(std::string(RULES_DIR) + "climber.bin"); game::ClimberMode CM; view::ClimberView CV; bool climberLoaded = false, inClimber = false; int climberStage = 0; float climberAccum = 0;
    game::ShadowData shadowData; shadowData.load(std::string(RULES_DIR) + "shadow.bin"); game::ShadowMode shadow; shadow.onSe = [&](int id) { SND.playSe(id); }; int shadowStage = 0; bool inShadow = false;
    game::BomblissData bombData; bombData.load(std::string(RULES_DIR) + "bombliss.bin"); game::BomblissMode bomb; bomb.onSe = [&](int id) { SND.playSe(id); }; int bombPhase = 0; bool inBombliss = false;
    game::DropData dropData; dropData.load(std::string(RULES_DIR) + "drop.bin"); game::DropMode drop; drop.onSe = [&](int id) { SND.playSe(id); };
    int dropStage = argc > 2 ? atoi(argv[2]) % 10 : 0; bool dropEndless = argc > 3 && argv[3][0] == 'e'; int dropOpt = argc > 3 && argv[3][0] == 'c'; bool inDrop = false; float dropBest[10]; for (float& b : dropBest) b = -1;
    int hiScore[32] = {}; float bestTime[32]; for (float& b : bestTime) b = -1;
    // RECORDS: récords en disco (hiScore/bestTime/dropBest del HUD se rellenan desde REC con syncRec) y pantallas de fin de partida
    game::Records REC; REC.load(); gx::Arc UIWIN; UIWIN.load(std::string(ASSET_DIR) + "window.arc");
    result::ResultWindow RW; RW.load(&LR, &UIWIN); RW.onSe = [&](int id) { SND.playSe(id); }; result::ResultMenu RM; RM.load(&LR, &M, &TL.arc); RM.onSe = [&](int id) { SND.playSe(id); };
    bool newRecPending = false; int nrScore = -1; float nrTime = -1; result::Data resData;
    int stageOfMode = 0;  // etapa del modo en curso para la clave de récords (Sombra/Corredor/Escalador/Bombliss)
    auto recStage = [&]() { switch (spec.kind) { case game::Kind::Shadow: return shadowStage; case game::Kind::Drop: return dropStage; case game::Kind::Climber: return climberStage; case game::Kind::Bombliss: return bombPhase; default: return 0; } };
    auto syncRec = [&]() { stageOfMode = recStage(); int k = game::Records::key(curMode, stageOfMode), m = curMode & 31; hiScore[m] = REC.bestScore(k); bestTime[m] = REC.bestTime(k); if (spec.kind == game::Kind::Drop) dropBest[dropStage] = REC.bestTime(k); };
    // PAUSA: menú de pausa (+); interruptores de música/SE en SND, Ghost = preferencia del jugador (bloqueado en Maestro)
    pausemenu::PauseMenu PM; PM.load(&LR, &M, &TL.arc, &UIWIN); PM.onSe = [&](int id) { SND.playSe(id); }; bool ghostPref = true;
    auto applyGhost = [&]() { bool gh = spec.ghost && ghostPref; if (inVs) { for (int i = 0; i < vsN; i++) PL[i]->V.drawGhost = gh; } else V.drawGhost = gh; };
    PM.onChange = [&]() { SND.bgmOn = PM.bgm; SND.seOn = PM.se; if (!PM.ghostLocked) ghostPref = PM.ghost; applyGhost(); };
    Scene scene = dedication ? Scene::Dedication : title ? Scene::Title : Scene::Game; GamePhase phase = GamePhase::Countdown; float phaseTimer = 0; bool won = false;

    auto loadRules = [&](int mode) { spec = game::modeSpec(mode); std::string f = std::string(RULES_DIR) + spec.rules; auto buf = readFile(f); if (buf.empty()) { fprintf(stderr, "no se pudo leer %s\n", f.c_str()); return false; } R = tpf::parse(buf.data(), buf.size()); curMode = mode;
        { const game::ModeSettings& a = AJ.of(mode == 25 || bigVs(mode) ? 10 : mode); if (a.bg >= 0) spec.bg = a.bg; if (a.bgm != -1) spec.bgm = a.bgm; if (mode != 8) spec.ghost = a.ghost; if (a.endless && (spec.kind == game::Kind::Marathon || spec.kind == game::Kind::Master || spec.kind == game::Kind::Beginner)) spec.lineGoal = 0; }  // AJUSTES: Fondo, Música, Ghost (no en Maestro), Sin fin
        return true; };
    // ---- Tetris VS (modo 10): 2-4 jugadores locales, cada uno con su motor, entrada y vista ----
    auto resetPads = [&]() { for (Pad& p : pads) p = Pad(); padsFresh = true; };
    int alertSlot = -1;  // ALARMA: una sola por partida (flow+0x1940 del original), mientras algún jugador tenga la pila en zona de peligro
    auto stackTop = [](const te::Game& g) { for (int y = 0; y < g.h; y++) for (int x = 0; x < g.w; x++) if (g.cells[x + y * g.w].mino) return y; return g.h; };
    auto updateAlert = [&](bool anyDanger) { if (anyDanger && alertSlot < 0) alertSlot = SND.playSeLoop(snd::SE_ALERT); else if (!anyDanger && alertSlot >= 0) { SND.stopSe(alertSlot); alertSlot = -1; } };
    auto alertOff = [&]() { updateAlert(false); };
    auto setVsPlay = [&](bool on) { for (int i = 0; i < vsN; i++) { PL[i]->in.enabled = on; PL[i]->cpu.enabled = on; } };
    auto startVs = [&]() {
        vsIa = curMode == 1;  // VSIA
        vsRun = vsIa ? game::VsConfig() : MN.vsCfg; if (!vsRun.valid()) { vsRun = game::VsConfig(); }
        tf = 0;
        if (vsRun.autoHandicap) for (int j = 0; j < 4; j++) vsRun.handicap[j] = std::max(0, std::min(5, vsRun.handicap[j] + vsRating[j] / 2));  // AUTOH
        { const game::ModeSettings& a = AJ.of(curMode); if (a.bg < 0) spec.bg = (int)((((uint32_t)time(nullptr) * 2654435761u) >> 12) % 16); }  // FONDO: en VS con Fondo = auto, uno distinto en cada partida  // sin ventana de jugadores (arranque directo): J1 humano contra una IA (vs IA: siempre J1 + IA)
        { const game::ModeSettings& a = AJ.of(curMode); vsRun.aiLevel = a.aiLevel; vsRun.slippery = a.slippery; vsRun.mapPattern = a.map ? (int)((((uint32_t)time(nullptr) * 2654435761u) >> 16) % 20) : -1; items.enabled = !vsIa && !bigVs(curMode); items.mask = AJ.itemMask; }  // AJUSTES: Nivel IA, Penalización, Mapa (patrón único para todos), Objetos
        if (bigVs(curMode) && vsRun.activeCount() != bigVsN(curMode)) { vsRun = game::VsConfig(); for (int j = 0; j < game::MAX_PLAYERS; j++) { vsRun.type[j] = j >= bigVsN(curMode) ? game::VT_OFF : j == 0 || (int)joyIds.size() > j ? game::VT_HUMAN : game::VT_CPU; vsRun.team[j] = j; } }  // VS 6/8 lanzado sin pasar por el menú
        if (bigVs(curMode)) { vsRun.autoHandicap = false; for (int& h : vsRun.handicap) h = 0; }
        vsN = bigVs(curMode) ? bigVsN(curMode) : std::max(2, std::min(4, vsRun.activeCount()));
        LR.overrides.clear(); resetPads(); inVs = true; vsLayout = true; scene = Scene::Game; won = false; TL.stop(); TLv.clear();
        uint32_t seed = (uint32_t)time(nullptr); std::vector<game::PlayerBase*> ptrs; int k = 0;
        for (int j = 0; j < game::MAX_PLAYERS && k < vsN; j++) {
            if (vsRun.type[j] == game::VT_OFF) continue;
            Player& p = *PL[k]; view::ClimberView::detach(p.V); p.V.extra = nullptr; p.V.clipField = false; p.V.scrollY = 0; p.V.goalBoundary = -1; p.V.showTitle = false;
            if (!(bigVs(curMode) ? (curMode == 28 ? p.V.initVS8(&RES, k, k == 0 ? spec.bg : -1, vsRun.team[j]) : p.V.initVS6(&RES, k, k == 0 ? spec.bg : -1, vsRun.team[j])) : p.V.initVS(&RES, vsN, k, k == 0 ? spec.bg : -1, 3, vsRun.team[j], vsIa ? (k == 0 ? 3 : 17) : -1))) fprintf(stderr, "faltan frame00_02.arc / frame01_01.arc en %s\n", ASSET_DIR);
            bool cpu = vsRun.type[j] == game::VT_CPU;
            p.V.drawGhost = spec.ghost; p.g.init(R, seed); p.g.holdEnabled = true; p.g.lineGoal = 0; p.in.drop = false; p.in.enabled = false; p.st.state = 0; p.st.vs = &vsState; p.st.slot = k; p.st.snd = cpu ? nullptr : &SND;  // la IA no suena (solo los efectos de reglas de VersusState)
            p.practice = false; p.outTimer = 0; p.pad = &pads[j]; p.in.p = &pads[j]; p.cpu.enabled = false; p.cpu.planned = false; p.cpu.level = vsRun.aiLevel; p.cpu.seed = seed * 2246822519u + (uint32_t)(j + 1) * 3266489917u; if (!p.cpu.seed) p.cpu.seed = 1;
            p.g.in = cpu ? (te::Input*)&p.cpu : (te::Input*)&p.in;
            int kk = k; p.g.onLock = [&vsState, kk] { vsState.onLock(kk); }; p.g.spawnGate = [&vsState, kk] { return vsState.spawnGate(kk); }; p.g.onSpawn = nullptr;
            p.V.drawTop = [&p](view::GameView& v) { game::VersusState::drawPenalty(p, v); if (!p.alive) v.drawOut(p.g.w, p.g.h); };  // FANTASMA: velo + GAME OVER sobre el eliminado
            ptrs.push_back(&p); k++;
        }
        inShadow = inClimber = inDrop = inBombliss = false;
        vsState.onSe = [&](int id) { if (testGhost) fprintf(stderr, "SE vs %#x\n", id); SND.playSe(id); };
        vsState.start(ptrs, vsRun, seed);  // asigna equipos/hándicap y aplica las filas iniciales de basura
        applyGhost();  // PAUSA
        items.start(ptrs); IV.reset();  // ITEMS: tras vsState.start (copia p.cpu y encadena g.onSpawn/g.onLock delante de los de VersusState)
        SND.stopBgm(0); SND.playBgm(snd::JINGLE_START); TL.play("start.brlyt", "start_00.brlan"); phase = GamePhase::Countdown; phaseTimer = 0;
    };
    auto endVs = [&]() {
        setVsPlay(false); for (int i = 0; i < vsN; i++) items.clearEffects(i);  // ITEMS
        alertOff();
        if (vsRun.autoHandicap && !vsState.draw) {  // AUTOH: tabla 0x803FB1D8 por puesto (0 = ganador): 2 {+2,-2}; 3 {+2,0,-2}; 4 {+2,+1,-1,-2}
            static const int D[3][4] = {{2, -2, 0, 0}, {2, 0, -2, 0}, {2, 1, -1, -2}};
            std::vector<Player*> order; for (int i = 0; i < vsN; i++) order.push_back(PL[i].get());
            std::sort(order.begin(), order.end(), [](Player* a, Player* b) { int ra = a->alive ? 1000 : a->outOrder, rb = b->alive ? 1000 : b->outOrder; return ra > rb; });  // vivos primero, luego el último eliminado…
            for (int k = 0; k < vsN; k++) { int cs = order[k]->cfgSlot; if (cs >= 0 && cs < 4) vsRating[cs] = std::max(-10000, std::min(10000, vsRating[cs] + D[vsN - 2][k])); }
        }
        phase = GamePhase::Ending; phaseTimer = 0; SND.stopBgm(0.2f); if (!vsState.draw) SND.playBgm(vsIa && vsState.winner != 0 ? snd::BGM_GAMEOVER : snd::JINGLE_WINNER);  // empate: sin música (VSIA: gana la IA = BGM_GAMEOVER)
        if (vsIa) game::vsIaEndTelop(TLv, TL, vsState.draw, vsState.winner == 0); else  // VSIA: win_anim01 / lose_anim01 / draw_anim01
        vsState.playEndTelops(TLv, TL);  // draw / multi_win_anim%02d / team_win_anim%02d
        resSel = 0;
    };
    auto setResBtn = [&](int sel, bool init) {  // menú de resultado (máscara 5 = CONTINUE + QUIT): pause01 "CONTINUE", pause04 "QUIT"; el elegido se ilumina
        for (int i = 0; i < 2; i++) {
            const char* ly = i == 0 ? "pause01.brlyt" : "pause04.brlyt"; char an[32]; snprintf(an, sizeof an, "%s_anim0%d.brlan", i == 0 ? "pause01" : "pause04", i == sel ? 1 : 2);
            if (init) { BT[i].share(TL); BT[i].play(ly, an, 0, i == 0 ? -80 : -130); if (BT[i].L) if (lyt::Pane* t = BT[i].L->find(i == 0 ? "text_pause01" : "text_pause04")) t->text = M.get(14, i == 0 ? 15 : 16); }
            else { std::u16string tx; if (BT[i].L) if (lyt::Pane* t = BT[i].L->find(i == 0 ? "text_pause01" : "text_pause04")) tx = t->text; BT[i].play(ly, an, 0, i == 0 ? -80 : -130); if (BT[i].L) if (lyt::Pane* t = BT[i].L->find(i == 0 ? "text_pause01" : "text_pause04")) t->text = tx; }
        }
    };
    auto startGame = [&]() {
        if (spec.kind == game::Kind::Versus) { startVs(); return; }
        items.stop();  // ITEMS: fuera del VS se devuelve la entrada original y se apagan los ganchos
        if (vsLayout) { vsLayout = false; V.init(&RES, "Single", 1, -1); g.spawnGate = nullptr; g.onLock = nullptr; g.onSpawn = nullptr; g.in = &in; V.drawTop = nullptr; st.snd = &SND; st.slot = 0; }  // tras un VS la vista 0 vuelve al marco de 1 jugador y sin ganchos de VS
        inVs = false; st.vs = nullptr; st.slot = 0;
        g.init(R, (uint32_t)time(nullptr)); st.state = 0; scene = Scene::Game; won = false; in.enabled = false;
        V.showTitle = false; V.setBackground(spec.bg); V.setFieldConfig(spec.cfgLayout, spec.fieldGroup, 1); V.setFrameAnimName(spec.hudAnim); V.drawGhost = spec.ghost;
        g.lineGoal = spec.lineGoal; g.holdEnabled = true;
        { const game::ModeSettings& a = AJ.of(curMode); if (a.startLevel > 0 && (spec.kind == game::Kind::Marathon || spec.kind == game::Kind::Master || spec.kind == game::Kind::Beginner)) { g.levelBase = a.startLevel; g.setLevel(a.startLevel + 1); g.goal = 10; }  // AJUSTES: NIV inicial
          if (spec.kind == game::Kind::Beginner) { static const int ids[6] = {8, 2, 9, 10, 1, 7}; int n = a.tetriminos ? 6 : 4; g.bag.clear(); for (int i = 0; i < n; i++) g.bag.push_back({(uint32_t)ids[i], 1, 1}); g.queue.clear(); } }  // AJUSTES: Tetriminos FÁCIL (2I, O, 3I, ML) / NORMAL (+ I, T): FUN_8005fda8 / FUN_8005fc3c
        inShadow = (spec.kind == game::Kind::Shadow && shadowData.ok);
        if (inShadow) { shadow.start(g, shadowData, shadowStage); g.pieceBudget = shadow.maxPieces; V.setTitle(M.get(14, 0x3a + shadow.silhouette)); }
        inClimber = (spec.kind == game::Kind::Climber && climberData.ok);
        if (inClimber) {  // Escalador: etapa, basura inicial, banderas y escalador (notes_modes_special.md §2)
            CM.start(g, climberData, climberStage, (uint32_t)time(nullptr)); CM.accumMs = climberAccum; CM.onSe = [&](int id) { SND.playSe(id); };
            if (!climberLoaded) { climberLoaded = true; CV.onSe = [&](int id) { SND.playSe(id); }; CV.load(&LR, V, ASSET_DIR, &CM); }
            CV.attach(V); CV.reset(); g.lineGoal = 0;
        } else view::ClimberView::detach(V);
        inDrop = (spec.kind == game::Kind::Drop && dropData.ok); in.drop = inDrop; V.clipField = inDrop; V.scrollY = 0; V.goalBoundary = -1;
        if (inDrop) { drop.start(g, dropData, dropStage, dropEndless, dropOpt, (uint32_t)time(nullptr)); V.setTitle(M.get(14, 0x2c + dropStage)); }
        inBombliss = (spec.kind == game::Kind::Bombliss && bombData.ok); V.extra = nullptr;
        if (inBombliss) { bomb.start(g, bombData, bombPhase); V.extra = &bomb; }
        SND.stopBgm(0); SND.playBgm(snd::JINGLE_START); TL.play("start.brlyt", "start_00.brlan"); phase = GamePhase::Countdown; phaseTimer = 0;
        syncRec(); applyGhost(); RW.st = result::ResultWindow::Closed; RM.shown = false; newRecPending = false;  // RECORDS / PAUSA
    };
    auto playEndTelop = [&](bool victory) {  // telop de fin: CLEARED!/FINISH + JINGLE_CLEAR, o GAME OVER + BGM_GAMEOVER
        if (victory) { SND.playBgm(snd::JINGLE_CLEAR); bool finish = spec.kind == game::Kind::Sprint || spec.kind == game::Kind::AllClear; TL.play(finish ? "finish.brlyt" : "clear.brlyt", finish ? "finish_anim.brlan" : "clear_Tag_00.brlan"); }
        else { SND.playBgm(snd::BGM_GAMEOVER); TL.play("gameover.brlyt", "gameover_Tag_00.brlan"); }
    };
    // RECORDS: actualiza el top-5 (puntuación y tiempo) de este modo/etapa, guarda en disco y decide si hay cartel NEW RECORD (§2.1/§2.4)
    auto recordEnd = [&](bool victory) {
        using game::Kind; nrScore = -1; nrTime = -1; int k = game::Records::key(curMode, recStage()); Kind kd = spec.kind;
        if (AJ.of(curMode).startLevel > 0) { resData = result::Data(); resData.kind = kd; resData.victory = victory; resData.bestScore = REC.bestScore(k); resData.bestTime = REC.bestTime(k); resData.score = g.visScore; resData.time = g.gameTime; return; }  // AJUSTES: con NIV inicial > 1 no se guardan récords (aviso #0xcb)
        int score = inBombliss ? (victory ? bomb.finalPoints : 0) : (kd == Kind::Shadow || kd == Kind::Drop || kd == Kind::Climber) ? 0 : g.visScore;
        float t = inClimber ? CM.accumMs / 1000.0f : g.gameTime;
        bool scoreKind = kd == Kind::Marathon || kd == Kind::Master || kd == Kind::Sprint || kd == Kind::Beginner || kd == Kind::AllClear || kd == Kind::Bombliss;
        if (scoreKind && REC.addScore(k, score) == 0 && (kd == Kind::Marathon || kd == Kind::Master || kd == Kind::Beginner || kd == Kind::Bombliss)) nrScore = score;  // Sprint/All Clear: la puntuación se guarda en silencio
        if (victory && !(inDrop && dropEndless) && REC.addTime(k, t) == 0 && (kd == Kind::Sprint || kd == Kind::AllClear || kd == Kind::Shadow || kd == Kind::Drop || kd == Kind::Climber)) nrTime = t;  // Maratón/Maestro/Principiantes: sin NEW RECORD por tiempo
        if (REC.dirty) REC.save(); syncRec();
        resData = result::Data(); resData.kind = kd; resData.victory = victory; resData.bestScore = REC.bestScore(k); resData.bestTime = REC.bestTime(k); resData.score = score; resData.time = t;
    };
    auto endGame = [&](bool victory) { alertOff();
        won = victory; in.enabled = false; phase = GamePhase::Ending; phaseTimer = 0; SND.stopBgm(0.2f);
        recordEnd(victory);  // RECORDS
        if (nrScore >= 0 || nrTime >= 0) { SND.playSe(0x7a); result::playNewRecord(TL, nrScore, nrTime); newRecPending = true; }  // NEW RECORD antes del telop (SE_63_NEW_RECORD)
        else playEndTelop(victory);
    };
    auto openSummary = [&]() { TL.stop(); if (RW.open(result::makeRows(M, resData))) phase = GamePhase::Summary; else { RM.open(won); phase = GamePhase::Result; } };  // RECORDS
    auto hudTexts = [&]() {
        int m = curMode & 31; char buf[32];
        auto setN = [&](const char* pane, int v) { snprintf(buf, sizeof buf, "%d", v); LR.setText(pane, buf); };
        auto setS = [&](const char* pane, const std::string& s) { LR.setText(pane, s); };
        switch (spec.kind) {
        case game::Kind::Drop: setN("TextBox_lines", std::min(dropEndless ? 9999 : 400, drop.rows)); setN("TextBox_bestline", 0); setS("TextBox_time", fmtTime(g.gameTime)); setS("TextBox_btime", dropBest[dropStage] >= 0 ? fmtTime(dropBest[dropStage]) : "--'--\"--"); break;
        case game::Kind::Climber: { setN("TextBox_stage", climberStage + 1); float t = CM.accumMs + (CM.goalRunning ? 0 : CM.stageMs); setS("TextBox_time_00", fmtTime(t / 1000.0f)); setS("TextBox_btime_01", bestTime[m] >= 0 ? fmtTime(bestTime[m]) : "--'--\"--"); break; }
        case game::Kind::Shadow: setN("TextBox_skip", shadow.remaining()); snprintf(buf, sizeof buf, "%d%%", shadow.percentShown()); setS("TextBox_match", buf); { int t = (int)ceilf(shadow.timeLeft()); snprintf(buf, sizeof buf, "%02d:%02d", t / 60, t % 60); setS("TextBox_time", buf); } break;
        case game::Kind::Bombliss: setS("TextBox_time", fmtTime(g.gameTime)); setN("TextBox_point", bomb.points()); setN("TextBox_level", bombPhase + 1); setN("TextBox_hiscore", hiScore[m]); setN("TextBox_total", hiScore[m]); break;
        case game::Kind::Sprint: setN("TextBox_lines_01", std::max(0, g.lineGoal - g.lines)); setN("TextBox_level_01", g.level); setN("TextBox_score", g.visScore); setS("TextBox_time_01", fmtTime(g.gameTime)); setS("TextBox_btime_02", bestTime[m] >= 0 ? fmtTime(bestTime[m]) : "--'--\"--"); break;
        case game::Kind::Beginner: setN("TextBox_lines_00", std::min(9999, g.lines)); setN("TextBox_level_00", g.level); setN("TextBox_allclear", g.allClears); setS("TextBox_time", fmtTime(g.gameTime)); setN("TextBox_score", g.visScore); setN("TextBox_hiscore", hiScore[m]); break;
        case game::Kind::AllClear: setN("TextBox_lines_00", std::min(9999, g.lines)); setN("TextBox_level_00", g.level); setN("TextBox_allclear", std::max(0, 20 - g.allClears)); setS("TextBox_time", fmtTime(g.gameTime)); setN("TextBox_score", g.visScore); setS("TextBox_btime", bestTime[m] >= 0 ? fmtTime(bestTime[m]) : "--'--\"--"); break;
        default: setN("TextBox_lines_01", std::min(9999, g.lines)); setN("TextBox_level_01", g.level); setN("TextBox_score_00", g.visScore); setN("TextBox_hisco_00", hiScore[m]); setS("TextBox_time", fmtTime(g.gameTime)); setS("TextBox_time_01", fmtTime(g.gameTime)); break;
        }
    };
    if (!loadRules(argc > 1 ? atoi(argv[1]) : 0)) return 1;
    if (title && !dedication) SND.playBgm(snd::BGM_TITLE);
    if (scene == Scene::Game) startGame();
    if (testGhost) { for (int j = 0; j < 3; j++) MN.vsCfg.type[j] = game::VT_HUMAN; MN.vsCfg.type[3] = game::VT_OFF; MN.vsCfg.handicap[1] = 3; MN.vsCfg.autoHandicap = getenv("TPD_TEST_AUTOH") != nullptr; scene = Scene::Game; startGame(); }
    // mando (instance id) → Pad del jugador: en partida VS el k-ésimo mando conectado es el jugador k; en menús y modos de 1 jugador todos mueven al jugador 1
    // Estado de los mandos por sondeo cada frame (SDL_JoystickGet*): botones 0..15, cruceta como hat y sticks como ejes 0/1. En VS el k-ésimo mando es el jugador k; si no, todos al jugador 1.
    auto pollPads = [&]() {
        bool cur[game::MAX_PLAYERS][B_COUNT] = {}; const int T = 16000;
        for (size_t k = 0; k < joyIds.size(); k++) {
            SDL_Joystick* js = SDL_JoystickFromInstanceID(joyIds[k]); if (!js) continue; int slot = (inVs && scene == Scene::Game) ? std::min((int)k, game::MAX_PLAYERS - 1) : 0;
            for (int b = 0; b < 16; b++) { int lb = Pad::fromJoy(b); if (lb >= 0 && SDL_JoystickGetButton(js, b)) cur[slot][lb] = true; }
            if (SDL_JoystickNumHats(js) > 0) { int v = SDL_JoystickGetHat(js, 0); if (v & SDL_HAT_LEFT) cur[slot][B_LEFT] = true; if (v & SDL_HAT_RIGHT) cur[slot][B_RIGHT] = true; if (v & SDL_HAT_UP) cur[slot][B_UP] = true; if (v & SDL_HAT_DOWN) cur[slot][B_DOWN] = true; }
            if (SDL_JoystickNumAxes(js) > 1) { int x = SDL_JoystickGetAxis(js, 0), y = SDL_JoystickGetAxis(js, 1); if (x < -T) cur[slot][B_LEFT] = true; if (x > T) cur[slot][B_RIGHT] = true; if (y < -T) cur[slot][B_UP] = true; if (y > T) cur[slot][B_DOWN] = true; }
        }
        for (int b = 0; b < B_COUNT; b++) if (kb[b]) cur[0][b] = true;
        for (int sl = 0; sl < game::MAX_PLAYERS; sl++) for (int b = 0; b < B_COUNT; b++) { if (padsFresh) pads[sl].down[b] = cur[sl][b]; else pads[sl].set(b, cur[sl][b]); }
        padsFresh = false;
    };
    auto joyPad = [&](SDL_JoystickID id) -> Pad& { if (!inVs || scene != Scene::Game) return pads[0]; auto it = std::find(joyIds.begin(), joyIds.end(), id); int k = it == joyIds.end() ? 0 : (int)(it - joyIds.begin()); return pads[std::min(k, game::MAX_PLAYERS - 1)]; };
    bool running = true; Uint32 last = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_JOYDEVICEADDED) addJoy(e.jdevice.which);
            else if (e.type == SDL_JOYDEVICEREMOVED) joyIds.erase(std::remove(joyIds.begin(), joyIds.end(), (SDL_JoystickID)e.jdevice.which), joyIds.end());
            else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) { int b = Pad::fromKey(e.key.keysym.sym); if (b >= 0) kb[b] = e.type == SDL_KEYDOWN; }
        }
        pollPads();
        Uint32 now = SDL_GetTicks(); float dt = (now - last) / 1000.0f; last = now; if (dt > 0.1f) dt = 0.1f;
        if (testGhost) dt = 1.0f / 60;

        SDL_RenderSetViewport(rn, nullptr); SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn);
        refreshViewport(scene == Scene::Game && inVs && bigVs(curMode)); SDL_RenderSetViewport(rn, &vp);
        if (scene == Scene::Dedication) {
            if (pad.pressed[B_A] || pad.pressed[B_PLUS]) DED.skip();
            DED.update(dt); DED.draw();
            if (DED.done) { bool direct = getenv("TP_SNAP") && argc > 1; /* depuración: con TP_SNAP y modo por argumento se salta el título */ scene = (title && !direct) ? Scene::Title : Scene::Game; if (scene == Scene::Title) SND.playBgm(snd::BGM_TITLE); else startGame(); }
        } else if (scene == Scene::Title) {
            if (pad.pressed[B_PLUS]) running = false;
            if (pad.pressed[B_A]) { if (!T.inDone) T.skipIntro(); else if (hasMenu) { SND.playSe(snd::SE_DECISION_B); MN.open(); scene = Scene::Menu; SND.playBgm(snd::BGM_MENU_A); } else startGame(); }
            T.update(dt); T.draw();
        } else if (scene == Scene::Menu) {
            menu::Input mi; mi.up = pad.pressed[B_UP]; mi.down = pad.pressed[B_DOWN]; mi.left = pad.pressed[B_LEFT]; mi.right = pad.pressed[B_RIGHT]; mi.a = pad.pressed[B_A]; mi.b = pad.pressed[B_B]; mi.x = pad.pressed[B_X]; mi.plus = pad.pressed[B_PLUS];
            MN.joyCount = (int)joyIds.size(); MN.update(dt, mi); MN.draw();
            if (MN.startMode >= 0) { int m = MN.startMode == 25 ? 10 : MN.startMode;  /* el 27 (VS 6) conserva su id */ MN.vsCfg.autoHandicap = MN.startMode == 25; for (int& r : vsRating) r = 0; loadRules(m); startGame(); }  // modo 0x19 (VS auto-hándicap) = 10 con auto-hándicap
            else if (MN.backToTitle) { scene = Scene::Title; SND.playBgm(snd::BGM_TITLE); }
        } else {
            // PAUSA: entrada de menú de cualquier mando, apertura y paso del menú de pausa
            auto uiInput = [&]() { menu::Input mi; for (Pad& q : pads) { mi.up |= q.pressed[B_UP]; mi.down |= q.pressed[B_DOWN]; mi.left |= q.pressed[B_LEFT]; mi.right |= q.pressed[B_RIGHT]; mi.a |= q.pressed[B_A]; mi.b |= q.pressed[B_B]; mi.plus |= q.pressed[B_PLUS]; } return mi; };
            auto pauseOpen = [&]() { alertOff(); PM.ghostLocked = !spec.ghost; PM.ghost = spec.ghost && ghostPref; PM.bgm = SND.bgmOn; PM.se = SND.seOn; PM.open(); SND.setBgmDuck(0.25f); };
            auto toMenu = [&]() { alertOff(); SND.stopBgm(0.3f); TL.stop(); TLv.clear(); inVs = false; resetPads(); if (hasMenu) { MN.open(); scene = Scene::Menu; SND.playBgm(snd::BGM_MENU_A); } else if (title) scene = Scene::Title; else running = false; };
            auto pauseStep = [&](float d) { PM.update(d, uiInput()); if (PM.finished) { SND.setBgmDuck(1.0f); if (PM.result == pausemenu::PauseMenu::Retry) startGame(); else if (PM.result == pausemenu::PauseMenu::Quit) toMenu(); } };
            if (inVs) {  // ---- Tetris VS: cuenta atrás común, cada motor con su entrada; el último vivo gana ----
                bool anyA = false, anyB = false, anyPlus = false; for (int i = 0; i < game::MAX_PLAYERS; i++) { anyA |= pads[i].pressed[B_A]; anyB |= pads[i].pressed[B_B]; anyPlus |= pads[i].pressed[B_PLUS]; }
                if (testGhost && inVs && phase == GamePhase::Result && testCont > 0) { testCont--; anyA = true; fprintf(stderr, "TEST ganador %d ratings %d,%d,%d → CONTINUE\n", vsState.winner, vsRating[0], vsRating[1], vsRating[2]); }
                TL.update(dt);
                if (phase == GamePhase::Countdown) {
                    if (anyPlus) toMenu();
                    else if (TL.done()) { TL.stop(); phase = GamePhase::Play; setVsPlay(true); SND.playBgm(spec.bgm); }
                } else if (phase == GamePhase::Play) {
                    if (PM.active) pauseStep(dt);  // PAUSA
                    else if (anyPlus) pauseOpen();
                    else {
                        for (int i = 0; i < vsN; i++) { Pad& q = *PL[i]->pad; items.input(i, q.pressed[B_Y], q.pressed[B_Y] || q.pressed[B_A] || q.pressed[B_B] || q.pressed[B_X] || q.pressed[B_L] || q.pressed[B_R], q.down[B_LEFT], q.down[B_RIGHT]); }  // ITEMS: B_Y usa el objeto; cualquier botón agita el humo (la CPU lo hace sola)
                        items.update(dt);  // ITEMS
                        for (int i = 0; i < vsN; i++) { Player& p = *PL[i]; if (p.alive) { p.g.update(items.dt(i, dt)); /* ITEMS: dt=0 con "parar el tiempo" */ continue; }
                            // FANTASMA: el humano eliminado ve su campo congelado 1,5 s y luego sigue practicando con un campo nuevo; no ataca ni recibe basura ni objetos (alive = false)
                            if (p.game::PlayerBase::cpu) continue;  // la CPU no practica (g.in lo envuelve el sistema de objetos: no sirve para distinguir)
                                                        if (p.g.state == 2) { p.outTimer += dt; if (p.outTimer > 1.5f) { p.outTimer = 0; p.practice = true; p.st.vs = nullptr; p.pendingGarbage.clear(); p.g.init(R, (uint32_t)time(nullptr) * 2654435761u + (uint32_t)i * 40503u + 1u); p.g.holdEnabled = true; p.g.lineGoal = 0; p.st.state = 0; } }
                            else if (p.practice) p.g.update(dt);
                        }
                        { bool d = false; for (int i = 0; i < vsN; i++) { Player& p = *PL[i]; if ((p.alive || p.practice) && p.g.state == 0 && stackTop(p.g) < p.g.h - 16) d = true; } updateAlert(d); }  // ALARMA
                        if (vsState.update()) endVs();
                        if (testGhost) {  // prueba sin pantalla (TPD_TEST_GHOST=1 ./tetris-mac 10): J1 pierde a los 2 s y debe seguir practicando
                            tf++; if (tf == 120) PL[0]->g.lose(); if (tf == 300 && testCont > 0) PL[1]->g.lose();
                            auto minos = [&](int k) { int c = 0; for (auto& ce : PL[k]->g.cells) if (ce.mino) c++; return c; };
                            if (tf == 5) fprintf(stderr, "TEST f5: minos J1 %d J2 %d J3 %d (hándicap J2=3 → 6 filas con 3..6 huecos ≈ 24..42) mapa %d hand %d,%d,%d auto %d\n", minos(0), minos(1), minos(2), vsRun.mapPattern, vsRun.handicap[0], vsRun.handicap[1], vsRun.handicap[2], (int)vsRun.autoHandicap);
                            if (tf == 60) { for (int q = 0; q < 3; q++) vsState.addGarbage(*PL[2], *PL[1]); fprintf(stderr, "TEST f60: 3 basuras a J3, pendientes %d\n", (int)PL[2]->pendingGarbage.size()); PL[2]->g.hardPend = 15; }
                            if (tf == 500) { te::Game& q = PL[2]->g; for (int y = q.h - 17; y < q.h; y++) for (int x = 1; x < q.w; x++) { te::Cell c; c.mino = 2; q.cells[x + y * q.w] = c; } }
                            if (tf == 510) fprintf(stderr, "TEST f510: pila J3 17 filas → alarma %s (esperado ON)\n", alertSlot >= 0 ? "ON" : "off");
                            if (tf == 520) { te::Game& q = PL[2]->g; for (auto& c : q.cells) c = te::Cell(); }
                            if (tf == 530) fprintf(stderr, "TEST f530: campo J3 vacío → alarma %s (esperado off)\n", alertSlot >= 0 ? "ON" : "off");
                            if (tf == 100) fprintf(stderr, "TEST f100: J3 pendientes %d minos %d piezas %d\n", (int)PL[2]->pendingGarbage.size(), minos(2), PL[2]->g.lockCount);
                            if (tf == 130 || tf == 230 || tf == 1500) fprintf(stderr, "GHOST f%d: vivo %d practice %d state %d hasPiece %d piezas %d basura %d\n", tf, (int)PL[0]->alive, (int)PL[0]->practice, PL[0]->g.state, (int)PL[0]->g.hasPiece, PL[0]->g.lockCount, (int)PL[0]->pendingGarbage.size());
                            if (tf == 400) testShot = true;
                            if (tf == 1500) running = false;
                        }
                    }  // update() elimina a quien tenga g.state == 2
                } else if (phase == GamePhase::Ending) {
                    bool all = true; for (auto& t : TLv) { t.update(dt); all &= t.done(); } phaseTimer += dt; if (all && phaseTimer > 1.5f) { phase = GamePhase::Result; if (vsIa) VM.open(TL, &M, vsState.winner == 0 && vsRun.aiLevel + 1 < 15); else setResBtn(0, true); }  // VSIA: máscara 7 (con CONTINUE) si gana J1 y nivelIA+1 < 15, si no 6
                } else if (vsIa) {  // VSIA: resultado de vs IA (RETRY / CONTINUE / QUIT): CONTINUE sube el nivel de la IA (FUN_8012513c)
                    for (auto& t : TLv) t.update(dt); VM.update(dt);
                    bool up = false, dn = false; for (int i = 0; i < game::MAX_PLAYERS; i++) { up |= pads[i].pressed[B_UP]; dn |= pads[i].pressed[B_DOWN]; }
                    if ((up && VM.move(-1)) || (dn && VM.move(1))) SND.playSe(13);
                    if (anyA) { int c = VM.choice(); if (c == game::VIA_QUIT) { SND.playSe(snd::SE_CANCEL); toMenu(); } else { SND.playSe(snd::SE_DECISION_A); if (c == game::VIA_CONTINUE) { game::ModeSettings& a = AJ.of(1); if (a.aiLevel < 14) { a.aiLevel++; AJ.save(); } } TLv.clear(); startGame(); } }
                    else if (anyB || anyPlus) { SND.playSe(snd::SE_CANCEL); toMenu(); }
                } else {  // Result (menú CONTINUE / QUIT): arriba/abajo elige, A confirma (CONTINUE reinicia con la misma configuración), B/+ = QUIT
                    for (auto& t : TLv) t.update(dt); for (auto& b : BT) b.update(dt);
                    bool up = false, dn = false; for (int i = 0; i < game::MAX_PLAYERS; i++) { up |= pads[i].pressed[B_UP]; dn |= pads[i].pressed[B_DOWN]; }
                    if ((up || dn) && resSel == (up ? 1 : 0)) { resSel = up ? 0 : 1; SND.playSe(13); setResBtn(resSel, false); }
                    if (anyA) { if (resSel == 0) { SND.playSe(snd::SE_DECISION_A); TLv.clear(); startGame(); } else { SND.playSe(snd::SE_CANCEL); toMenu(); } }
                    else if (anyB || anyPlus) { SND.playSe(snd::SE_CANCEL); toMenu(); }
                }
            } else if (phase == GamePhase::Countdown) {
                TL.update(dt); if (pad.pressed[B_PLUS]) toMenu();
                if (TL.done()) { TL.stop(); phase = GamePhase::Play; in.enabled = true; SND.playBgm(spec.bgm); }
            } else if (phase == GamePhase::Play) {
                if (PM.active) pauseStep(dt); else if (pad.pressed[B_PLUS]) pauseOpen(); else {  // PAUSA
                if (inDrop) { drop.input(pad.down[B_DOWN], pad.down[B_UP], pad.pressed[B_UP]); drop.update(dt); }
                if (inClimber) { if (CM.state != game::CS_DEAD) g.update(dt); CM.update(dt); CV.update(dt); if (CM.finished) { endGame(CM.cleared); } }
                else g.update(dt);
                if (!inShadow && !inDrop && !inBombliss && !inClimber) updateAlert(g.state == 0 && stackTop(g) < g.h - 16);  // ALARMA (modos de 1 jugador normales)
                if (inClimber) {}
                else if (inDrop) { V.scrollY = drop.scrollY(); V.goalBoundary = drop.goalBoundary(); if (drop.cleared) endGame(true); else if (g.state == 2) endGame(false); }
                else if (inBombliss) { bomb.update(dt); if (bomb.finished) endGame(bomb.cleared); }
                else if (inShadow) { shadow.update(dt); if (shadow.finished) endGame(shadow.cleared); }
                else if (g.state == 2) endGame(false);
                else if (g.lineGoal > 0 && g.lines >= g.lineGoal) endGame(true);
                else if (spec.allClearGoal > 0 && g.allClears >= spec.allClearGoal) endGame(true);
                else if (g.state == 1) endGame(true);
                }  // PAUSA
            } else if (phase == GamePhase::Ending) {
                TL.update(dt); phaseTimer += dt; if (inShadow) shadow.update(dt); if (inClimber) CV.update(dt);
                if (newRecPending) { if (TL.done() && phaseTimer > 3.0f) { newRecPending = false; playEndTelop(won); phaseTimer = 0; } }  // RECORDS: NEW RECORD (3 s) y luego el telop
                else if (TL.done() && phaseTimer > 1.5f) openSummary();  // RECORDS: ventana de resultados
            } else if (phase == GamePhase::Summary) {  // RECORDS: single_result; A cierra y pasa al menú de resultado
                if (inShadow) shadow.update(dt); if (inClimber) CV.update(dt);
                if (RW.update(dt, pad.pressed[B_A])) { RM.open(won); phase = GamePhase::Result; }
            } else {  // Result: menú CONTINUE (tras completar) / RETRY (tras game over) / QUIT (también B/+); CONTINUE = siguiente etapa en Sombra/Escalador/Bombliss/Corredor
                TL.update(dt); result::ResultMenu::Choice ch = RM.update(dt, uiInput());
                if (ch == result::ResultMenu::Quit) toMenu();
                else if (ch != result::ResultMenu::None) { if (inShadow && shadow.cleared) shadowStage = (shadowStage + 1) % 50; if (inClimber && CM.cleared) { climberAccum = CM.accumMs; if (climberStage >= 9) { climberStage = 0; climberAccum = 0; } else climberStage++; } if (inBombliss && bomb.cleared) bombPhase = (bombPhase + 1) % 30; if (inDrop && drop.cleared && !dropEndless) dropStage = dropStage == 4 ? 0 : dropStage == 9 ? 5 : dropStage + 1;  /* FUN_800bb2bc: 4->0, 9->5 */ TL.stop(); startGame(); }
            }
            if (scene == Scene::Game && inVs) { if (assets) for (int i = 0; i < vsN; i++) { game::VersusState::hud(*PL[i]); if (vsIa && i == 1) PL[i]->V.setTextS("TextBox_comLV", std::to_string(vsRun.aiLevel + 1)); /* VSIA */ PL[i]->V.draw(PL[i]->g, dt); IV.draw(items, i, PL[i]->V, dt); /* ITEMS */ } TL.draw(); for (auto& t : TLv) t.draw(); if (phase == GamePhase::Result) { if (vsIa) VM.draw(); else for (auto& b : BT) b.draw(); } }  // VSIA
            else if (scene == Scene::Game) { hudTexts(); if (assets) V.draw(g, dt); TL.draw(); }
            if (scene == Scene::Game) { RW.draw(); RM.draw(); PM.draw(); }  // RECORDS / PAUSA (las partes se animan en su propio update)
        }
        for (Pad& p : pads) p.endFrame();
#ifndef __SWITCH__
        { static int snapN = getenv("TP_SNAP") ? atoi(getenv("TP_SNAP")) : -1; static int fr = 0; if (snapN >= 0 && ++fr == snapN) {  // depuración (Mac): TP_SNAP=frames TP_SNAP_OUT=fichero.bmp guarda la pantalla y sale
            int w, h; SDL_GetRendererOutputSize(rn, &w, &h); SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32); SDL_RenderSetViewport(rn, nullptr);
            if (sf && SDL_RenderReadPixels(rn, nullptr, SDL_PIXELFORMAT_RGBA32, sf->pixels, sf->pitch) == 0) SDL_SaveBMP(sf, getenv("TP_SNAP_OUT") ? getenv("TP_SNAP_OUT") : "/tmp/tp_snap.bmp"); if (sf) SDL_FreeSurface(sf); running = false; } }
#endif
        if (testShot) { testShot = false; int w, h; SDL_GetRendererOutputSize(rn, &w, &h); SDL_Surface* sh = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32); SDL_RenderReadPixels(rn, nullptr, SDL_PIXELFORMAT_RGBA32, sh->pixels, sh->pitch); SDL_SaveBMP(sh, "/tmp/ghost.bmp"); SDL_FreeSurface(sh); }
        SDL_RenderPresent(rn);
    }
    SDL_DestroyRenderer(rn); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
