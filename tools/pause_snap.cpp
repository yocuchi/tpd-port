// Captura de la pausa, el diálogo de salir, NEW RECORD, la ventana de resultados y el menú de resultado sobre una partida:
//   pause_snap salida.bmp escena [modo=0] [sel=0] [frames de anim=60] [victoria=1]
//   escena: pause | dialog | newrec | result (single_result) | menu (CONTINUE/RETRY/QUIT) | list (imprime el top-5 de un fichero de récords de prueba)
#include "../gfx/game_view.hpp"
#include "../gfx/telop.hpp"
#include "../gfx/msg.hpp"
#include "../gfx/pause.hpp"
#include "../gfx/result_window.hpp"
#include "../game/records.hpp"
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
    if (argc < 3) { fprintf(stderr, "uso: pause_snap salida.bmp pause|dialog|newrec|result|menu|list [modo] [sel] [frames] [victoria]\n"); return 1; }
    std::string what = argv[2];
    if (what == "list") {  // prueba del fichero de récords: escribe, vuelve a leer y compara
        game::Records a; a.load("/tmp/tp_records_test.bin"); a.m.clear(); int k = game::Records::key(0, 0);
        for (int s : {500, 9000, 100, 7000, 300, 8000, 50}) a.addScore(k, s); for (float t : {61.5f, 40.25f, 99.f, 50.f, 45.f, 41.f}) a.addTime(k, t); a.save();
        game::Records b; bool ok = b.load("/tmp/tp_records_test.bin"); const game::RecEntry& e = b.m[k];
        printf("carga %d  puntos:", ok); for (int i = 0; i < 5; i++) printf(" %d", e.score[i]); printf("  tiempos:"); for (int i = 0; i < 5; i++) printf(" %.2f", e.time[i]); printf("\nruta por defecto: %s\n", game::Records::defaultPath().c_str()); return 0;
    }
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32); SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    msg::Catalog M; M.dir = "assets_src/mess/";
    view::GameView V; if (!V.load(&R, "assets_src/")) { fprintf(stderr, "recursos\n"); return 1; }
    int mode = argc > 3 ? atoi(argv[3]) : 0, sel = argc > 4 ? atoi(argv[4]) : 0, fr = argc > 5 ? atoi(argv[5]) : 60; bool victory = argc > 6 ? atoi(argv[6]) : 1;
    game::ModeSpec spec = game::modeSpec(mode); auto buf = readFile(std::string("data/") + spec.rules); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    V.setBackground(spec.bg); V.setFieldConfig(spec.cfgLayout, spec.fieldGroup, 1); V.setFrameAnimName(spec.hudAnim); V.drawGhost = spec.ghost;
    te::Game g; RandomInput in; g.in = &in; g.init(rules, 99); g.lineGoal = spec.lineGoal; for (int i = 0; i < 600 && g.state == 0; i++) g.update(1.0f / 60.0f);
    char b[32]; auto setN = [&](const char* p, int v) { snprintf(b, sizeof b, "%d", v); R.setText(p, b); };
    setN("TextBox_lines_01", g.lines); setN("TextBox_level_01", g.level); setN("TextBox_score_00", g.visScore); setN("TextBox_hisco_00", 12345); R.setText("TextBox_time", "01'23\"45"); R.setText("TextBox_time_01", "01'23\"45");
    view::Telop TL; TL.load(&R, "assets_src/"); gx::Arc WIN; WIN.load("assets_src/window.arc");
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); V.draw(g, 1.0f);
    const float dt = 1.0f / 60.0f;
    if (what == "pause" || what == "dialog") {
        pausemenu::PauseMenu PM; PM.load(&R, &M, &TL.arc, &WIN); PM.ghostLocked = !spec.ghost; PM.ghost = spec.ghost; PM.bgm = true; PM.se = false; PM.open();
        menu::Input none; for (int i = 0; i < 40; i++) PM.update(dt, none);
        for (int i = 0; i < sel; i++) { menu::Input d; d.down = true; PM.update(dt, d); }
        if (what == "dialog") { menu::Input a; a.a = true; PM.sel = pausemenu::PauseMenu::R_QUIT; PM.update(dt, a); for (int i = 0; i < fr; i++) PM.update(dt, none); }
        for (int i = 0; i < 25; i++) PM.update(dt, none); PM.draw();
        printf("pausa: activa %d estado %d sel %d\n", PM.active, (int)PM.st, PM.sel);
    } else if (what == "newrec") {
        result::playNewRecord(TL, 12345, -1.0f); if (argc > 6 && atoi(argv[6]) == 2) result::playNewRecord(TL, -1, 83.45f); for (int i = 0; i < fr; i++) TL.update(dt); TL.draw();
    } else if (what == "result" || what == "menu") {
        if (what == "result") {
            result::Data d; d.kind = spec.kind; d.victory = victory; d.bestScore = 123456; d.score = g.visScore; d.bestTime = mode == 0 ? -1.0f : 95.5f; d.time = 101.25f;
            result::ResultWindow RW; RW.load(&R, &WIN); RW.open(result::makeRows(M, d)); for (int i = 0; i < fr; i++) RW.update(dt, false); RW.draw();
        } else {
            result::ResultMenu RM; RM.load(&R, &M, &TL.arc); RM.open(victory); menu::Input none; for (int i = 0; i < sel; i++) { menu::Input d; d.down = true; RM.update(dt, d); } for (int i = 0; i < fr; i++) RM.update(dt, none); RM.draw();
        }
    }
    SDL_SaveBMP(surf, argv[1]); return 0;
}
