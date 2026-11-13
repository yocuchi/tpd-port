// Captura/prueba de los objetos de Tetris VS: items_snap salida.bmp <id|0|-1> [jugadores=2] [frames tras usarlo=40] [frames previos=240]
//   id 1..18: da el objeto al jugador 1, lo usa (SE no suena) y captura tras N frames (3 Disparar y 18 Bomba: además dispara/coloca; 9 Cambio: pulsa ←; 1 Humo: agita 3 veces)
//   id 0: muestra la celda-objeto (pieza activa y celda fijada) y las 3 ranuras llenas; id -1: soak de 20000 frames con 4 CPU forzando objetos (sin captura), imprime estadísticas.
#include "../gfx/item_view.hpp"
#include "../gfx/telop.hpp"
#include <fstream>
#include <memory>
struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; hd = false; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 15) == 0; ho = ((v >> 8) & 63) == 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return ho; }
};
struct SnapPlayer : game::PlayerBase { RandomInput in; };
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "uso: items_snap salida.bmp id [jugadores] [frames] [previos]\n"); return 1; }
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    int id = atoi(argv[2]); int n = argc > 3 ? atoi(argv[3]) : 2; if (n < 2) n = 2; if (n > 4) n = 4; int after = argc > 4 ? atoi(argv[4]) : 40, pre = argc > 5 ? atoi(argv[5]) : 240; bool soak = id == -1;
    view::GameRes res; if (!res.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    view::ItemView IV; if (!IV.load(&R, "assets_src/")) { fprintf(stderr, "item.arc\n"); return 1; } printf("effect.arc %d climber.arc %d\n", (int)IV.fxOk, (int)IV.climbOk);
    auto buf = readFile("data/8044aa58.tpf2"); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    game::VersusState vs; game::ItemSystem items; std::vector<std::unique_ptr<SnapPlayer>> PL; std::vector<game::PlayerBase*> base;
    for (int i = 0; i < n; i++) {
        auto p = std::make_unique<SnapPlayer>(); if (!p->V.initVS(&res, n, i, i == 0 ? 15 : -1, 3)) { fprintf(stderr, "marcos VS\n"); return 1; }
        p->in.s = 777 + 1000 * i; p->g.in = &p->in; p->g.holdEnabled = true; p->g.init(rules, 99 + i);
        base.push_back(p.get()); PL.push_back(std::move(p));
    }
    vs.start(base); items.start(base); for (int i = 0; i < n; i++) items.cpu[i] = soak;  // start() copia PlayerBase::cpu (J2 es CPU por defecto en VsConfig): en las capturas, a mano
    const float D = 1.0f / 60.0f; int clears = 0; int rowsKilled = 0;
    auto step = [&](int frames) { for (int f = 0; f < frames; f++) { items.update(D); for (auto& p : PL) if (p->alive) p->g.update(items.dt(p->slot, D)); vs.update(); } };
    auto render = [&]() { SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); for (auto& p : PL) { game::VersusState::hud(*p); p->V.draw(p->g, D); IV.draw(items, p->slot, p->V, D); } };
    if (soak) {
        // fuerza filas completas con celdas-objeto para ejercitar la generación, y entrega objetos cada 2 s
        for (int f = 0; f < 20000 && !vs.over; f++) {
            if (f % 120 == 0) for (int i = 0; i < n; i++) if (items.alive(i)) items.give(i, game::ITEM_INFO[(f / 120 + i) % 13].id);
            if (f % 600 == 0) for (int i = 0; i < n; i++) if (items.alive(i)) { te::Game& g = PL[i]->g; int y = g.h - 1; for (int x = 0; x < g.w - 1; x++) { te::Cell c; c.mino = 2 + x % 7; c.flags = x == 3 ? (game::CELL_ITEM | 1) : 1; g.cells[x + y * g.w] = c; } }
            step(1); render(); items.fx.clear();
        }
        printf("soak: over=%d ganador=%d |", (int)vs.over, vs.winner); for (int i = 0; i < n; i++) printf(" J%d got %d used %d estado %d garbage %d |", i + 1, items.S[i].got, items.S[i].used, PL[i]->g.state, items.garbageSent[i]); printf("\n"); return 0;
    }
    if (id == -2) {  // prueba de obtención real: fila inferior completa con 2 celdas-objeto + 1 bomba; al fijarse la siguiente pieza se borra y deben salir 2 objetos y explotar la bomba
        te::Game& g = PL[0]->g; int y = g.h - 1; for (int x = 0; x < g.w; x++) { te::Cell c; c.mino = 2 + x % 7; c.flags = 1 | (x == 2 || x == 6 ? game::CELL_ITEM : 0) | (x == 8 ? game::CELL_BOMB : 0); g.cells[x + y * g.w] = c; }
        for (int x = 7; x < 10; x++) { te::Cell c; c.mino = 5; c.flags = 1; g.cells[x + (y - 1) * g.w] = c; }
        int got0 = items.S[0].got; for (int f = 0; f < 2400 && g.lines == 0; f++) step(1); printf("lineas %d, objetos obtenidos %d, bloques en fila y-1 cols 7..9: %d %d %d\n", g.lines, items.S[0].got - got0, g.at(7, y - 1).mino ? 1 : 0, g.at(8, y - 1).mino ? 1 : 0, g.at(9, y - 1).mino ? 1 : 0);
        printf("ranuras %d %d %d\n", items.S[0].slot[0], items.S[0].slot[1], items.S[0].slot[2]); return 0;
    }
    step(pre);
    if (id == 0) {  // celdas-objeto: una fija en la fila inferior (flag), otra en la pieza activa; las 3 ranuras llenas
        te::Game& g = PL[0]->g; te::Cell c; c.mino = 4; c.flags = game::CELL_ITEM | 1; g.cells[5 + (g.h - 1) * g.w] = c; items.forceMark(0); for (int k : {4, 10, 18}) items.give(0, k); items.give(1, 2); items.give(1, 6);
        printf("pieza marcada: R%d C%d\n", items.S[0].markR, items.S[0].markC);
    } else {
        items.give(0, id); items.give(0, 6); items.give(0, 2);
        // campo del jugador 1 con algo de pila para ver el efecto sobre el campo (cambio, patear, bomba, escalador)
        te::Game& g1 = PL[1 % n]->g; for (int y = g1.h - 3; y < g1.h; y++) for (int x = 0; x < g1.w; x++) if ((x + y) % 3) { te::Cell c; c.mino = 2 + (x + y) % 7; c.flags = 1; g1.cells[x + y * g1.w] = c; }
        te::Game& g0 = PL[0]->g; for (int y = g0.h - 3; y < g0.h; y++) for (int x = 0; x < g0.w; x++) if ((x * 2 + y) % 4) { te::Cell c; c.mino = 2 + (x + y) % 7; c.flags = 1; g0.cells[x + y * g0.w] = c; }
        bool okUse = items.use(0); printf("uso id %d: %d\n", id, (int)okUse);
        if (id == 3) { step(10); for (int k = 0; k < 5; k++) { items.use(0); step(8); } }
        if (id == 18) { step(10); items.use(0); }
        if (id == 9) { step(5); items.input(0, false, false, true, false); }
        if (id == 1) { step(30); for (int k = 0; k < 3; k++) items.input(1, false, true, false, false); }
    }
    step(after); render(); SDL_SaveBMP(surf, argv[1]);
    for (int i = 0; i < n; i++) { auto& s = items.S[i]; printf("J%d slots %d %d %d | accel %.1f hide %.1f stop %.1f lock %.1f conf %.1f fix %.1f casc %.1f shot %.1f smoke %d climb %d\n", i + 1, s.slot[0], s.slot[1], s.slot[2], s.tAccel, s.tHide, s.tFreeze, s.tLock, s.tConfuse, s.tFix, s.tCascade, s.tShot, s.smoke, (int)s.clAlive); }
    return 0;
}
