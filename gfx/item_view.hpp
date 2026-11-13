// Vista de los objetos de Tetris VS: iconos de las ranuras Item0..2 (item.arc: item.brlyt + item_NN.brlan), celdas-objeto, efectos (effect.arc:
// item_get, simo01..07 humo, hide_eff, speed_eff, stoptime_eff, muteki_eff, confusion, fix_*_mino, kick_eff, blk_shot_eff, expl_bomb_eff) y escalador (climber.arc).
// Todo lo de effect.arc/climber.arc es opcional: sin esos arcs solo faltan los adornos. Se dibuja DESPUÉS de GameView::draw (encima del marco).
#pragma once
#include "game_view.hpp"
#include "../game/items.hpp"
#include <map>

namespace view {

class ItemView {
public:
    lyt::Renderer* R = nullptr; gx::Arc itemArc, fxArc, climbArc; bool ok = false, fxOk = false, climbOk = false;
    std::unique_ptr<lyt::Layout> icon[14]; std::map<std::string, lyt::Anim> anims;
    struct Act { const gx::Arc* arc = nullptr; std::unique_ptr<lyt::Layout> L; const lyt::Anim* A = nullptr; float f = 0, x = 0, y = 0, sc = 1; bool loop = false; bool done() const { return !A || A->frameCount == 0 || (!loop && f >= A->frameCount - 1); } };
    struct Cloud { std::unique_ptr<lyt::Layout> L; const lyt::Anim *disp = nullptr, *wipe = nullptr; float f = 0; int ph = 2; };  // ph 0 aparece/permanece, 1 se retira, 2 fuera
    struct Persist { Act a; bool on = false; };
    struct PV { std::vector<Act> act; Cloud cloud[7]; Persist accel, stop, cascade, confuse, fix; std::unique_ptr<lyt::Layout> climber; std::unique_ptr<lyt::Layout> mark; const lyt::Anim* climbAnim[3] = {}; Act hide; float markF = 0, clF = 0; int fixId = 0; } pv[4];

    std::unique_ptr<lyt::Layout> mk(const gx::Arc& a, const std::string& n) { size_t len; const uint8_t* d = a.get("/" + n, len); if (!d) d = a.get(n, len); auto L = std::make_unique<lyt::Layout>(); if (!d || !L->parse(d, len)) return nullptr; R->bindTextures(*L, a); return L; }
    const lyt::Anim* anim(const gx::Arc& a, const std::string& n) {
        auto it = anims.find(n); if (it != anims.end()) return it->second.frameCount ? &it->second : nullptr;
        size_t len; const uint8_t* d = a.get("/" + n, len); if (!d) d = a.get(n, len); lyt::Anim A; if (d) A.parse(d, len); auto& e = anims[n]; e = A; return e.frameCount ? &e : nullptr;
    }
    bool load(lyt::Renderer* r, const std::string& dir) {
        R = r; ok = itemArc.load(dir + "item.arc"); fxOk = fxArc.load(dir + "effect.arc"); climbOk = climbArc.load(dir + "climber.arc");
        if (!ok) return false;
        for (const game::ItemInfo& in : game::ITEM_INFO) {  // un layout por icono, con su item_NN.brlan (1 frame) aplicado
            char nm[32]; snprintf(nm, sizeof nm, "item_%02d.brlan", in.icon); auto L = mk(itemArc, "item.brlyt"); if (!L) continue;
            if (const lyt::Anim* A = anim(itemArc, nm)) { A->apply(*L, 0); R->bindTextures(*L, itemArc); }  // el brlan cambia la textura de Picture_00: se vuelve a ligar
            icon[in.icon] = std::move(L);
        }
        return true;
    }
    void reset() { for (PV& p : pv) { p = PV(); } }

