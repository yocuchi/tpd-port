// Prueba del motor sin pantalla: caída rápida por pulsación. hard_test [fichero tpf2]
//  1) X mantenida 10 s → cae UNA pieza.  2) pulsos de 1 frame cada 20 frames → una pieza por pulso.
//  3) pulso de 1 frame justo tras fijar (durante el borrado/aparición) → la pieza siguiente cae (no se pierde la pulsación).
//  4) pulso de 1 frame 40 frames antes de la aparición siguiente (pieza cayendo sola, más de 15 frames de margen) → se consume en la pieza actual, no en la siguiente.
#include "../engine/engine.hpp"
#include <fstream>
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
struct In : te::Input { bool h = false; bool left() override { return false; } bool right() override { return false; } bool rotA() override { return false; } bool rotB() override { return false; } bool soft() override { return false; } bool hard() override { return h; } bool hold() override { return false; } };
int main(int argc, char** argv) {
    auto buf = readFile(argc > 1 ? argv[1] : "data/8044aa58.tpf2"); tpf::Rules R = tpf::parse(buf.data(), buf.size());
    int fails = 0;
    {   // 1: mantenida
        te::Game g; In in; g.in = &in; g.init(R, 5); for (int f = 0; f < 30; f++) g.update(1.0f / 60); int before = g.lockCount;
        in.h = true; for (int f = 0; f < 600; f++) g.update(1.0f / 60); in.h = false;
        int n = g.lockCount - before; printf("1) X mantenida 10 s: %d piezas fijadas (esperado 1 por la pulsación + las que caen por gravedad; con gravedad nivel 1 ~ 0-1)\n", n); if (n > 2) fails++;
    }
    {   // 2: pulsos cada 20 frames
        te::Game g; In in; g.in = &in; g.init(R, 5); for (int f = 0; f < 30; f++) g.update(1.0f / 60); int before = g.lockCount, pulses = 0;
        for (int f = 0; f < 400; f++) { in.h = (f % 20 == 0); if (in.h) pulses++; g.update(1.0f / 60); if (f % 20 == 10) for (auto& c : g.cells) c = te::Cell(); /* vacía el campo para no rebasar el techo */ }
        in.h = false; for (int f = 0; f < 30; f++) g.update(1.0f / 60);
        int n = g.lockCount - before; printf("2) %d pulsos de 1 frame cada 20 frames: %d piezas fijadas\n", pulses, n); if (n != pulses) fails++;
    }
    {   // 3: pulso justo tras fijar
        te::Game g; In in; g.in = &in; g.init(R, 5); for (int f = 0; f < 30; f++) g.update(1.0f / 60);
        in.h = true; g.update(1.0f / 60); in.h = false; int lc = g.lockCount;  // pieza 1 cae
        // ahora hay borrado/aparición: 1 frame suelto y pulso de 1 frame
        g.update(1.0f / 60); in.h = true; g.update(1.0f / 60); in.h = false; for (int f = 0; f < 20; f++) g.update(1.0f / 60);
        int n = g.lockCount - lc; printf("3) pulso 1 frame tras fijar: %d pieza(s) más fijadas en 21 frames (esperado 1)\n", n); if (n != 1) fails++;
    }
    {   // 4: pulso con la pieza cayendo sola → se consume en esa pieza; sin pulsación la siguiente no cae
        te::Game g; In in; g.in = &in; g.init(R, 5); for (int f = 0; f < 30; f++) g.update(1.0f / 60); int lc = g.lockCount;
        in.h = true; g.update(1.0f / 60); in.h = false; for (int f = 0; f < 60; f++) g.update(1.0f / 60);
        int n = g.lockCount - lc; printf("4) un pulso y 60 frames de espera: %d pieza(s) fijadas (esperado 1)\n", n); if (n != 1) fails++;
    }
    printf(fails ? "FALLOS: %d\n" : "OK\n", fails); return fails ? 1 : 0;
}
