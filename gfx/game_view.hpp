// Pantalla de juego con los recursos originales: fondo (bg_NN), marco (frame00a + anim por modo), frame00b (fondo del campo),
// bloques (blocks00.brres), siguientes/hold (blocks10) y textos, colocados según FieldInfo (config_*.brlyt).
#pragma once
#include "render.hpp"
#include "brlan.hpp"
#include "../engine/engine.hpp"
#include <memory>
#include <functional>
#include <fstream>
#include <cstdio>

namespace view {

struct PlayerCfg {
    float blockSize = 16, blockScale = 1, left = 248, top = 116, centerX = 320, centerY = 268, picX = 320, picY = 240; int nextNum = 5, holdNum = 1, penaltyNum = 0, itemNum = 0;
    std::vector<std::pair<float, float>> nextPos, holdPos, penaltyPos, itemPos; std::vector<float> nextScale, holdScale;
};

// Recursos pesados compartidos entre todas las vistas de jugador (arcs, atlas de bloques, texturas): se cargan una sola vez.
struct GameRes {
    lyt::Renderer* R = nullptr; std::string dir;
    gx::Arc frameArc, frame2Arc, frame3Arc, cfgArc;  // frame00_01 (1 jugador), frame00_02 (2 jugadores), frame01_01 (3-4 jugadores), config_01
    std::vector<uint8_t> blocksBrres; SDL_Texture* atlas = nullptr; SDL_Texture* previews = nullptr; int atlasW = 512, atlasH = 512;
    SDL_Rect prevRect[12] = {};  // recuadro real (sin transparencia) de cada tetrimino en blocks10, por id (I=1,O=2,Z=3,S=4,L=5,J=6,T=7; 8-11 piezas de principiantes)
    void computePreviewRects(const gx::Image& im) {
        auto bbox = [&](int cx, int cy, int cw, int ch) { int x0 = cx + cw, y0 = cy + ch, x1 = cx - 1, y1 = cy - 1; for (int y = cy; y < cy + ch && y < im.h; y++) for (int x = cx; x < cx + cw && x < im.w; x++) if (im.rgba[(y * im.w + x) * 4 + 3] > 32) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; } SDL_Rect r = {cx, cy, cw, ch}; if (x1 >= x0) r = {x0, y0, x1 - x0 + 1, y1 - y0 + 1}; return r; };
        const int Y = 24, C = 64;
        prevRect[7] = bbox(0, Y + 0 * C, C, C); prevRect[5] = bbox(0, Y + 1 * C, C, C); prevRect[6] = bbox(0, Y + 2 * C, C, C);
        prevRect[1] = bbox(0, Y + 3 * C, C, C); prevRect[3] = bbox(2 * C, Y + 3 * C, C, C); prevRect[4] = bbox(0, Y + 4 * C, C, C); prevRect[2] = bbox(2 * C, Y + 4 * C, C, C);
        for (int i = 8; i < 12; i++) prevRect[i] = bbox((i - 8) * C, 0, C, 24);
        prevRect[0] = prevRect[7];
    }
    bool hasVs2 = false, hasVs4 = false, ok = false;
    static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
    bool load(lyt::Renderer* r, const std::string& assetDir) {
        R = r; dir = assetDir;
        if (!frameArc.load(dir + "frame00_01.arc") || !cfgArc.load(dir + "config_01.arc")) return false;
        R->loadArcTextures(frameArc);
        hasVs2 = frame2Arc.load(dir + "frame00_02.arc"); if (hasVs2) R->loadArcTextures(frame2Arc);
        hasVs4 = frame3Arc.load(dir + "frame01_01.arc"); if (hasVs4) R->loadArcTextures(frame3Arc);
        auto font = readFile(dir + "tetrisfont02.brfna"); R->loadFont("tetrisfont02.brfna", font.data(), font.size());
        blocksBrres = readFile(dir + "blocks00.brres");
        gx::Image im; if (gx::loadTEX0(blocksBrres.data(), blocksBrres.size(), "blocks00", im)) { atlas = R->makeTexture(im); atlasW = im.w; atlasH = im.h; }
        if (gx::loadTEX0(blocksBrres.data(), blocksBrres.size(), "blocks10", im)) { previews = R->makeTexture(im); computePreviewRects(im); }
        ok = atlas != nullptr; return ok;
    }
};