    static void place(lyt::Layout& L, float X, float Y, float sc) { if (!L.root) return; L.root->x = X - 320; L.root->y = 240 - Y; L.root->sx = L.root->sy = sc; }
    Act spawn(const std::string& lyt, const std::string& an, float x, float y, float sc, bool loop = false) {
        Act a; if (!fxOk) return a; a.arc = &fxArc; a.L = mk(fxArc, lyt); a.A = anim(fxArc, an); a.x = x; a.y = y; a.sc = sc; a.loop = loop; if (a.L && a.A) { a.A->apply(*a.L, 0); R->bindTextures(*a.L, fxArc); } return a;
    }
    void adv(Act& a, float dt) { if (!a.A || !a.L) return; a.f += dt * 60.0f; if (a.loop) { while (a.f >= a.A->frameCount) a.f -= a.A->frameCount; } else if (a.f > a.A->frameCount - 1) a.f = (float)(a.A->frameCount - 1); a.A->apply(*a.L, a.f); if (a.arc) R->bindTextures(*a.L, *a.arc); }
    void drawAct(Act& a) { if (a.L) { place(*a.L, a.x, a.y, a.sc); R->draw(*a.L); } }
    void persist(Persist& ps, bool want, float dt, const char* lyt, const char* an, float x, float y, float sc) {
        if (want && !ps.on) { ps.a = spawn(lyt, an, x, y, sc, true); ps.on = true; } else if (!want) { ps.on = false; ps.a = Act(); }
        if (ps.on && ps.a.L) { ps.a.x = x; ps.a.y = y; ps.a.sc = sc; adv(ps.a, dt); drawAct(ps.a); }
    }
    static float cellX(GameView& v, float x) { return v.curFx + v.curBs / 2 + x * v.curBs; }
    static float cellY(GameView& v, float y) { return v.curFy + v.curBs / 2 + (y - v.curTop) * v.curBs; }
    static const char* fixName(int id) { switch (id) { case 1: return "fix_i_mino.brlan"; case 2: return "fix_o_mino.brlan"; case 3: return "fix_z_mino.brlan"; case 4: return "fix_s_mino.brlan"; case 5: return "fix_l_mino.brlan"; case 6: return "fix_j_mino.brlan"; } return "fix_t_mino.brlan"; }

