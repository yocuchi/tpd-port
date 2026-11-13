// Captura del Corredor de etapas tras N frames con entradas aleatorias: drop_snap salida.bmp [frames] [etapa] [sinfin 0/1] [opcion 0/1] [pin 0/1]
#include "../gfx/game_view.hpp"
#include "../gfx/msg.hpp"
#include "../game/modes.hpp"
#include "../game/drop.hpp"
#include <fstream>
struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; if (frame % 8 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = ((v >> 4) & 3) == 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return false; } bool hold() override { return false; }
};
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    msg::Catalog M; M.dir = "assets_src/mess/";
    view::GameView V; if (!V.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    game::DropData D; if (!D.load("data/drop.bin")) { fprintf(stderr, "drop.bin\n"); return 1; }
    game::ModeSpec spec = game::modeSpec(4);
    auto buf = readFile(std::string("data/") + spec.rules); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    int stage = argc > 3 ? atoi(argv[3]) : 0; bool endless = argc > 4 && atoi(argv[4]); int opt = argc > 5 ? atoi(argv[5]) : 0; bool pin = argc > 6 && atoi(argv[6]);  // pin: pieza clavada en y=34 con abajo pulsado (prueba de meta)
    V.setBackground(spec.bg); V.setFieldConfig(spec.cfgLayout, spec.fieldGroup, 1); V.setFrameAnimName(spec.hudAnim); V.drawGhost = spec.ghost;
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99);
    game::DropMode S; S.start(g, D, stage, endless, opt, 1234); V.setTitle(M.get(14, 0x2c + stage)); V.clipField = true;
    int frames = argc > 2 ? atoi(argv[2]) : 600;
    for (int i = 0; i < frames && !S.finished; i++) { if (pin) { if (!g.hasPiece) g.update(1.0f / 60.0f); if (g.hasPiece) g.piece.y = 34; } S.input(pin, false, false); S.update(1.0f / 60.0f); if (!pin) g.update(1.0f / 60.0f); }
    char b[32]; auto setN = [&](const char* p, int v) { snprintf(b, sizeof b, "%d", v); R.setText(p, b); };
    setN("TextBox_lines", std::min(endless ? 9999 : 400, S.rows)); setN("TextBox_bestline", 0); R.setText("TextBox_time", "00'10\"00"); R.setText("TextBox_btime", "--'--\"--");
    V.scrollY = S.scrollY(); V.goalBoundary = S.goalBoundary();
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 1.0f); SDL_SaveBMP(surf, argv[1]);
    printf("etapa %d param %d patron %d fila %d filas %d scrolled %d score %d estado %d cleared %d failed %d pieza %d\n", stage, S.param, S.pattern, S.patRow, S.rows, S.scrolled, g.visScore, g.state, S.cleared, S.failed, g.lockCount);
    return 0;
}
