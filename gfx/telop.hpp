// Telops de partida (game/telop.arc + telop_lang.arc): START, GAME OVER, CLEARED!, FINISH… con su animación.
#pragma once
#include "render.hpp"
#include "brlan.hpp"
#include <memory>

namespace view {

class Telop {
public:
    lyt::Renderer* R = nullptr; gx::Arc arc, langArc; const Telop* shared = nullptr; float scale = 1; /* shared: otro Telop dueño de los arcs (varios telops a la vez, p. ej. uno por campo en VS) */ std::unique_ptr<lyt::Layout> L; lyt::Anim A; float frame = 0; bool active = false; bool loaded = false;
    bool load(lyt::Renderer* r, const std::string& dir) { R = r; loaded = arc.load(dir + "telop.arc"); if (loaded) R->loadArcTextures(arc); if (langArc.load(dir + "telop_lang.arc")) R->loadArcTextures(langArc); return loaded; }
    void share(const Telop& o) { shared = &o; R = o.R; loaded = o.loaded; }
    bool play(const std::string& brlyt, const std::string& brlan, float x = 0, float y = 0, float sc = 1) {
        if (!loaded) return false; const gx::Arc& arc = shared ? shared->arc : this->arc; const gx::Arc& langArc = shared ? shared->langArc : this->langArc; scale = sc;
        // Arc::get busca por final de ruta: "win.brlyt" casaría con "team_win.brlyt"; se prueba antes la ruta exacta ("/" + nombre).
        auto find = [&](const std::string& n, size_t& l) { const uint8_t* r = arc.get("/" + n, l); if (!r) r = langArc.get("/" + n, l); if (!r) r = arc.get(n, l); if (!r) r = langArc.get(n, l); return r; };
        size_t len; const uint8_t* d = find(brlyt, len);
        L = std::make_unique<lyt::Layout>(); if (!d || !L->parse(d, len)) { L.reset(); return false; }
        if (L->root) { L->root->x = x; L->root->y = y; L->root->sx = L->root->sy = sc; }
        d = find(brlan, len); A = lyt::Anim(); if (d) A.parse(d, len);
        frame = 0; active = true; if (A.frameCount > 0) A.apply(*L, 0); return true;
    }
    bool done() const { return !active || A.frameCount == 0 || frame >= A.frameCount - 1; }
    void stop() { active = false; L.reset(); }
    void update(float dt) { if (!active || !L) return; if (A.frameCount > 0) { frame += dt * 60.0f; if (A.loop) { while (frame >= A.frameCount) frame -= A.frameCount; } else if (frame > A.frameCount - 1) frame = (float)(A.frameCount - 1); A.apply(*L, frame); } }
    void draw() { if (active && L) R->draw(*L); }
};

} // namespace view