class GameView;
// Gancho de dibujo de modos con campo propio (Bombliss): sustituye el dibujo de bloques fijados y adorna la pieza activa.
struct FieldExtra {
    virtual ~FieldExtra() {}
    virtual void drawField(GameView& v, float fx, float fy, float bs, int top, float dt) = 0;
    virtual uint32_t pieceMino(uint32_t m) { return m; }
    virtual bool pieceBomb(const te::Game& g, int i, int j) { return false; }  // celda (i,j) del grid 7x7 de la pieza activa lleva bomba
};

class GameView {
public:
    FieldExtra* extra = nullptr;
    std::function<void(GameView&)> drawExtra, drawTop;  // ganchos: drawExtra (sobre bloques, bajo el marco), drawTop (sobre el marco): Escalador
    float curFx = 0, curFy = 0, curBs = 16; int curTop = 20;  // geometría del campo del último draw (centro de celda = fx+bs/2)
    lyt::Renderer* R = nullptr;
    GameRes* res = nullptr; std::unique_ptr<GameRes> ownRes;  // recursos compartidos (atlas, arcs); ownRes solo en el modo de un jugador autónomo (load)
    gx::Arc bgArc;
    std::unique_ptr<lyt::Layout> frame, bg, cfg, frameB, titleLyt; lyt::Anim frameAnim, bgAnim, frameBAnim, titleAnim; float bgFrame = 0, titleFrame = 0; bool showTitle = false;
    SDL_Texture* atlas = nullptr; SDL_Texture* previews = nullptr; int atlasW = 512, atlasH = 512;  // copias de los punteros de res
    PlayerCfg pc; std::string dir; int bgIndex = -1; bool drawGhost = true;
    bool drawBg = true;                    // en partidas de varios jugadores solo la vista 0 dibuja el fondo
    bool placeFrame = false;               // marcos VS: el marco "a" se coloca en (PictureCenterX-320, 240-PictureCenterY)
    float fbBaseW = 160, fbBaseH = 320;    // tamaño del fondo negro del campo (frame00b 160x320, frame01b 120x240) para escalarlo al campo
    bool ok = false;
    // Corredor de etapas: desplazamiento del campo (celdas), línea de meta frame00f (frontera visible 0..20, <0 oculta)
    float scrollY = 0, goalBoundary = -1; bool clipField = false; std::unique_ptr<lyt::Layout> goalLyt; lyt::Anim goalAnim; float goalFrame = 0;

