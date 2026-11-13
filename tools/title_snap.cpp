// Captura la pantalla de título tras N frames: title_snap salida.bmp [frames]
#include "../gfx/title_view.hpp"
#include "../gfx/msg.hpp"
#include <fstream>
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    for (const char* f : {"tetrisfont00.brfna", "tetrisfont01.brfna", "tetrisfont02.brfna"}) { auto d = readFile(std::string("assets_src/") + f); R.loadFont(f, d.data(), d.size()); }
    msg::Catalog M; M.dir = "assets_src/mess/";
    view::TitleView T; if (!T.load(&R, "assets_src/", M.get(1, 1))) { fprintf(stderr, "no carga\n"); return 1; }
    int n = argc > 2 ? atoi(argv[2]) : 400; for (int i = 0; i < n; i++) T.update(1.0f / 60.0f);
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn); T.draw(); SDL_SaveBMP(surf, argv[1]); printf("escrito %s (intro %s)\n", argv[1], T.inDone ? "terminada" : "en curso");
    return 0;
}