    // Dibuja los objetos del jugador i sobre su vista V (llamar tras V.draw). dt en segundos.
    void draw(game::ItemSystem& it, int i, GameView& V, float dt) {
        if (!ok || i >= it.n() || !it.enabled) return; PV& p = pv[i]; const game::ItemSystem::St& s = it.S[i]; te::Game& g = it.G(i); const PlayerCfg& pc = V.pc;
        float fw = g.w * V.curBs, sc = fw / 160.0f, fcx = pc.centerX, fcy = pc.centerY; int N = it.n(); bool two = N == 2;
        // ---- eventos de este jugador ----
        for (size_t k = 0; k < it.fx.size();) {
            const game::ItemFx& e = it.fx[k]; if (e.player != i) { k++; continue; }
            switch (e.type) {
            case game::ItemFx::Get: if (e.a < (int)pc.itemPos.size()) { float s0 = e.a == 0 ? (two ? 0.7f : 0.88f) : (two ? 0.6f : 0.7f); p.act.push_back(spawn("item_get.brlyt", "item_get_00.brlan", pc.itemPos[e.a].first, pc.itemPos[e.a].second, s0 * 1.4f)); } break;
            case game::ItemFx::SmokeStart: {
                char sz = two ? 'l' : 'm'; for (int c = 0; c < 7; c++) { Cloud& cl = p.cloud[c]; char b[48]; snprintf(b, sizeof b, "simo%02d.brlyt", c + 1); cl.L = fxOk ? mk(fxArc, b) : nullptr;
                    snprintf(b, sizeof b, "simo%02d_%c_display.brlan", c + 1, sz); cl.disp = anim(fxArc, b); snprintf(b, sizeof b, "simo%02d_%c_wipe.brlan", c + 1, sz); cl.wipe = anim(fxArc, b); cl.f = 0; cl.ph = cl.L ? 0 : 2; if (cl.L && cl.disp) cl.disp->apply(*cl.L, 0); }
                break; }
            case game::ItemFx::SmokeWipe: if (e.a >= 0 && e.a < 7 && p.cloud[e.a].ph == 0) { p.cloud[e.a].ph = 1; p.cloud[e.a].f = 0; } break;
            case game::ItemFx::Kick: p.act.push_back(spawn("kick_eff.brlyt", "kick_eff_00.brlan", fcx, V.curFy + (e.a + 0.5f) * V.curBs, sc)); break;
            case game::ItemFx::Shot: p.act.push_back(spawn("blk_shot_eff.brlyt", "blk_shot_eff_shoot.brlan", cellX(V, e.a), cellY(V, e.b), V.curBs / 16.0f)); break;
            case game::ItemFx::BombBlast: p.act.push_back(spawn("expl_bomb_eff.brlyt", "expl_bomb_eff.brlan", cellX(V, e.a), cellY(V, e.b), V.curBs * 3 / 16.0f)); break;
            case game::ItemFx::Hit: if (e.a == game::IT_FIX) p.fixId = e.c; break;
            default: break;
            }
            it.fx.erase(it.fx.begin() + k);
        }
        // ---- celdas-objeto (pieza activa y ya fijadas) y bombas ----
        p.markF += dt * 60.0f; float pulse = 0.85f + 0.15f * sinf(p.markF * 0.15f);
        auto markAt = [&](float cx, float cy) {  // celda-objeto: brillo pulsante blanco con borde dorado (el original usa un bloque especial; sin textura propia) [inferido]
            if (cy < V.curTop) return; float X = cellX(V, cx) - V.curBs / 2, Y = cellY(V, cy) - V.curBs / 2, b = std::max(1.0f, V.curBs / 8), w = V.curBs; Uint8 a = (Uint8)(80 + 110 * (pulse - 0.85f) / 0.15f * 0.5f + 60);
            V.fillRect(X, Y, w, w, 255, 255, 255, a); V.fillRect(X, Y, w, b, 255, 210, 40, 255); V.fillRect(X, Y + w - b, w, b, 255, 210, 40, 255); V.fillRect(X, Y, b, w, 255, 210, 40, 255); V.fillRect(X + w - b, Y, b, w, 255, 210, 40, 255);
        };
        for (int y = V.curTop; y < g.h; y++) for (int x = 0; x < g.w; x++) { const te::Cell& c = g.at(x, y); if (!c.mino) continue; if (c.flags & game::CELL_ITEM) markAt((float)x, (float)y); if ((c.flags & game::CELL_BOMB) && V.atlas) { SDL_Rect b = {176, 0, 16, 16}; V.blit(V.atlas, b, V.curFx + x * V.curBs, V.curFy + (y - V.curTop) * V.curBs, V.curBs, V.curBs); } }
        int mx, my; if (it.pieceMark(i, mx, my)) markAt((float)mx, (float)my);
        // ---- humo, escalador y efectos persistentes sobre el campo ----
        for (int c = 0; c < 7; c++) { Cloud& cl = p.cloud[c]; if (cl.ph == 0 && !s.cloud[c]) cl.ph = 2; /* limpiado desde fuera (fin de partida) */ if (cl.ph == 2 || !cl.L) continue; const lyt::Anim* A = cl.ph == 0 ? cl.disp : cl.wipe;
            if (A) { cl.f += dt * 60.0f; if (cl.ph == 0) { if (cl.f > A->frameCount - 1) cl.f = (float)(A->frameCount - 1); } else if (cl.f >= A->frameCount - 1) { cl.ph = 2; continue; } A->apply(*cl.L, cl.f); R->bindTextures(*cl.L, fxArc); }
            place(*cl.L, fcx, fcy, sc); R->draw(*cl.L); }
        if (s.clAlive && climbOk) {
            if (!p.climber) { p.climber = mk(climbArc, "climber_00.brlyt"); p.climbAnim[0] = anim(climbArc, "climber_00_wait.brlan"); }
            if (p.climber && s.clY >= V.curTop) { p.clF += dt * 60.0f; const lyt::Anim* A = p.climbAnim[0]; if (A) { A->apply(*p.climber, fmodf(p.clF, (float)A->frameCount)); R->bindTextures(*p.climber, climbArc); } place(*p.climber, cellX(V, (float)s.clX), cellY(V, (float)s.clY), V.curBs / 16.0f); R->draw(*p.climber); }
        }
        persist(p.accel, s.tAccel > 0, dt, "speed_eff.brlyt", "speed_eff_anim.brlan", fcx, fcy, sc);
        persist(p.stop, s.tFreeze > 0, dt, "stoptime_eff.brlyt", "stoptime_eff_anim.brlan", fcx, fcy, sc);
        persist(p.cascade, s.tCascade > 0, dt, "muteki_eff.brlyt", "muteki_eff_anim.brlan", fcx, fcy, sc);
        persist(p.confuse, s.tConfuse > 0, dt, "confusion.brlyt", "confusion.brlan", fcx, fcy, sc);
        persist(p.fix, s.tFix > 0, dt, "fix.brlyt", fixName(p.fixId), fcx, fcy, sc);
        // ---- esconder: tapa la cola de SIGUIENTE ----
        if (s.tHide > 0) for (size_t k = 0; k < pc.nextPos.size() && (int)k < pc.nextNum; k++) { float ns = pc.nextScale[k]; V.fillRect(pc.nextPos[k].first - 36 * ns, pc.nextPos[k].second - 22 * ns, 72 * ns, 44 * ns, 24, 24, 40, 255);
            if (fxOk) { if (!p.hide.L) { p.hide = spawn("hide_eff.brlyt", "hide_eff_00.brlan", 0, 0, 1); if (p.hide.L && p.hide.A) { p.hide.A->apply(*p.hide.L, (float)(p.hide.A->frameCount - 1)); R->bindTextures(*p.hide.L, fxArc); } } if (p.hide.L) { p.hide.x = pc.nextPos[k].first; p.hide.y = pc.nextPos[k].second; p.hide.sc = ns * 1.2f; drawAct(p.hide); } } }
        // ---- estado: iconos pequeños sobre el campo con los efectos activos (recibidos o propios) ----
        { struct E { float t; int icon; } es[] = {{s.tAccel, 7}, {s.tHide, 8}, {s.tFreeze, 5}, {s.tLock, 6}, {s.tConfuse, 11}, {s.tFix, 12}, {s.tCascade, 4}, {s.tShot, 1}, {s.tBomb, 13}, {s.tShift, 2}, {s.clAlive ? 1.0f : 0.0f, 9}, {s.smoke > 0 ? 1.0f : 0.0f, 0}}; int k = 0;
          for (const E& e : es) { if (e.t <= 0 || !icon[e.icon]) continue; place(*icon[e.icon], V.curFx + 10 + k * 18 * V.curBs / 16.0f, V.curFy + 10, 0.5f * V.curBs / 16.0f); R->draw(*icon[e.icon]); k++; } }
        // ---- iconos de las ranuras (Item0 = el que se usará primero) ----
        for (int k = 0; k < game::ItemSystem::SLOTS && k < (int)pc.itemPos.size(); k++) {
            if (!s.slot[k]) continue; const game::ItemInfo* in = game::itemInfo(s.slot[k]); if (!in || !icon[in->icon]) continue;
            float s0 = k == 0 ? (two ? 0.7f : 0.88f) : (two ? 0.6f : 0.7f); place(*icon[in->icon], pc.itemPos[k].first, pc.itemPos[k].second, s0); R->draw(*icon[in->icon]);
        }
        // ---- efectos puntuales (item_get, patada, disparo, explosión) ----
        for (size_t k = 0; k < p.act.size();) { Act& a = p.act[k]; adv(a, dt); drawAct(a); if (a.done() && a.f > 0) p.act.erase(p.act.begin() + k); else k++; }
    }
};

} // namespace view