    static std::vector<uint8_t> readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }

    // Modo autónomo de un jugador: carga sus propios recursos (herramientas de captura y modos de 1 jugador sin Player).
    bool load(lyt::Renderer* r, const std::string& assetDir, const std::string& mode = "Single", int player = 1, int bgIdx = 0, int frameAnimIndex = 0) {
        ownRes = std::make_unique<GameRes>(); if (!ownRes->load(r, assetDir)) return false;
        return init(ownRes.get(), mode, player, bgIdx, frameAnimIndex);
    }
    void attachRes(GameRes* r) { res = r; R = r->R; dir = r->dir; atlas = r->atlas; previews = r->previews; atlasW = r->atlasW; atlasH = r->atlasH; }
    std::unique_ptr<lyt::Layout> mkLayout(const gx::Arc& a, const std::string& n) { size_t len; const uint8_t* d = a.get(n, len); auto L = std::make_unique<lyt::Layout>(); if (!d || !L->parse(d, len)) return nullptr; if (R) R->bindTextures(*L, a); return L; }
    static void loadAnim(const gx::Arc& a, const std::string& n, lyt::Anim& A) { size_t len; const uint8_t* d = a.get(n, len); A = lyt::Anim(); if (d) A.parse(d, len); }
    // Vista de 1 jugador sobre recursos compartidos (marco frame00a + frame00b, cartel frame00e, meta frame00f).
    bool init(GameRes* r, const std::string& mode = "Single", int player = 1, int bgIdx = 0, int frameAnimIndex = 0) {
        attachRes(r); const gx::Arc& fa = res->frameArc; size_t len; const uint8_t* d;
        frame = mkLayout(fa, "frame00a.brlyt"); if (!frame) return false;
        setFrameAnim(frameAnimIndex);
        frameB = mkLayout(fa, "frame00b.brlyt"); if (frameB) { loadAnim(fa, "frame00b_00.brlan", frameBAnim); frameBAnim.apply(*frameB, 0); } fbBaseW = 160; fbBaseH = 320;
        titleLyt = mkLayout(fa, "frame00e.brlyt"); if (titleLyt) loadAnim(fa, "frame00e_00.brlan", titleAnim);
        goalLyt = mkLayout(fa, "frame00f.brlyt"); if (goalLyt) loadAnim(fa, "frame00f_00.brlan", goalAnim);
        (void)len; (void)d; drawBg = true; placeFrame = false;
        if (bgIdx >= 0) setBackground(bgIdx);
        setFieldConfig("config_00.brlyt", mode, player);
        ok = atlas != nullptr; return ok;
    }
    // Vista de jugador `slot` (0..n-1) en una partida de n (2..4) jugadores: marcos VS de frame00_02.arc (2) o frame01_01.arc (3-4), config VS2/VS3/VS4.
    // notes_modes_basic.md §3.2: "a" en (PictureCenterX-320, 240-PictureCenterY), "b" en FieldCenter; anims _07/_08 (2 jug.) y frame01a_%02d_03 (3-4).
    // team >= 0: color del marco = equipo del jugador (0 rojo, 1 azul, 2 verde, 3 amarillo; notes_vs §2.2: `%d` = equipo+1); por defecto el del slot.
    bool initVS(GameRes* r, int n, int slot, int bgIdx = -1, int anim34 = 3, int team = -1, int anim2 = -1) {  // anim2 >= 0: anim del marco de 2 jugadores (vs IA: 3 = J1, 17 = IA; por defecto 7/8)
        attachRes(r); if (n < 2 || (n == 2 ? !res->hasVs2 : !res->hasVs4)) return false;
        char nm[64], an[64]; const gx::Arc& fa = n == 2 ? res->frame2Arc : res->frame3Arc; int col = (team >= 0 && team < 4 ? team : slot) + 1;
        if (n == 2) { snprintf(nm, sizeof nm, "frame00a_%d_%dP.brlyt", slot == 0 ? 2 : 3, col); snprintf(an, sizeof an, "frame00a_%d_%dP_%02d.brlan", slot == 0 ? 2 : 3, col, anim2 >= 0 ? anim2 : slot == 0 ? 7 : 8); }
        else { snprintf(nm, sizeof nm, "frame01a_%02d.brlyt", col); snprintf(an, sizeof an, "frame01a_%02d_%02d.brlan", col, anim34); }
        frame = mkLayout(fa, nm); if (!frame) return false;
        setFrameAnimName(an, &fa);
        frameB = mkLayout(fa, n == 2 ? "frame00b.brlyt" : "frame01b.brlyt"); if (frameB) { loadAnim(fa, n == 2 ? "frame00b_00.brlan" : "frame01b_00.brlan", frameBAnim); frameBAnim.apply(*frameB, 0); }
        fbBaseW = n == 2 ? 160.0f : 120.0f; fbBaseH = n == 2 ? 320.0f : 240.0f; titleLyt.reset(); goalLyt.reset();
        drawBg = slot == 0; placeFrame = true; if (bgIdx >= 0) setBackground(bgIdx);
        setFieldConfig("config_00.brlyt", n == 2 ? "VS2" : n == 3 ? "VS3" : "VS4", slot + 1);
        ok = atlas != nullptr; return ok;
    }
    // fondo theme/bg_NN.arc (bg_NN.brlyt + bg_NN_anim.brlan en bucle)
    void setBackground(int idx) {
        if (idx == bgIndex) return; bgIndex = idx; char bgName[64]; snprintf(bgName, sizeof bgName, "bg_%02d.arc", idx);
        bgArc = gx::Arc(); bg.reset(); bgAnim = lyt::Anim(); bgFrame = 0;
        if (!bgArc.load(dir + bgName)) return; R->loadArcTextures(bgArc);
        size_t len; const uint8_t* d = bgArc.get(".brlyt", len); bg = std::make_unique<lyt::Layout>(); if (!d || !bg->parse(d, len)) { bg.reset(); return; }
        d = bgArc.get(".brlan", len); if (d) bgAnim.parse(d, len);
    }
    // FieldInfo/<grupo>/<prefijo>Player0N del layout de configuración (config_00, config_mini, config_sprint…)
    void setFieldConfig(const std::string& cfgLayout, const std::string& mode, int player) {
        size_t len; const uint8_t* d = res->cfgArc.get(cfgLayout, len); cfg = std::make_unique<lyt::Layout>(); if (!d || !cfg->parse(d, len)) { cfg.reset(); return; }
        pc = PlayerCfg(); std::string pname = (mode == "Single" ? "" : mode) + "Player" + (player < 10 ? "0" : "") + std::to_string(player);
        if (lyt::Pane* p = cfg->find(pname)) {
            pc.blockSize = p->udf("BlockSize", 16); pc.blockScale = p->udf("BlockScale", 1); pc.left = p->udf("Left", 248); pc.top = p->udf("Top", 116);
            pc.centerX = p->udf("FieldCenterX", 320); pc.centerY = p->udf("FieldCenterY", 268); pc.nextNum = (int)p->udf("NextBlockNum", 5); pc.holdNum = (int)p->udf("HoldNum", 1);
            pc.picX = p->udf("PictureCenterX", 320); pc.picY = p->udf("PictureCenterY", 240); pc.penaltyNum = (int)p->udf("PenaltyNum", 0); pc.itemNum = (int)p->udf("ItemNum", 0);
            float px = p->x, py = p->y; for (lyt::Pane* q = p->parent; q; q = q->parent) { px += q->x; py += q->y; }
            for (lyt::Pane* c : p->children) {
                float ax = 320 + px + c->x, ay = 240 - (py + c->y);
                if (c->name.find("_NextBlock") != std::string::npos) { pc.nextPos.push_back({ax, ay}); pc.nextScale.push_back(c->sx); }
                else if (c->name.find("_Hold") != std::string::npos) { pc.holdPos.push_back({ax, ay}); pc.holdScale.push_back(c->sx); }
                else if (c->name.find("_Penalty") != std::string::npos) pc.penaltyPos.push_back({ax, ay});
                else if (c->name.find("_Item") != std::string::npos) pc.itemPos.push_back({ax, ay});
            }
        }
    }
    // animación de configuración del marco (frame00a_NN.brlan / frame00a_all_clear.brlan)
    void setFrameAnimName(const std::string& an, const gx::Arc* arc = nullptr) { if (!frame) return; size_t len; const uint8_t* d = (arc ? arc : &res->frameArc)->get(an, len); lyt::Anim A; if (d && A.parse(d, len)) { frameAnim = A; frameAnim.apply(*frame, 0); } if (lyt::Pane* mii = frame->find("L_win_mii")) mii->flags &= ~1; }
    void setFrameAnim(int idx) { char an[64]; snprintf(an, sizeof an, "frame00a_%02d.brlan", idx); setFrameAnimName(an); }
    void setTextS(const std::string& pane, const std::string& s) { std::u16string u; for (char c : s) u.push_back((char16_t)(unsigned char)c); setText(pane, u); }  // texto propio de este marco (no el override global del renderer)
    void setText(const std::string& pane, const std::u16string& s) { if (frame) if (lyt::Pane* p = frame->find(pane)) p->text = s; }
    void setTitle(const std::u16string& s) { if (!titleLyt) return; if (lyt::Pane* p = titleLyt->find("text_title00")) p->text = s; showTitle = true; titleFrame = 0; }

    // atlas blocks00: celdas de 16 px desde x=16; columnas gris,rojo,verde,azul,naranja,amarillo,cian,morado,oscuro (ids 1-9), filas 2-3 ids 10-18
    void minoRect(uint32_t id, SDL_Rect& src) const { int i = (int)id - 1; if (i < 0) i = 0; int col = i % 9, row = (i / 9) * 2; src = {16 + col * 16, row * 16, 16, 16}; }
    // atlas blocks10: vistas previas en celdas de 64 px (4 rotaciones por fila) desde y=24; fila 1 T, 2 L, 3 J, 4 I, 5 Z, 6 S, 7 O
    void previewRect(uint32_t tetrId, SDL_Rect& src) const { src = res ? res->prevRect[tetrId < 12 ? tetrId : 0] : SDL_Rect{0, 24, 64, 64}; }
    // dibuja la vista previa centrada en (x,y) con la escala del panel (16 px por celda a escala 1)
    void blitPreview(uint32_t id, float x, float y, float scale) { SDL_Rect s; previewRect(id, s); float w = s.w * scale, h = s.h * scale; blit(previews, s, x - w / 2, y - h / 2, w, h); }
    void blit(SDL_Texture* t, const SDL_Rect& src, float x, float y, float w, float h, Uint8 alpha = 255) {
        float sx = R->screenW / 640.0f, sy = R->screenH / 480.0f;
        SDL_Rect dst = {(int)lroundf(x * sx), (int)lroundf(y * sy), (int)lroundf(w * sx), (int)lroundf(h * sy)};
        SDL_SetTextureAlphaMod(t, alpha); SDL_RenderCopy(R->rn, t, &src, &dst); SDL_SetTextureAlphaMod(t, 255);
    }

    void fillRect(float x, float y, float w, float h, Uint8 r, Uint8 gg, Uint8 b, Uint8 a) {
        float sx = R->screenW / 640.0f, sy = R->screenH / 480.0f; SDL_Rect d = {(int)lroundf(x * sx), (int)lroundf(y * sy), (int)lroundf(w * sx), (int)lroundf(h * sy)};
        SDL_SetRenderDrawBlendMode(R->rn, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(R->rn, r, gg, b, a); SDL_RenderFillRect(R->rn, &d);
    }
    void draw(const te::Game& g, float dt) {
        if (bg && drawBg) { if (bgAnim.frameCount > 0) { bgFrame += dt * 60.0f; if (bgFrame >= bgAnim.frameCount) bgFrame -= bgAnim.frameCount; bgAnim.apply(*bg, bgFrame); } R->draw(*bg); }
        float bs = pc.blockSize * pc.blockScale; int top = g.h / 2; float fx = pc.left - bs / 2, fy = pc.top - bs / 2;  // Left/Top = centro de la primera celda
        if (frameB) { if (frameB->root) { frameB->root->x = pc.centerX - 320; frameB->root->y = 240 - pc.centerY; frameB->root->sx = (g.w * bs) / fbBaseW; frameB->root->sy = ((g.h / 2) * bs) / fbBaseH; } R->draw(*frameB); }
        if (clipField) { float sx = R->screenW / 640.0f, sy = R->screenH / 480.0f; SDL_Rect c = {(int)lroundf(fx * sx), (int)lroundf(fy * sy), (int)lroundf(g.w * bs * sx), (int)lroundf((g.h - top) * bs * sy)}; SDL_RenderSetClipRect(R->rn, &c); }
        float so = scrollY * bs;  // desplazamiento suave del campo (Corredor)
        curFx = fx; curFy = fy; curBs = bs; curTop = top;
        // siluetas del modo Sombra (celdas objetivo, atenuadas)
        for (int y = top; y < g.h; y++) for (int x = 0; x < g.w; x++) { const te::Cell& c = g.at(x, y); if (!c.shadow || c.mino) continue; SDL_Rect s; minoRect(c.shadow <= 18 ? c.shadow : 1, s); blit(atlas, s, fx + x * bs, fy + (y - top) * bs, bs, bs, 110); }
        // bloques fijados (solo filas visibles)
        if (extra) extra->drawField(*this, fx, fy, bs, top, dt);
        else for (int y = (scrollY > 0 ? top - 1 : top); y < g.h; y++) for (int x = 0; x < g.w; x++) { const te::Cell& c = g.at(x, y); if (!c.mino) continue; SDL_Rect s; minoRect(c.mino, s); blit(atlas, s, fx + x * bs, fy + (y - top) * bs + so, bs, bs); }
        if (g.hasPiece) {
            const te::TetDef& t = *g.piece.tet; int ax = t.marks ? t.mx(g.piece.dir, 0) : 0, ay = t.marks ? t.my(g.piece.dir, 0) : 0; int gy = g.ghostY(), pfy = g.piece.floorY();
            float pso = g.pieceOnBlock() ? so : 0;
            for (int j = 0; j < 7; j++) for (int i = 0; i < 7; i++) {
                uint32_t m = t.cell(i, j, g.piece.dir); if (!m) continue; if (extra) m = extra->pieceMino(m); SDL_Rect s; minoRect(m, s); int cx = i + g.piece.x - ax;
                int cy = j + gy - ay; if (drawGhost && cy >= top) blit(atlas, s, fx + cx * bs, fy + (cy - top) * bs + so, bs, bs, 90);
                cy = j + pfy - ay; if (cy >= top) { blit(atlas, s, fx + cx * bs, fy + (cy - top) * bs + pso, bs, bs); if (extra && extra->pieceBomb(g, i, j)) { SDL_Rect b = {176, 0, 16, 16}; blit(atlas, b, fx + cx * bs, fy + (cy - top) * bs + pso, bs, bs); } }
            }
        }
        if (goalLyt && goalBoundary >= 0) {  // línea de meta del Corredor
            if (goalAnim.frameCount > 0) { goalFrame += dt * 60.0f; if (goalFrame >= goalAnim.frameCount) goalFrame -= goalAnim.frameCount; goalAnim.apply(*goalLyt, goalFrame); }
            if (goalLyt->root) { goalLyt->root->x = pc.centerX - 320; goalLyt->root->y = 240 - (fy + goalBoundary * bs + so); goalLyt->root->sx = (g.w * bs) / 160.0f; goalLyt->root->sy = bs / 16.0f; }
            R->draw(*goalLyt);
        }
        if (clipField) SDL_RenderSetClipRect(R->rn, nullptr);
        if (drawExtra) drawExtra(*this);
        if (frame) { if (placeFrame && frame->root) { frame->root->x = pc.picX - 320; frame->root->y = 240 - pc.picY; } R->draw(*frame); }
        if (drawTop) drawTop(*this);
        if (showTitle && titleLyt) { if (titleAnim.frameCount > 0) { titleFrame += dt * 60.0f; if (titleFrame > titleAnim.frameCount - 1) titleFrame = (float)(titleAnim.frameCount - 1); titleAnim.apply(*titleLyt, titleFrame); } R->draw(*titleLyt); }
        // siguientes y hold
        if (previews) {
            int k = 0; for (uint32_t id : g.queue) { if (k >= pc.nextNum || k >= (int)pc.nextPos.size()) break; blitPreview(id, pc.nextPos[k].first, pc.nextPos[k].second, pc.nextScale[k]); k++; }
            if (g.heldId && !pc.holdPos.empty()) blitPreview(g.heldId, pc.holdPos[0].first, pc.holdPos[0].second, pc.holdScale[0]);
        }
    }
};

} // namespace view
