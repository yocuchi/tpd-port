// Prueba sin ventana de la detonacion/cadena: bombliss_test
#include "../game/bombliss.hpp"
#include <cstdio>
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
static void dump(game::BomblissMode& B) { for (int y = B.h - 9; y < B.h; y++) { for (int x = 0; x < B.w; x++) { int i = B.idx(x, y); putchar(!B.occ[i] ? '.' : (B.bf[i] & 4) ? 'B' : (B.bf[i] & 2) ? 'o' : '#'); } putchar('\n'); } }
int main() {
    auto buf = readFile("data/8044aa58.tpf2"); tpf::Rules rules = tpf::parse(buf.data(), buf.size());
    game::BomblissData D; D.load("data/bombliss.bin"); te::Game g; g.init(rules, 1); game::BomblissMode B; B.start(g, D, 10);
    std::fill(B.occ.begin(), B.occ.end(), 0); std::fill(B.bf.begin(), B.bf.end(), 0); std::fill(B.col.begin(), B.col.end(), 0);
    int H = B.h; auto put = [&](int x, int y, int bomb) { int i = B.idx(x, y); B.occ[i] = 1; B.col[i] = 12; B.bf[i] = bomb ? 2 : 0; };
    for (int y = H - 2; y < H; y++) for (int x = 0; x < 10; x++) put(x, y, 0);
    for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) B.bf[B.idx(4 + dx, H - 2 + dy)] = 2;  // 2x2 de bombas -> se unen
    put(0, H - 6, 1); put(1, H - 6, 0); put(2, H - 6, 0); put(9, H - 7, 0); put(9, H - 8, 0);
    B.combine(); printf("combinada(bf):%d %d\n", B.bf[B.idx(4, H - 2)], B.bf[B.idx(5, H - 1)]);
    printf("inicio:\n"); dump(B);
    int frames = 0; bool done = false; while (!done && frames < 3000) { done = B.tick(16.7f); frames++; }
    printf("tras %d ticks (%.1f s), cadena=%d:\n", frames, frames * 0.0167f, B.chain); dump(B);
    return 0;
}
