// Juega N frames con entradas pseudoaleatorias y guarda la pantalla completa (recursos originales) en un BMP.
#include "../gfx/game_view.hpp"
#include <fstream>

struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 7) == 0; ho = ((v >> 8) & 63) == 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return ho; }
};

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    int W = 640, H = 480;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = (float)W; R.screenH = (float)H;
    view::GameView V; if (!V.load(&R, "assets_src/")) { fprintf(stderr, "no se cargaron los recursos\n"); return 1; }
    printf("campo: bloque %g*%g en (%g,%g), %d siguientes en %zu posiciones, hold %zu\n", V.pc.blockSize, V.pc.blockScale, V.pc.left, V.pc.top, V.pc.nextNum, V.pc.nextPos.size(), V.pc.holdPos.size());
    std::ifstream f("data/80466f08.tpf2", std::ios::binary); std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), {});
    tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99);
    int frames = argc > 2 ? atoi(argv[2]) : 900; for (int i = 0; i < frames && g.state == 0; i++) g.update(1.0f / 60.0f);
    V.setTexts(g, 123456);
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 0); SDL_SaveBMP(surf, argv[1]);
    printf("lineas %d nivel %d escrito %s\n", g.lines, g.level, argv[1]);
    return 0;
}
