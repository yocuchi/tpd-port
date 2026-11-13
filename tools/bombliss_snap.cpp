// Captura de Bombliss tras N frames con entradas aleatorias: bombliss_snap salida.bmp [frames] [fase]
#include "../gfx/game_view.hpp"
#include "../gfx/msg.hpp"
#include "../game/bombliss.hpp"
#include <fstream>
struct RandomInput : te::Input {
    unsigned s = 4242; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 3) == 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return false; }
};
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    view::GameView V; if (!V.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    auto buf = readFile("data/8044aa58.tpf2"); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    game::BomblissData D; if (!D.load("data/bombliss.bin")) { fprintf(stderr, "bombliss.bin\n"); return 1; }
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99);
    int ph = argc > 3 ? atoi(argv[3]) : 10; game::BomblissMode B; B.start(g, D, ph); V.extra = &B; V.setFrameAnimName("frame00a_23.brlan"); V.setBackground(15);
    if (argc > 4) {  // demo: dos filas completas con bombas sobre una fase; se captura en plena explosion
        for (int y = B.h - 2; y < B.h; y++) for (int x = 0; x < 10; x++) { int i = B.idx(x, y); B.occ[i] = 1; B.col[i] = 12; B.bf[i] = (x == 2 || x == 7) ? 2 : 0; }
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) { int i = B.idx(4 + dx, B.h - 4 + dy); B.occ[i] = 1; B.col[i] = 12; B.bf[i] = 2; }
        B.combine(); B.sync();
    }
    int frames = argc > 2 ? atoi(argv[2]) : 600; int maxChain = 0, waves = 0;
    for (int i = 0; i < frames && !B.finished; i++) { g.update(1.0f / 60.0f); B.update(1.0f / 60.0f); maxChain = std::max(maxChain, B.chain); }
    int occ = 0; for (auto o : B.occ) occ += o;
    char b[32]; snprintf(b, sizeof b, "%d", B.points()); R.setText("TextBox_point", b); snprintf(b, sizeof b, "%d", ph + 1); R.setText("TextBox_level", b); R.setText("TextBox_time", "00'00\"00"); R.setText("TextBox_hiscore", "0"); R.setText("TextBox_total", "0");
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 1.0f / 60.0f); SDL_SaveBMP(surf, argv[1]);
    printf("fase %d piezas %d puntos %d bloques %d cadena_max %d cleared %d failed %d finished %d estado %d\n", ph, g.lockCount, B.points(), occ, maxChain, (int)B.cleared, (int)B.failed, (int)B.finished, g.state);
    return 0;
}
