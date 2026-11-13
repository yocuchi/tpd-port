// Simulación sin pantalla del motor con las reglas reales.
//   sim <tpf2> [frames]      juego con entradas pseudoaleatorias
//   sim <tpf2> --lineas      prueba guionizada: debe limpiar 2 líneas con una I vertical
#include "../engine/engine.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <cstdlib>

struct RandomInput : te::Input {
    unsigned s = 12345; int frame = 0; bool l = 0, r = 0, a = 0, b = 0, sf = 0, hd = 0, ho = 0;
    unsigned rnd() { s = s * 1103515245u + 12345u; return (s >> 16) & 0x7fff; }
    void poll() override { frame++; if (frame % 6 == 0) { unsigned v = rnd(); l = v & 1; r = (v >> 1) & 1; a = (v >> 2) & 1; b = (v >> 3) & 1; sf = (v >> 4) & 1; hd = ((v >> 5) & 7) == 0; ho = ((v >> 8) & 31) == 0; } }
    bool left() override { return l; } bool right() override { return r; } bool rotA() override { return a; } bool rotB() override { return b; }
    bool soft() override { return sf; } bool hard() override { return hd; } bool hold() override { return ho; }
};
// Guion: cada entrada dura N frames. 'A' rotar, 'R' derecha, 'H' hard drop, '.' nada
struct ScriptInput : te::Input {
    std::string seq; int frame = 0; int per = 4;
    char cur() { int i = frame / per; return i < (int)seq.size() ? seq[i] : '.'; }
    void poll() override { frame++; }
    bool left() override { return cur() == 'L'; } bool right() override { return cur() == 'R'; } bool rotA() override { return cur() == 'A'; } bool rotB() override { return false; }
    bool soft() override { return false; } bool hard() override { return cur() == 'H'; } bool hold() override { return false; }
};
struct LogClient : te::Client {
    int events = 0, lines = 0; std::vector<std::string> cls;
    void event(int c) override { events++; if (c == 1) lines++; }
    void clientLine(const std::string& raw) override { if (cls.size() < 12) cls.push_back(raw); }
    void win() override { printf("  WIN\n"); } void lose() override { printf("  LOSE\n"); } void levelUp(int l) override { printf("  nivel %d\n", l); }
};

static void dump(te::Game& g) {
    for (int y = g.h / 2 - 2; y < g.h; y++) { printf("  %c|", y < g.h / 2 ? ' ' : '#'); for (int x = 0; x < g.w; x++) { auto& c = g.at(x, y); putchar(c.mino ? '0' + (c.mino % 10) : '.'); } printf("|\n"); }
}

int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), {});
    tpf::Rules R = tpf::parse(buf.data(), buf.size());
    te::Game g; LogClient cl; g.cl = &cl;
    if (argc > 2 && !strcmp(argv[2], "--lineas")) {
        ScriptInput in; g.in = &in; g.init(R, 7);
        // dos filas inferiores llenas salvo la última columna
        for (int y = g.h - 2; y < g.h; y++) for (int x = 0; x < g.w - 1; x++) { g.cells[x + y * g.w].mino = 8; }
        g.queue.clear(); g.queue.push_back(1); g.queue.push_back(2); g.queue.push_back(2);  // I, O, O
        in.seq = "....A...R.R.R.R.R.R.R.R.....H......";
        for (int i = 0; i < 400 && g.state == 0; i++) g.update(1.0f / 60.0f);
        printf("lineas=%d score=%d fase=%d\n", g.lines, g.scoreVal, g.phase);
        for (auto& s : cl.cls) printf("  cliente: %s\n", s.c_str());
        dump(g);
        return g.lines == 2 ? 0 : 1;
    }
    RandomInput in; g.in = &in; g.init(R, 42);
    int frames = argc > 2 ? atoi(argv[2]) : 3600;
    for (int i = 0; i < frames && g.state == 0; i++) g.update(1.0f / 60.0f);
    printf("campo %dx%d fase %d nivel %d lineas %d score %d goal %d gain %d tiempo %.1f eventos %d estado %d\n", g.w, g.h, g.phase, g.level, g.lines, g.scoreVal, g.goal, g.gain, g.gameTime, cl.events, g.state);
    dump(g);
    return 0;
}
