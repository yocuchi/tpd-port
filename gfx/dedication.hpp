// Pantalla de dedicatoria al arrancar: texto (UTF-8, varias líneas) de assets/dedicatoria.txt, con fundido de entrada y salida.
#pragma once
#include "render.hpp"
#include <fstream>
#include <memory>

namespace view {

class Dedication {
public:
    lyt::Renderer* R = nullptr; std::unique_ptr<lyt::Layout> L; float t = 0; float duration = 7.0f; bool done = false; bool ok = false;
    static std::u16string utf8to16(const std::string& s) {
        std::u16string r; for (size_t i = 0; i < s.size();) { unsigned char c = s[i]; uint32_t cp; int n;
            if (c < 0x80) { cp = c; n = 1; } else if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; n = 2; } else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; n = 3; } else { cp = c & 0x07; n = 4; }
            for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3f); i += n; if (cp == '\r') continue; r.push_back(cp < 0x10000 ? (char16_t)cp : u'?'); }
        return r;
    }
    bool load(lyt::Renderer* r, const std::string& path, const std::string& font = "tetrisfont00.brfna") {
        R = r; std::ifstream f(path, std::ios::binary); if (!f) return false; std::string txt((std::istreambuf_iterator<char>(f)), {}); while (!txt.empty() && (txt.back() == '\n' || txt.back() == '\r')) txt.pop_back();
        L = std::make_unique<lyt::Layout>(); L->fonts.push_back(font);
        lyt::Pane* root = new lyt::Pane(); root->type = "pan1"; root->name = "root"; L->panes.push_back(root); L->root = root;
        lyt::Pane* p = new lyt::Pane(); p->type = "txt1"; p->name = "dedic"; p->font = 0; p->fsx = p->fsy = 26; p->lineSpace = 10; p->w = 600; p->h = 400; p->align = 4;
        memset(p->vcol, 255, sizeof p->vcol); memset(p->tcol, 255, sizeof p->tcol); p->text = utf8to16(txt); p->parent = root; root->children.push_back(p); L->panes.push_back(p);
        ok = true; return true;
    }
    void skip() { if (t < duration - 1.0f) t = duration - 1.0f; }
    void update(float dt) { t += dt; if (t >= duration) done = true; }
    void draw() {
        if (!ok || !L) return; float a = t < 1.0f ? t : t > duration - 1.0f ? (duration - t) : 1.0f; if (a < 0) a = 0;
        L->root->alpha = (uint8_t)(a * 255); R->draw(*L);
    }
};

} // namespace view
