// Renderiza layouts a un BMP (sin ventana) para inspeccionarlos.
//   lyt_view salida.bmp fuente.brfna arc1:layout1.brlyt [arc2:layout2.brlyt ...] [--show pane,pane] [--hide pane,pane] [--text pane=texto]
#include "../gfx/render.hpp"
#include "../gfx/brlan.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>

static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "uso\n"); return 1; }
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* rn = SDL_CreateSoftwareRenderer(surf);
    lyt::Renderer R(rn);
    auto font = readFile(argv[2]); std::string fname = argv[2]; fname = fname.substr(fname.find_last_of('/') + 1);
    if (!R.loadFont(fname, font.data(), font.size())) fprintf(stderr, "fuente no cargada\n");
    std::vector<std::unique_ptr<lyt::Layout>> layouts; std::vector<std::unique_ptr<gx::Arc>> arcs;
    std::vector<std::string> show, hide; std::vector<std::pair<std::string, std::string>> texts;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--show" || a == "--hide") { std::string list = argv[++i]; size_t p = 0; while (p <= list.size()) { size_t q = list.find(',', p); if (q == std::string::npos) q = list.size(); (a == "--show" ? show : hide).push_back(list.substr(p, q - p)); p = q + 1; } continue; }
        if (a == "--text") { std::string kv = argv[++i]; size_t e = kv.find('='); texts.push_back({kv.substr(0, e), kv.substr(e + 1)}); continue; }
        if (a == "--anim") { std::string spec = argv[++i]; float fr = 0; size_t at = spec.find('@'); if (at != std::string::npos) { fr = atof(spec.c_str() + at + 1); spec = spec.substr(0, at); }
            size_t len; const uint8_t* d = nullptr; for (auto& ar : arcs) { d = ar->get(spec, len); if (d) break; } if (!d) { fprintf(stderr, "anim %s no encontrada\n", spec.c_str()); return 1; }
            lyt::Anim A; if (!A.parse(d, len)) { fprintf(stderr, "parse anim\n"); return 1; } printf("anim %s: %d frames, %zu entradas\n", spec.c_str(), A.frameCount, A.entries.size());
            for (auto& L : layouts) A.apply(*L, fr); continue; }
        size_t c = a.find(':'); auto arc = std::make_unique<gx::Arc>(); if (!arc->load(a.substr(0, c))) { fprintf(stderr, "arc %s\n", a.c_str()); return 1; }
        R.loadArcTextures(*arc);
        size_t len; const uint8_t* d = arc->get(a.substr(c + 1), len); if (!d) { fprintf(stderr, "layout %s no encontrado\n", a.c_str()); return 1; }
        auto L = std::make_unique<lyt::Layout>(); if (!L->parse(d, len)) { fprintf(stderr, "parse %s\n", a.c_str()); return 1; }
        printf("%s: %zu paneles, %zu materiales, %zu texturas, %zu fuentes\n", a.c_str(), L->panes.size(), L->materials.size(), L->textures.size(), L->fonts.size());
        layouts.push_back(std::move(L)); arcs.push_back(std::move(arc));
    }
    for (auto& L : layouts) { for (auto& s : show) if (auto* p = L->find(s)) p->flags |= 1; for (auto& s : hide) if (auto* p = L->find(s)) p->flags &= ~1; }
    for (auto& t : texts) R.setText(t.first, t.second);
    SDL_SetRenderDrawColor(rn, 0, 0, 0, 255); SDL_RenderClear(rn);
    for (auto& L : layouts) R.draw(*L);
    SDL_SaveBMP(surf, argv[1]);
    printf("escrito %s\n", argv[1]);
    return 0;
}
