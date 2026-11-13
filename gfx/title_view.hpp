// Pantalla de título: title.arc (title_us.brlyt + title_us_in.brlan una vez, luego title_us_loop.brlan en bucle).
#pragma once
#include "render.hpp"
#include "brlan.hpp"
#include <memory>

namespace view {

class TitleView {
public:
    lyt::Renderer* R = nullptr; gx::Arc arc; std::unique_ptr<lyt::Layout> L; lyt::Anim in, loop;
    float frame = 0; bool inDone = false; bool ok = false;

    lyt::Pane* pressPic = nullptr; lyt::Pane* pressTxt = nullptr;
    // pressText: "PULSA " (mainmenu idx 1); sustituye la imagen "PRESS 2" del mando de Wii por texto con el botón A
    bool load(lyt::Renderer* r, const std::string& dir, const std::u16string& pressText = u"") {
        R = r; if (!arc.load(dir + "title.arc")) return false; R->loadArcTextures(arc);
        size_t len; const uint8_t* d = arc.get("title_us.brlyt", len); L = std::make_unique<lyt::Layout>(); if (!d || !L->parse(d, len)) return false;
        d = arc.get("title_us_in.brlan", len); if (d) in.parse(d, len);
        d = arc.get("title_us_loop.brlan", len); if (d) loop.parse(d, len);
        pressPic = L->find("pressA");
        if (pressPic && !pressText.empty()) {
            lyt::Pane* t = new lyt::Pane(); t->type = "txt1"; t->name = "pressTxt"; t->x = pressPic->x; t->y = pressPic->y; t->w = 400; t->h = 40; t->align = 4;
            t->font = 0; t->fsx = t->fsy = 30; t->text = pressText; memset(t->vcol, 255, sizeof t->vcol); memset(t->tcol, 255, sizeof t->tcol); t->alpha = pressPic->alpha;
            t->parent = pressPic->parent; if (t->parent) t->parent->children.push_back(t); L->panes.push_back(t); pressTxt = t; pressPic->flags &= ~1;
        }
        if (in.frameCount > 0) in.apply(*L, 0); ok = true; return true;
    }
    void skipIntro() { if (!inDone) { inDone = true; frame = 0; if (in.frameCount > 0) in.apply(*L, (float)in.frameCount - 1); } }
    void update(float dt) {
        frame += dt * 60.0f;
        if (!inDone) { if (in.frameCount > 0 && frame < in.frameCount - 1) { in.apply(*L, frame); return; } inDone = true; frame = 0; }
        if (loop.frameCount > 0) { while (frame >= loop.frameCount) frame -= loop.frameCount; loop.apply(*L, frame); }
    }
    void draw() { if (pressTxt && pressPic) { pressTxt->alpha = pressPic->alpha; pressPic->flags &= ~1; } if (L) R->draw(*L); }
};

} // namespace view
