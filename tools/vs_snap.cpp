// Captura de una partida VS local (2-4 jugadores): vs_snap salida.bmp [jugadores=2] [frames=600] [frames de telop=300] [pozo=0|1]
//   Se detiene al quedar un solo jugador vivo (entonces muestra el telop de fin tras [frames de telop]); pozo=1 prerrellena la fila inferior para provocar despejes.
//   Entradas aleatorias por defecto. Variables de entorno (verificación de las reglas VS):
//     VS_CPU=1111   máscara por jugador (J1..J4): 1 = lo juega la IA (game/vs_ai.hpp); VS_LEVEL=0..14 nivel de IA
//     VS_TEAMS=0,0,1,1   color de equipo por jugador; VS_HANDICAP=0,3,0,0   hándicap 0..5; VS_SLIP=1   Resbaladizo
//     VS_GARBAGE=n[,jugador]   mete n líneas de basura pendientes (de un atacante ficticio) al jugador (def. J1) al empezar
//     VS_IA=1   Tetris vs. IA (modo 1, VSIA): 2 jugadores, J1 aleatorio + IA (VS_LEVEL), marcos _03/_17, TextBox_comLV, telop win/lose; VS_RESULT=n sigue marcando el botón n
//     VS_VERBOSE=1   imprime los envíos de basura; VS_ANIM=n   anim del marco 3-4P
#include "../gfx/game_view.hpp"
#include "../gfx/telop.hpp"
#include "../game/versus.hpp"
#include "../game/vs_ai.hpp"
#include "../game/vs_ia.hpp"
#include "../gfx/msg.hpp"
#include <fstream>
#include <memory>
struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    bool well = false;  // "pozo": prerrellena la fila inferior salvo la columna 0 y empuja las piezas a la izquierda (para provocar despejes)
    void poll() override { frame++; hd = false; if (frame % 6 == 0) { unsigned v = rnd(); l = well ? true : v & 1; r = well ? false : (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 15) == 0; ho = ((v >> 8) & 63) == 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return ho; }
};
struct SnapClient : te::Client {  // igual que Stats de main_sdl.cpp: reenvía los despejes al punto de enganche de ataque
    game::VersusState* vs = nullptr; int slot = 0; int clears = 0, lineClears = 0;
    void clearEvent(int lines, int spin, bool b2b, bool allClear, int combo) override { clears++; if (lines > 0) lineClears++; if (vs) vs->onLinesCleared(slot, lines, spin, b2b, allClear, combo); }
};
struct SnapPlayer : game::PlayerBase { RandomInput in; game::CpuInput cpuIn; SnapClient cl; };
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "uso: vs_snap salida.bmp [jugadores] [frames] [telop] [pozo]\n"); return 1; }
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    bool vsIa = getenv("VS_IA") && atoi(getenv("VS_IA"));  // VSIA
    int n = vsIa ? 2 : argc > 2 ? atoi(argv[2]) : 2; if (n < 2) n = 2; if (n > 4) n = 4; int frames = argc > 3 ? atoi(argv[3]) : 600; int telopFrames = argc > 4 ? atoi(argv[4]) : 300; bool well = argc > 5 && atoi(argv[5]);
    view::GameRes res; if (!res.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    auto buf = readFile("data/8044aa58.tpf2"); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    game::VersusState vs; std::vector<std::unique_ptr<SnapPlayer>> PL; std::vector<game::PlayerBase*> base;
    game::VsConfig cfg; for (int i = 0; i < 4; i++) cfg.type[i] = i < n ? game::VT_HUMAN : game::VT_OFF;
    auto envList = [&](const char* name, int* out, int cnt) { const char* e = getenv(name); if (!e) return; for (int i = 0; i < cnt && *e; i++) { out[i] = atoi(e); while (*e && *e != ',') e++; if (*e == ',') e++; } };
    envList("VS_TEAMS", cfg.team, 4); envList("VS_HANDICAP", cfg.handicap, 4); cfg.slippery = getenv("VS_SLIP") && atoi(getenv("VS_SLIP")); if (getenv("VS_LEVEL")) cfg.aiLevel = atoi(getenv("VS_LEVEL"));
    if (vsIa) { cfg.type[0] = game::VT_HUMAN; cfg.type[1] = game::VT_CPU; cfg.team[0] = 0; cfg.team[1] = 1; }
    const char* cpuMask = getenv("VS_CPU"); if (cpuMask) for (int i = 0; i < n && cpuMask[i]; i++) if (cpuMask[i] == '1') cfg.type[i] = game::VT_CPU;
    bool verbose = getenv("VS_VERBOSE") && atoi(getenv("VS_VERBOSE"));
    for (int i = 0; i < n; i++) {
        auto p = std::make_unique<SnapPlayer>(); if (!p->V.initVS(&res, n, i, i == 0 ? 15 : -1, getenv("VS_ANIM") ? atoi(getenv("VS_ANIM")) : 3, cfg.team[i], vsIa ? (i == 0 ? 3 : 17) : -1)) { fprintf(stderr, "marcos VS (frame00_02.arc / frame01_01.arc)\n"); return 1; }
        p->in.s = 777 + 1000 * i; p->cl.vs = &vs; p->cl.slot = i; p->g.cl = &p->cl; p->g.holdEnabled = true; p->g.init(rules, 99); p->in.well = well;
        if (cfg.type[i] == game::VT_CPU) { p->cpuIn.g = &p->g; p->cpuIn.level = cfg.aiLevel; p->cpuIn.seed = 4242 + i * 77; p->g.in = &p->cpuIn; } else p->g.in = &p->in;
        if (well) for (int y = p->g.h - 1; y < p->g.h; y++) for (int x = 1; x < p->g.w; x++) { te::Cell c; c.mino = 2 + (x + y) % 7; p->g.cells[x + y * p->g.w] = c; }
        base.push_back(p.get()); PL.push_back(std::move(p));
    }
    vs.start(base, cfg, 99);
    for (int i = 0; i < n; i++) { game::PlayerBase* pb = base[i]; pb->g.onLock = [&vs, i] { vs.onLock(i); }; pb->g.spawnGate = [&vs, i] { return vs.spawnGate(i); }; pb->V.drawTop = [pb](view::GameView& v) { game::VersusState::drawPenalty(*pb, v); }; }
    if (const char* gs = getenv("VS_GARBAGE")) { int cnt = atoi(gs), who = 0; if (const char* c = strchr(gs, ',')) who = atoi(c + 1); if (who >= 0 && who < n) for (int k = 0; k < cnt; k++) { game::PlayerBase& from = *base[(who + 1) % n]; vs.addGarbage(*base[who], from); } }
    int seSent = 0; vs.onSe = [&](int id) { if (id == 0x6D) seSent++; };
    int f = 0; for (; f < frames; f++) { for (auto& p : PL) if (p->alive) p->g.update(1.0f / 60.0f); if (vs.update()) break; }
    std::vector<view::Telop> TL; view::Telop baseT; baseT.load(&R, "assets_src/");
    if (vs.over) { if (vsIa) game::vsIaEndTelop(TL, baseT, vs.draw, vs.winner == 0); else vs.playEndTelops(TL, baseT); }
    for (auto& t : TL) for (int i = 0; i < telopFrames; i++) t.update(1.0f / 60.0f);
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn);
    for (auto& p : PL) { game::VersusState::hud(*p); if (vsIa && p->slot == 1) p->V.setTextS("TextBox_comLV", std::to_string(cfg.aiLevel + 1)); p->V.draw(p->g, 1.0f / 60.0f); }
    for (auto& t : TL) t.draw();
    if (vs.over && vsIa && getenv("VS_RESULT")) {  // VSIA: RETRY (+ CONTINUE si gana J1) + QUIT
        msg::Catalog M; M.dir = "assets_src/mess/"; game::VsIaMenu VM; VM.open(baseT, &M, vs.winner == 0 && cfg.aiLevel + 1 < 15); VM.sel = std::min(VM.count - 1, atoi(getenv("VS_RESULT")) - 1); VM.apply(false); for (int f = 0; f < 30; f++) VM.update(1.0f / 60.0f); VM.draw();
    } else if (vs.over && getenv("VS_RESULT")) {  // menú de resultado CONTINUE / QUIT (como main_sdl.cpp: pause01 / pause04 del telop, el elegido iluminado)
        msg::Catalog M; M.dir = "assets_src/mess/"; int sel = atoi(getenv("VS_RESULT")) - 1; view::Telop BT[2];
        for (int i = 0; i < 2; i++) { char an[32]; snprintf(an, sizeof an, "pause0%d_anim0%d.brlan", i == 0 ? 1 : 4, i == sel ? 1 : 2); BT[i].share(baseT); BT[i].play(i == 0 ? "pause01.brlyt" : "pause04.brlyt", an, 0, i == 0 ? -80 : -130); if (BT[i].L) if (lyt::Pane* t = BT[i].L->find(i == 0 ? "text_pause01" : "text_pause04")) t->text = M.get(14, i == 0 ? 15 : 16); for (int f = 0; f < 30; f++) BT[i].update(1.0f / 60.0f); BT[i].draw(); }
    }
    SDL_SaveBMP(surf, argv[1]);
    printf("VS %d jugadores, %d frames: over=%d ganador=%d |", n, f, (int)vs.over, vs.winner);
    for (auto& p : PL) printf(" J%d estado %d lineas %d clearEvent %d (con lineas %d) vivo %d |", p->slot + 1, p->g.state, p->g.lines, p->cl.clears, p->cl.lineClears, (int)p->alive);
    printf(" | SE6D %d", seSent); for (auto& p : PL) printf(" | J%d equipo %d hand %d enviadas %d recibidas %d canceladas %d pendientes %d", p->slot + 1, p->team, p->handicap, p->sent, p->received, p->cancelled, (int)p->pendingGarbage.size());
    if (vs.over) printf(" | fin: empate=%d ganador=%d equipo=%d", (int)vs.draw, vs.winner, vs.winTeam);
    (void)verbose; printf("\n"); return 0;
}
