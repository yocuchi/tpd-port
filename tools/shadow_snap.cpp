// Captura del modo Sombra tras N frames con entradas aleatorias: shadow_snap salida.bmp [frames] [etapa]
#include "../gfx/game_view.hpp"
#include "../gfx/msg.hpp"
#include "../game/shadow.hpp"
#include <fstream>
struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 7) == 0; ho = 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return ho; }
};
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    msg::Catalog M; M.dir = "assets_src/mess/";
    view::GameView V; if (!V.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    auto buf = readFile("data/80466f08.tpf2"); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    game::ShadowData D; if (!D.load("data/shadow.bin")) { fprintf(stderr, "shadow.bin\n"); return 1; }
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99);
    int stage = argc > 3 ? atoi(argv[3]) : 0; game::ShadowMode S; S.start(g, D, stage); g.pieceBudget = S.maxPieces; V.setFrameAnim(12); V.setTitle(M.get(14, 0x3a + S.silhouette));
    int frames = argc > 2 ? atoi(argv[2]) : 600; for (int i = 0; i < frames && !S.finished; i++) { g.update(1.0f / 60.0f); S.update(1.0f / 60.0f); }
    char b[32]; snprintf(b, sizeof b, "%d", S.remaining()); R.setText("TextBox_skip", b); snprintf(b, sizeof b, "%d%%", S.percentShown()); R.setText("TextBox_match", b); int t = (int)ceilf(S.timeLeft()); snprintf(b, sizeof b, "%02d:%02d", t / 60, t % 60); R.setText("TextBox_time", b);
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 1.0f); SDL_SaveBMP(surf, argv[1]);
    printf("etapa %d silueta %d piezas %d/%d pct %d total %d dentro %d fuera %d estado %d terminado %d\n", stage, S.silhouette, g.lockCount, S.maxPieces, S.percentShown(), S.total, S.inside, S.outside, g.state, (int)S.finished);
    return 0;
}
