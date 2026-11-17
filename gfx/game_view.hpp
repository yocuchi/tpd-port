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
#include <cstring>
#include <algorithm>

namespace view {

struct PlayerCfg {
    float blockSize = 16, blockScale = 1, left = 248, top = 116, centerX = 320, centerY = 268, picX = 320, picY = 240; int nextNum = 5, holdNum = 1, penaltyNum = 0, itemNum = 0; float penaltySize = 0;  // penaltySize > 0: lado propio de las celdas de la barra Penalty (VS 6)
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
    bool vs6 = false; int vs6Slot = 0, vs6Team = 0, vs6Wins = 0; float vs6BaseW = 0, vs6U = 8, vs6Cx = 0, vs6BoxY = 0, vs6Fy = 0;
    int vs6Cols = 6, vs6Rows = 1; bool vs6Side = false; float vs6SideX = 0, vs6SideY = 0;  // disposición: VS 6 = 6x1 con cajas encima; VS 8 (initVS8) = 4x2 con cajas a la derecha  // Tetris VS 6 (initVS6): marco dibujado por código (sin brlyt)
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
        (void)len; (void)d; drawBg = true; placeFrame = false; vs6 = false;
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
        drawBg = slot == 0; placeFrame = true; vs6 = false; if (bgIdx >= 0) setBackground(bgIdx);
        setFieldConfig("config_00.brlyt", n == 2 ? "VS2" : n == 3 ? "VS3" : "VS4", slot + 1);
        ok = atlas != nullptr; return ok;
    }
    // Tetris VS 6 (modo 27): campo `slot` (0..5) de una fila de 6 en 640x480, sin marco brlyt (el original no trae marcos para 6): el borde, los recuadros de
    // hold/next, la etiqueta Pn y el contador de victorias se dibujan con rectángulos (fuente de píxeles 3x5, ver drawVs6). Celda de 8 px: campo 10x20 = 80x160.
    // Reparto: paso 106 px, centro del campo en x = 53 + 106*slot, y = 150..310; hold/next encima (y 92..148), Pn y victorias debajo, Penalty a la derecha.
    bool initVS6(GameRes* r, int slot, int bgIdx = -1, int team = -1) { return initVsBig(r, slot, bgIdx, team, 6, 1, false); }
    // Tetris VS 8 (modo 28): 4 columnas x 2 filas (alto 240 cada una); hold/next/Pn en una columna a la derecha del campo, barra Penalty a la izquierda.
    bool initVS8(GameRes* r, int slot, int bgIdx = -1, int team = -1) { return initVsBig(r, slot, bgIdx, team, 4, 2, true); }
    bool initVsBig(GameRes* r, int slot, int bgIdx, int team, int cols, int rows, bool side) {
        attachRes(r); if (!res->hasVs4 || slot < 0 || slot >= cols * rows) return false;
        vs6Cols = cols; vs6Rows = rows; vs6Side = side;
        frame.reset(); titleLyt.reset(); goalLyt.reset(); frameAnim = lyt::Anim();
        frameB = mkLayout(res->frame3Arc, "frame01b.brlyt"); if (frameB) { loadAnim(res->frame3Arc, "frame01b_00.brlan", frameBAnim); frameBAnim.apply(*frameB, 0); }
        fbBaseW = 120.0f; fbBaseH = 240.0f; drawBg = slot == 0; placeFrame = false; if (bgIdx >= 0) setBackground(bgIdx);
        vs6 = true; vs6Slot = slot; vs6Team = team >= 0 && team < 8 ? team : slot; vs6Wins = 0; cfg.reset(); layoutVs6();
        ok = atlas != nullptr; return ok;
    }
    // Geometría de VS 6 en función del ancho base W (640 en 4:3 → celda 8; 853 en 16:9 → celda 11): paso W/6, celda u = (paso-26)/10 entera, campo 10u x 20u,
    // recuadros de hold/next de 7u encima, Pn y victorias debajo; el conjunto (30u+6) se centra en vertical.
    void layoutVs8() {
        float W = R->baseW; vs6BaseW = W; float step = W / vs6Cols; float u = floorf(std::min((240 - 8) / 20.0f, (step - 56) / 14)); if (u < 6) u = 6; if (u > 12) u = 12; vs6U = u;
        int col = vs6Slot % vs6Cols, row = vs6Slot / vs6Cols; float sw = 4.5f * u, pw = floorf(1.1f * u) + 2, bw = pw + 10 * u + 4 + sw;  // penalty+borde | campo | borde+separación | columna
        float L = floorf(step * (col + 0.5f) - bw / 2), fx = L + pw, m = floorf((240 - 20 * u) / 2), fy = row * 240 + m; float cx = fx + 5 * u; vs6Cx = cx; vs6Fy = fy; vs6BoxY = fy;
        float sx = fx + 10 * u + 4; vs6SideX = sx; vs6SideY = fy; float ps = u / 16, hh = 3.5f * u, ny = fy + hh + 2;
        pc = PlayerCfg(); pc.blockSize = u; pc.blockScale = 1; pc.left = fx + u / 2; pc.top = fy + u / 2; pc.centerX = cx; pc.centerY = fy + 10 * u; pc.picX = cx; pc.picY = fy + 10 * u;
        pc.nextNum = 3; pc.holdNum = 1; pc.holdPos.push_back({sx + sw / 2, fy + 2.3f * u}); pc.holdScale.push_back(ps);
        for (int k = 0; k < 3; k++) { pc.nextPos.push_back({sx + sw / 2, ny + 2.4f * u + 2.4f * u * k}); pc.nextScale.push_back(ps); }
        pc.penaltyNum = 20; pc.penaltySize = u * 0.75f; for (int k = 0; k < 20; k++) pc.penaltyPos.push_back({fx - 2 - u * 0.55f, fy + 20 * u - u * 0.5f - u * k});
    }
    void layoutVs6() {
        if (vs6Side) { layoutVs8(); return; }
        float W = R->baseW; vs6BaseW = W; float step = W / 6; float u = floorf((step - 26) / 10); if (u < 6) u = 6; if (u > 12) u = 12; vs6U = u;
        float cx = step * (vs6Slot + 0.5f); vs6Cx = cx; float total = 30 * u + 6; float m = floorf((480 - total) / 2); vs6BoxY = m; float fy = m + 7 * u + 3; vs6Fy = fy;
        pc = PlayerCfg(); pc.blockSize = u; pc.blockScale = 1; pc.left = cx - 5 * u + u / 2; pc.top = fy + u / 2; pc.centerX = cx; pc.centerY = fy + 10 * u; pc.picX = cx; pc.picY = 240;
        pc.nextNum = 3; pc.holdNum = 1; float ps = u / 16; pc.holdPos.push_back({cx - 2.75f * u, m + 4 * u}); pc.holdScale.push_back(ps);
        for (int k = 0; k < 3; k++) { pc.nextPos.push_back({cx + 2.6f * u, m + 2.0f * u + 2 * u * k}); pc.nextScale.push_back(ps); }
        pc.penaltyNum = 20; pc.penaltySize = u * 0.75f; for (int k = 0; k < 20; k++) pc.penaltyPos.push_back({cx + 5 * u + 2 + u * 0.55f, fy + 20 * u - u * 0.5f - u * k});
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
        float sx = R->screenW / R->baseW, sy = R->screenH / 480.0f;
        SDL_Rect dst = {(int)lroundf(x * sx), (int)lroundf(y * sy), (int)lroundf(w * sx), (int)lroundf(h * sy)};
        SDL_SetTextureAlphaMod(t, alpha); SDL_RenderCopy(R->rn, t, &src, &dst); SDL_SetTextureAlphaMod(t, 255);
    }

