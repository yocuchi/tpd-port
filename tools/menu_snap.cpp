// Simula el menú y captura: menu_snap salida.bmp "secuencia"  (secuencia: letras separadas por espacios: w=esperar 60 frames, W=esperar 10, u/d/l/r/a/b/x=botón X, Ajustes)
#include "../gfx/menu.hpp"
#include <fstream>
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    msg::Catalog M; M.dir = "assets_src/mess/";
    menu::MenuSystem S; if (!S.load(&R, &M, "assets_src/")) { fprintf(stderr, "no carga\n"); return 1; }
    game::Settings AJ; S.cfg = &AJ;  // AJUSTES: ajustes en memoria (no toca el fichero del usuario)
    S.open();
    std::string seq = argc > 2 ? argv[2] : "w";
    for (char c : seq) {
        menu::Input in; int wait = 0;
        if (c == 'w') wait = 60; else if (c == 'W') wait = 10; else if (c == 'u') in.up = true; else if (c == 'd') in.down = true; else if (c == 'l') in.left = true; else if (c == 'r') in.right = true; else if (c == 'a') in.a = true; else if (c == 'b') in.b = true; else if (c == 'x') in.x = true; else continue;
        S.update(1.0f / 60.0f, in); for (int i = 0; i < wait; i++) S.update(1.0f / 60.0f, menu::Input());
    }
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); S.draw(); SDL_SaveBMP(surf, argv[1]);
    printf("escrito %s (pantalla %d fase %d sel %d startMode %d)\n", argv[1], (int)S.screen, (int)S.phase, S.sel, S.startMode);
    return 0;
}
