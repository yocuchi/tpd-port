// Dibuja líneas de texto UTF-16 (códigos hex) con una fuente: text_snap salida.bmp fuente.brfna tam "e040 e041 ..." ["..."]
#include "../gfx/render.hpp"
#include <fstream>
#include <sstream>
static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn); R.screenW = 640; R.screenH = 480;
    auto d = readFile(argv[2]); std::string fname = argv[2]; fname = fname.substr(fname.find_last_of('/') + 1); R.loadFont(fname, d.data(), d.size());
    float size = atof(argv[3]);
    lyt::Layout L; L.fonts.push_back(fname); lyt::Pane* root = new lyt::Pane(); root->type = "pan1"; root->name = "root"; L.panes.push_back(root); L.root = root;
    for (int i = 4; i < argc; i++) {
        lyt::Pane* t = new lyt::Pane(); t->type = "txt1"; t->name = "t"; t->font = 0; t->fsx = t->fsy = size; t->w = 620; t->h = size + 4; t->x = 0; t->y = 220 - (i - 4) * (size + 6); t->align = 3;  // izquierda, centrado vertical
        memset(t->vcol, 255, sizeof t->vcol); memset(t->tcol, 255, sizeof t->tcol);
        std::istringstream ss(argv[i]); std::string tok; while (ss >> tok) t->text.push_back((char16_t)strtol(tok.c_str(), nullptr, 16));
        t->parent = root; root->children.push_back(t); L.panes.push_back(t);
    }
    SDL_SetRenderDrawColor(rn, 40, 40, 90, 255); SDL_RenderClear(rn); R.draw(L); SDL_SaveBMP(surf, argv[1]); printf("escrito %s\n", argv[1]);
    return 0;
}