    void fillRect(float x, float y, float w, float h, Uint8 r, Uint8 gg, Uint8 b, Uint8 a) {
        float sx = R->screenW / R->baseW, sy = R->screenH / 480.0f; SDL_Rect d = {(int)lroundf(x * sx), (int)lroundf(y * sy), (int)lroundf(w * sx), (int)lroundf(h * sy)};
        SDL_SetRenderDrawBlendMode(R->rn, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(R->rn, r, gg, b, a); SDL_RenderFillRect(R->rn, &d);
    }
    // Fuente de píxeles 3x5 del marco VS 6 (P, W, H, O, L, D, N, E, X, T, G, A, M, V, R y dígitos); px = lado del píxel en coordenadas de 640x480
    void pixText(const char* s, float x, float y, float px, Uint8 r, Uint8 gg, Uint8 b) {
        static const char* G[] = {"0:111101101101111", "1:010110010010111", "2:111001111100111", "3:111001111001111", "4:101101111001001", "5:111100111001111", "6:111100111101111", "7:111001010010010",
            "8:111101111101111", "9:111101111001111", "P:111101111100100", "W:101101101111101", "H:101101111101101", "O:111101101101111", "L:100100100100111", "D:110101101101110", "N:111101101101101", "E:111100111100111", "X:101101010101101", "T:111010010010010", "G:111100101101111", "A:010101111101101", "M:101111111101101", "V:101101101101010", "R:110101110101101"};
        for (; *s; s++, x += 4 * px) { for (const char* gl : G) if (gl[0] == *s) for (int i = 0; i < 15; i++) if (gl[2 + i] == '1') fillRect(x + (i % 3) * px, y + (i / 3) * px, px, px, r, gg, b, 255); }
    }
    // Jugador eliminado en VS (modo fantasma): velo oscuro sobre el campo visible y "GAME OVER" en la parte alta; el campo se sigue viendo para practicar
    void drawOut(int w, int h) {
        float bs = pc.blockSize * pc.blockScale; int top = h / 2; float fx = pc.left - bs / 2, fy = pc.top - bs / 2, fw = w * bs, fh = (h - top) * bs;
        fillRect(fx, fy, fw, fh, 0, 0, 0, 110);
        float px = floorf(bs / 4); if (px < 1) px = 1; float tw = 9 * 4 * px - px; fillRect(fx + (fw - tw) / 2 - px, fy + 1.5f * bs - px, tw + 2 * px, 7 * px, 0, 0, 0, 170); pixText("GAME OVER", fx + (fw - tw) / 2, fy + 1.5f * bs, px, 255, 80, 60);
    }
    void drawVs6() {
        static const Uint8 TC[8][3] = {{255, 70, 60}, {80, 140, 255}, {70, 220, 70}, {255, 225, 40}, {64, 224, 255}, {255, 150, 40}, {190, 90, 255}, {230, 230, 230}};
        const Uint8* c = TC[vs6Team % 8]; float u = vs6U, cx = vs6Cx, fx = cx - 5 * u, fy = vs6Fy, fw = 10 * u, fh = 20 * u, by = vs6BoxY, bh = 7 * u;
        fillRect(fx - 2, fy - 2, fw + 4, 2, c[0], c[1], c[2], 255); fillRect(fx - 2, fy + fh, fw + 4, 2, c[0], c[1], c[2], 255);  // borde de 2 px del color del equipo
        fillRect(fx - 2, fy, 2, fh, c[0], c[1], c[2], 255); fillRect(fx + fw, fy, 2, fh, c[0], c[1], c[2], 255);
        if (vs6Side) {  // VS 8: columna a la derecha del campo: HOLD, NEXT (3 piezas), Pn + victorias
            float sx = vs6SideX, sw = 4.5f * u, hh = 3.5f * u, ny = fy + hh + 2, nh = 9 * u; float px8 = floorf(u / 8); if (px8 < 1) px8 = 1;
            fillRect(sx, fy, sw, hh, 0, 0, 0, 150); fillRect(sx, ny, sw, nh, 0, 0, 0, 150); pixText("HOLD", sx + 2, fy + 2, px8, 200, 200, 200); pixText("NEXT", sx + 2, ny + 2, px8, 200, 200, 200);
            float lp8 = floorf(u / 4); if (lp8 < 2) lp8 = 2; float ly = ny + nh + 5; char b8[8]; snprintf(b8, sizeof b8, "P%d", vs6Slot + 1); pixText(b8, sx, ly, lp8, c[0], c[1], c[2]);
            snprintf(b8, sizeof b8, "%d", std::min(99, vs6Wins)); pixText(b8, sx + sw - 4 * lp8 * (float)strlen(b8) + lp8, ly, lp8, 255, 255, 255); return;
        }
        fillRect(fx - 2, by, 4.75f * u, bh, 0, 0, 0, 150); fillRect(cx - 0.25f * u, by, 5.25f * u + 2, bh, 0, 0, 0, 150);  // fondo de hold y next
        float px = floorf(u / 8); if (px < 1) px = 1; pixText("HOLD", fx, by + 2, px, 200, 200, 200); pixText("NEXT", cx, by + 2, px, 200, 200, 200);
        float lp = floorf(u / 4); if (lp < 2) lp = 2; char b[8]; snprintf(b, sizeof b, "P%d", vs6Slot + 1); pixText(b, fx - 2, fy + fh + 5, lp, c[0], c[1], c[2]);
        snprintf(b, sizeof b, "%d", std::min(99, vs6Wins)); pixText(b, fx + fw + 2 - 4 * lp * (float)strlen(b) + lp, fy + fh + 5, lp, 255, 255, 255);
    }
    void draw(const te::Game& g, float dt) {
        if (vs6 && R->baseW != vs6BaseW) layoutVs6();
        if (bg && drawBg && vs6 && bg->root) { float k = R->baseW / 640.0f; bg->root->sx = k; bg->root->sy = k; }
        if (bg && drawBg) { if (bgAnim.frameCount > 0) { bgFrame += dt * 60.0f; if (bgFrame >= bgAnim.frameCount) bgFrame -= bgAnim.frameCount; bgAnim.apply(*bg, bgFrame); } R->draw(*bg); }
        float bs = pc.blockSize * pc.blockScale; int top = g.h / 2; float fx = pc.left - bs / 2, fy = pc.top - bs / 2;  // Left/Top = centro de la primera celda
        if (frameB) { if (frameB->root) { frameB->root->x = pc.centerX - 320; frameB->root->y = 240 - pc.centerY; frameB->root->sx = (g.w * bs) / fbBaseW; frameB->root->sy = ((g.h / 2) * bs) / fbBaseH; }
            if (vs6) {  // VS 6: frame01b es más ancho que el campo (en el VS de 4 lo tapa el marco): fondo oscuro sólido y recorte al campo
                float sx = R->screenW / R->baseW, sy = R->screenH / 480.0f; SDL_Rect c = {(int)lroundf(fx * sx), (int)lroundf(fy * sy), (int)lroundf(g.w * bs * sx), (int)lroundf((g.h - top) * bs * sy)};
                (void)c; fillRect(fx, fy, g.w * bs, (g.h - top) * bs, 8, 8, 16, 235); for (int k = 1; k < g.w; k++) fillRect(fx + k * bs, fy, 1, (g.h - top) * bs, 255, 255, 255, 18); for (int k = 1; k < g.h - top; k++) fillRect(fx, fy + k * bs, g.w * bs, 1, 255, 255, 255, 18);  // rejilla propia en vez de frame01b
            } else R->draw(*frameB); }
        if (clipField) { float sx = R->screenW / R->baseW, sy = R->screenH / 480.0f; SDL_Rect c = {(int)lroundf(fx * sx), (int)lroundf(fy * sy), (int)lroundf(g.w * bs * sx), (int)lroundf((g.h - top) * bs * sy)}; SDL_RenderSetClipRect(R->rn, &c); }
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
        if (vs6) drawVs6();
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
