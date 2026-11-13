// Captura de un modo tras N frames con entradas aleatorias: mode_snap salida.bmp modo [frames] [telop:anim@frame]
#include "../gfx/game_view.hpp"
#include "../gfx/telop.hpp"
#include "../gfx/msg.hpp"
#include "../game/modes.hpp"
#include "../game/shadow.hpp"
#include <fstream>
struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 7) == 0; ho = ((v >> 8) & 63) == 0; } }
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
    int mode = argc > 2 ? atoi(argv[2]) : 0; game::ModeSpec spec = game::modeSpec(mode);
    auto buf = readFile(std::string("data/") + spec.rules); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    V.setBackground(spec.bg); V.setFieldConfig(spec.cfgLayout, spec.fieldGroup, 1); V.setFrameAnimName(spec.hudAnim); V.drawGhost = spec.ghost;
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99); g.lineGoal = spec.lineGoal;
    int frames = argc > 3 ? atoi(argv[3]) : 900; for (int i = 0; i < frames && g.state == 0; i++) g.update(1.0f / 60.0f);
    char b[32]; auto setN = [&](const char* p, int v) { snprintf(b, sizeof b, "%d", v); R.setText(p, b); };
    setN("TextBox_lines_01", spec.kind == game::Kind::Sprint ? std::max(0, g.lineGoal - g.lines) : g.lines); setN("TextBox_level_01", g.level); setN("TextBox_score_00", g.visScore); setN("TextBox_hisco_00", 12345); setN("TextBox_score", g.visScore); setN("TextBox_hiscore", 12345);
    setN("TextBox_lines_00", g.lines); setN("TextBox_level_00", g.level); setN("TextBox_allclear", g.allClears); R.setText("TextBox_time", "01'23\"45"); R.setText("TextBox_time_01", "01'23\"45"); R.setText("TextBox_btime_02", "--'--\"--"); R.setText("TextBox_btime", "--'--\"--");
    view::Telop TL; TL.load(&R, "assets_src/");
    if (argc > 4) { std::string s = argv[4]; size_t c = s.find(':'), at = s.find('@'); std::string ly = s.substr(0, c), an = s.substr(c + 1, at - c - 1); float fr = atof(s.c_str() + at + 1); TL.play(ly, an); for (int i = 0; i < (int)fr; i++) TL.update(1.0f / 60.0f); }
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 1.0f); TL.draw(); SDL_SaveBMP(surf, argv[1]);
    printf("modo %d: lineas %d nivel %d score %d (script %d) combo %d b2b %d allclear %d estado %d\n", mode, g.lines, g.level, g.visScore, g.scoreVal, g.combo, g.b2bCount, g.allClears, g.state);
    return 0;
}
