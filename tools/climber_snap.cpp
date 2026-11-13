// Captura del Escalador tras N frames con entradas aleatorias: climber_snap salida.bmp [frames] [etapa] [oxigeno 0/1]
#include "../gfx/climber_view.hpp"
#include "../gfx/msg.hpp"
#include "../game/modes.hpp"
#include <fstream>
struct RandomInput : te::Input {
    unsigned s = 777; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; hd = false; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = ((v >> 4) & 3) == 0; hd = ((v >> 5) & 15) == 0; ho = 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a && frame > 40; } bool rotB() override { return false; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return ho; }
};
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    view::GameView V; if (!V.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    game::ModeSpec spec = game::modeSpec(2); auto buf = readFile(std::string("data/") + spec.rules); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    V.setBackground(spec.bg); V.setFieldConfig(spec.cfgLayout, spec.fieldGroup, 1); V.setFrameAnimName(spec.hudAnim);
    game::ClimberData D; if (!D.load("data/climber.bin")) { fprintf(stderr, "climber.bin\n"); return 1; }
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99); int stage = argc > 3 ? atoi(argv[3]) : 4;
    game::ClimberMode M; M.start(g, D, stage, 4242, argc > 4 && atoi(argv[4]));
    view::ClimberView CV; if (!CV.load(&R, V, "assets_src/", &M)) fprintf(stderr, "sin climber.arc\n");
    if (argc > 5 && atoi(argv[5])) {  // prueba de meta: coloca al escalador arriba sobre un bloque
        for (auto& f : M.flags) if (f.state == 1) f.state = 0; M.cx = M.tx = M.fx = 3; M.cy = M.ty = M.fy = 21; for (int i = 0; i < 3; i++) g.cells[3 + (22 + i) * g.w].mino = 3; }
    int frames = argc > 2 ? atoi(argv[2]) : 900; int i = 0;
    for (; i < frames && !M.finished; i++) { if (M.state != game::CS_DEAD && !M.goalRunning) g.update(1.0f / 60.0f); if (argc > 5 && i == 0) {} M.update(1.0f / 60.0f); CV.update(1.0f / 60.0f); }
    printf("g.state %d phase %d lock %d piece y %.1f x %d has %d t %.2f\n", g.state, g.phase, g.lockCount, g.piece.y, g.piece.x, (int)g.hasPiece, g.gameTime);
    char b[32]; snprintf(b, sizeof b, "%d", stage + 1); R.setText("TextBox_stage", b);
    int ms = (int)(M.stageMs); snprintf(b, sizeof b, "%02d'%02d\"%02d", ms / 60000, (ms / 1000) % 60, (ms / 10) % 100); R.setText("TextBox_time_00", b); R.setText("TextBox_btime_01", "--'--\"--");
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 1.0f); SDL_SaveBMP(surf, argv[1]);
    printf("frames %d etapa %d estado %x pos (%d,%d) dest (%d,%d) banderas restantes %d todas %d altura %d meta %d fallo %d lineas %d\n", i, stage + 1, M.state, M.cx, M.cy, M.tx, M.ty, M.flagsLeft(), (int)M.allFlags, M.record, (int)M.cleared, (int)M.failed, g.lines);
    return 0;
}
