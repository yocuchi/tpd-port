// Renderizador de layouts NW4R sobre SDL2 (SDL_RenderGeometry): paneles, imágenes, ventanas y texto.
#pragma once
#include <SDL2/SDL.h>
#include <cmath>
#include <map>
#include <memory>
#include "brlyt.hpp"
#include "brfnt.hpp"
#include "u8.hpp"

namespace lyt {

struct Mat { float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;  // [a c tx; b d ty]
    Mat mul(const Mat& m) const { Mat r; r.a = a * m.a + c * m.b; r.b = b * m.a + d * m.b; r.c = a * m.c + c * m.d; r.d = b * m.c + d * m.d; r.tx = a * m.tx + c * m.ty + tx; r.ty = b * m.tx + d * m.ty + ty; return r; }
    void apply(float x, float y, float& ox, float& oy) const { ox = a * x + c * y + tx; oy = b * x + d * y + ty; }
    static Mat trs(float x, float y, float rz, float sx, float sy) { Mat m; float r = rz * 3.14159265f / 180.0f, cs = cosf(r), sn = sinf(r); m.a = cs * sx; m.b = sn * sx; m.c = -sn * sy; m.d = cs * sy; m.tx = x; m.ty = y; return m; }
};

class Renderer {
public:
    SDL_Renderer* rn = nullptr;
    float screenW = 640, screenH = 480;   // tamaño destino en píxeles
    float baseW = 640;                     // ancho del área base (640 = 4:3; VS 6 usa 480*ancho/alto de la pantalla para ocupar el 16:9)
    std::map<std::string, SDL_Texture*> textures;
    std::map<std::string, std::vector<SDL_Texture*>> fontSheets;
    std::map<std::string, std::shared_ptr<Font>> fonts;

    explicit Renderer(SDL_Renderer* r) : rn(r) {}

    int texFailures = 0;
    SDL_Texture* makeTexture(const gx::Image& im) {
        SDL_Texture* t = SDL_CreateTexture(rn, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im.w, im.h);
        if (!t) { fprintf(stderr, "[tetris] textura %dx%d fallo: %s\n", im.w, im.h, SDL_GetError()); texFailures++; return nullptr; }
        SDL_UpdateTexture(t, nullptr, im.rgba.data(), im.w * 4); SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        if (im.magFilter) SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);  // como la Wii (GX_LINEAR): los degradados diminutos estirados (cielo de bg_15: 5x20) salen suaves y no a escalones
        return t;
    }
    // Carga todas las texturas timg/*.tpl de un arc con el nombre de fichero como clave.
    void loadArcTextures(const gx::Arc& arc) {
        for (auto& kv : arc.files) {
            const std::string& k = kv.first; if (k.size() < 4 || k.compare(k.size() - 4, 4, ".tpl")) continue;
            std::string name = k.substr(k.find_last_of('/') + 1); if (textures.count(name)) continue;
            gx::Image im; if (gx::loadTPL(&arc.data[kv.second.first], kv.second.second, im)) textures[name] = makeTexture(im);
        }
    }
    void addTexture(const std::string& name, const gx::Image& im) { textures[name] = makeTexture(im); }
    // Texturas por arc (varios arcs tienen ficheros con el mismo nombre y distinto contenido): el layout queda ligado a las de su arc.
    std::map<const gx::Arc*, std::map<std::string, SDL_Texture*>> arcTextures;
    void bindTextures(Layout& L, const gx::Arc& arc) {
        auto& m = arcTextures[&arc];
        if (m.empty()) for (auto& kv : arc.files) { const std::string& k = kv.first; if (k.size() < 4 || k.compare(k.size() - 4, 4, ".tpl")) continue; std::string name = k.substr(k.find_last_of('/') + 1); gx::Image im; if (gx::loadTPL(&arc.data[kv.second.first], kv.second.second, im)) m[name] = makeTexture(im); }
        L.boundTex.assign(L.textures.size(), nullptr); for (size_t i = 0; i < L.textures.size(); i++) { auto it = m.find(L.textures[i]); if (it != m.end()) L.boundTex[i] = it->second; }
    }
    bool loadFont(const std::string& name, const uint8_t* d, size_t n) {
        auto f = std::make_shared<Font>(); if (!f->parse(d, n)) return false;
        fonts[name] = f; auto& v = fontSheets[name]; for (auto& im : f->sheets) v.push_back(makeTexture(im));
        return true;
    }

    // ---- dibujo ----
    void draw(const Layout& L) { if (L.root) drawPane(L, L.root, Mat(), 1.0f); }

    void drawPane(const Layout& L, const Pane* p, const Mat& parent, float alpha) {
        if (!p->visible()) return;
        float a = alpha * (p->alpha / 255.0f);
        Mat m = parent.mul(Mat::trs(p->x, p->y, p->rz, p->sx, p->sy));
        // rectángulo según origen (coordenadas y hacia arriba)
        float ox = -(p->origin % 3) * p->w / 2.0f, oy = (p->origin / 3) * p->h / 2.0f;
        float x0 = ox, x1 = ox + p->w, y1 = oy, y0 = oy - p->h;   // y0 abajo, y1 arriba
        if (p->type == "pic1") drawPic(L, p, m, x0, y0, x1, y1, a);
        else if (p->type == "wnd1") drawWindow(L, p, m, x0, y0, x1, y1, a);
        else if (p->type == "txt1") drawText(L, p, m, x0, y0, x1, y1, a);
        for (const Pane* c : p->children) drawPane(L, c, m, (p->flags & 2) ? a : alpha * 1.0f);
    }

    void quad(SDL_Texture* tex, const Mat& m, float x0, float y0, float x1, float y1, const float* uv /*tl tr bl br*/, const uint8_t col[4][4], float alpha) {
        SDL_Vertex v[4]; float xs[4] = {x0, x1, x0, x1}, ys[4] = {y1, y1, y0, y0};
        for (int i = 0; i < 4; i++) {
            float sx, sy; m.apply(xs[i], ys[i], sx, sy);
            v[i].position.x = (sx + baseW / 2) * screenW / baseW; v[i].position.y = (240.0f - sy) * screenH / 480.0f;
            v[i].tex_coord.x = uv ? uv[i * 2] : 0; v[i].tex_coord.y = uv ? uv[i * 2 + 1] : 0;
            v[i].color.r = col ? col[i][0] : 255; v[i].color.g = col ? col[i][1] : 255; v[i].color.b = col ? col[i][2] : 255; v[i].color.a = (Uint8)((col ? col[i][3] : 255) * alpha);
        }
        int idx[6] = {0, 1, 2, 1, 3, 2};
        SDL_RenderGeometry(rn, tex, v, 4, idx, 6);
    }
    SDL_Texture* materialTexture(const Layout& L, int mi) {
        if (mi < 0 || mi >= (int)L.materials.size()) return nullptr; const Material& M = L.materials[mi]; if (M.texmaps.empty()) return nullptr;
        int ti = M.texmaps[0].tex; if (ti >= (int)L.textures.size()) return nullptr;
        if (ti < (int)L.boundTex.size() && L.boundTex[ti]) return (SDL_Texture*)L.boundTex[ti];
        auto it = textures.find(L.textures[ti]); return it == textures.end() ? nullptr : it->second;
    }
    void drawPic(const Layout& L, const Pane* p, const Mat& m, float x0, float y0, float x1, float y1, float a) {
        SDL_Texture* t = materialTexture(L, p->material);
        float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1}; if (!p->uv.empty()) for (int i = 0; i < 8; i++) uv[i] = p->uv[0][i];
        if (p->material >= 0 && p->material < (int)L.materials.size()) applyTexSRT(L.materials[p->material], uv);
        uint8_t col[4][4]; memcpy(col, p->vcol, 16);
        if (p->material >= 0 && p->material < (int)L.materials.size() && !t) { const Material& M = L.materials[p->material]; if (!M.texmaps.empty()) return; /* material con textura que no se pudo cargar: no pintar un bloque opaco */ for (int i = 0; i < 4; i++) { col[i][0] = col[i][0] * clamp16(M.back[0]) / 255; col[i][1] = col[i][1] * clamp16(M.back[1]) / 255; col[i][2] = col[i][2] * clamp16(M.back[2]) / 255; col[i][3] = col[i][3] * clamp16(M.back[3]) / 255; } }
        quad(t, m, x0, y0, x1, y1, uv, col, a);
    }
    static int clamp16(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
    static void applyTexSRT(const Material& M, float* uv) {
        if (M.texsrt.empty()) return; const TexSRT& s = M.texsrt[0];
        float r = s.rot * 3.14159265f / 180.0f, cs = cosf(r), sn = sinf(r);
        for (int i = 0; i < 4; i++) { float u = uv[i * 2] - 0.5f, v = uv[i * 2 + 1] - 0.5f; float u2 = (cs * u - sn * v) * s.sx, v2 = (sn * u + cs * v) * s.sy; uv[i * 2] = u2 + 0.5f + s.tx; uv[i * 2 + 1] = v2 + 0.5f + s.ty; }
    }
    void drawWindow(const Layout& L, const Pane* p, const Mat& m, float x0, float y0, float x1, float y1, float a) {
        // contenido
        float cx0 = x0 + p->inset[0], cx1 = x1 - p->inset[1], cy1 = y1 - p->inset[2], cy0 = y0 + p->inset[3];
        SDL_Texture* t = materialTexture(L, p->material);
        float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1}; if (!p->uv.empty()) for (int i = 0; i < 8; i++) uv[i] = p->uv[0][i];
        quad(t, m, cx0, cy0, cx1, cy1, uv, p->vcol, a);
        // marcos: 1 (esquina repetida con volteo) o 4/8
        size_t nf = p->frames.size(); if (!nf) return;
        uint8_t white[4][4]; memset(white, 255, sizeof white);
        auto texSize = [&](int mi, int& w, int& h) { SDL_Texture* tx = materialTexture(L, mi); w = h = 0; if (tx) SDL_QueryTexture(tx, nullptr, nullptr, &w, &h); return tx; };
        if (nf == 1) {
            int w, h; SDL_Texture* tx = texSize(p->frames[0].first, w, h); if (!tx) return;
            float fw = (float)w, fh = (float)h;
            float tl[8] = {0, 0, 1, 0, 0, 1, 1, 1}, tr[8] = {1, 0, 0, 0, 1, 1, 0, 1}, bl[8] = {0, 1, 1, 1, 0, 0, 1, 0}, br[8] = {1, 1, 0, 1, 1, 0, 0, 0};
            quad(tx, m, x0, y1 - fh, x0 + fw, y1, tl, white, a); quad(tx, m, x1 - fw, y1 - fh, x1, y1, tr, white, a);
            quad(tx, m, x0, y0, x0 + fw, y0 + fh, bl, white, a); quad(tx, m, x1 - fw, y0, x1, y0 + fh, br, white, a);
            // bordes estirados a partir de la columna/fila exterior de la esquina
            float top[8] = {0.99f, 0, 0.99f, 0, 0.99f, 1, 0.99f, 1}; quad(tx, m, x0 + fw, y1 - fh, x1 - fw, y1, top, white, a);
            float bot[8] = {0.99f, 1, 0.99f, 1, 0.99f, 0, 0.99f, 0}; quad(tx, m, x0 + fw, y0, x1 - fw, y0 + fh, bot, white, a);
            float lef[8] = {0, 0.99f, 1, 0.99f, 0, 0.99f, 1, 0.99f}; quad(tx, m, x0, y0 + fh, x0 + fw, y1 - fh, lef, white, a);
            float rig[8] = {1, 0.99f, 0, 0.99f, 1, 0.99f, 0, 0.99f}; quad(tx, m, x1 - fw, y0 + fh, x1, y1 - fh, rig, white, a);
        } else {
            // 4 marcos: tl, tr, bl, br ; 8: + t, b, l, r
            int w, h; for (size_t i = 0; i < nf && i < 8; i++) {
                SDL_Texture* tx = texSize(p->frames[i].first, w, h); if (!tx) continue; float fw = (float)w, fh = (float)h; float u[8] = {0, 0, 1, 0, 0, 1, 1, 1};
                switch (i) { case 0: quad(tx, m, x0, y1 - fh, x0 + fw, y1, u, white, a); break; case 1: quad(tx, m, x1 - fw, y1 - fh, x1, y1, u, white, a); break;
                    case 2: quad(tx, m, x0, y0, x0 + fw, y0 + fh, u, white, a); break; case 3: quad(tx, m, x1 - fw, y0, x1, y0 + fh, u, white, a); break;
                    case 4: quad(tx, m, x0 + fw, y1 - fh, x1 - fw, y1, u, white, a); break; case 5: quad(tx, m, x0 + fw, y0, x1 - fw, y0 + fh, u, white, a); break;
                    case 6: quad(tx, m, x0, y0 + fh, x0 + fw, y1 - fh, u, white, a); break; case 7: quad(tx, m, x1 - fw, y0 + fh, x1, y1 - fh, u, white, a); break; }
            }
        }
    }
    void drawText(const Layout& L, const Pane* p, const Mat& m, float x0, float y0, float x1, float y1, float a) {
        std::u16string txt = p->text; auto ov = overrides.find(p->name); if (ov != overrides.end()) txt = ov->second;
        if (txt.empty()) return;
        std::string fname = (p->font >= 0 && p->font < (int)L.fonts.size()) ? L.fonts[p->font] : (fonts.empty() ? "" : fonts.begin()->first);
        auto fit = fonts.find(fname); if (fit == fonts.end()) { if (fonts.empty()) return; fit = fonts.begin(); fname = fit->first; }
        const Font& F = *fit->second; auto& sheets = fontSheets[fname];
        float sy = p->fsy > 0 ? p->fsy / (float)F.height : 1.0f, sx = p->fsx > 0 ? p->fsx / (float)F.height : sy;
        // líneas
        std::vector<std::u16string> lines(1); for (char16_t c : txt) { if (c == u'\n') lines.emplace_back(); else lines.back().push_back(c); }
        int halign = p->align % 3, valign = (p->align / 3) % 3;
        float lineH = F.height * sy + p->lineSpace; float blockH = lineH * lines.size() - p->lineSpace;
        float top = (valign == 0) ? y1 : (valign == 1) ? (y0 + y1) / 2 + blockH / 2 : y0 + blockH;
        uint8_t col[4][4]; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) col[i][j] = p->tcol[i < 2 ? 0 : 1][j];
        for (size_t li = 0; li < lines.size(); li++) {
            const std::u16string& ln = lines[li];
            float total = 0; for (char16_t c : ln) { int g = F.glyph(c); Font::Width w = F.width(g); total += w.adv * sx + p->charSpace; }
            float cx = (halign == 0) ? x0 : (halign == 1) ? (x0 + x1) / 2 - total / 2 : x1 - total;
            float cy = top - li * lineH;
            for (char16_t c : ln) {
                int g = F.glyph(c); Font::Width w = F.width(g); int sh, gx, gy; F.rect(g, sh, gx, gy);
                if (sh < (int)sheets.size()) {
                    float u0 = (float)gx / F.sheetW, v0 = (float)gy / F.sheetH, u1 = (float)(gx + F.cellW) / F.sheetW, v1 = (float)(gy + F.cellH) / F.sheetH;
                    float uv[8] = {u0, v0, u1, v0, u0, v1, u1, v1};
                    float gx0 = cx + w.left * sx;
                    quad(sheets[sh], m, gx0, cy - F.cellH * sy, gx0 + F.cellW * sx, cy, uv, col, a);
                }
                cx += (w.adv * sx) + p->charSpace;
            }
        }
    }
    void setText16(Layout& L, const std::string& pane, const std::u16string& s) { if (Pane* p = L.find(pane)) p->text = s; }
    std::map<std::string, std::u16string> overrides;  // texto por nombre de TextBox
    void setText(const std::string& pane, const std::string& s) { std::u16string u; for (char c : s) u.push_back((char16_t)(unsigned char)c); overrides[pane] = u; }
};

} // namespace lyt
