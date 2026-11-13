// Vista del Escalador: climber_00.brlyt (+14 animaciones climber_00_*.brlan), climb_flag.brlyt, climber_goal.brlyt y frame00c (banderas) — FUN_800c9034 / FUN_800c93e8 / FUN_800b553c.
#pragma once
#include "game_view.hpp"
#include "../game/climber.hpp"

namespace view {

class ClimberView {
public:
    gx::Arc arc; lyt::Renderer* R = nullptr; bool ok = false;
    std::unique_ptr<lyt::Layout> cl, fl, goal, fc; lyt::Anim an[14], flagAnim, goalA0, goalA1, fcA0, fcA1;
    int curAnim = -1; float frame = 0, flagFrame = 0, goalFrame = 0, fcFrame = 0; bool goalOn = false, fcAllTaken = false;
    game::ClimberMode* M = nullptr; std::function<void(int)> onSe;
    static std::unique_ptr<lyt::Layout> mk(const gx::Arc& a, const char* n) { size_t len; const uint8_t* d = a.get(n, len); auto L = std::make_unique<lyt::Layout>(); if (!d || !L->parse(d, len)) return nullptr; return L; }
    static void ld(const gx::Arc& a, const std::string& n, lyt::Anim& A) { size_t len; const uint8_t* d = a.get(n, len); if (d) A.parse(d, len); }

    bool load(lyt::Renderer* r, GameView& V, const std::string& dir, game::ClimberMode* m) {
        R = r; M = m; if (!arc.load(dir + "climber.arc")) return false; R->loadArcTextures(arc);
        cl = mk(arc, "climber_00.brlyt"); fl = mk(arc, "climb_flag.brlyt"); goal = mk(arc, "climber_goal.brlyt"); fc = mk(V.res->frameArc, "frame00c.brlyt");
        for (size_t i = 0; i < 14 && i < M->D->anims.size(); i++) ld(arc, M->D->anims[i], an[i]);
        ld(arc, "climb_flag_00.brlan", flagAnim); ld(arc, "climber_goal_00.brlan", goalA0); ld(arc, "climber_goal_01.brlan", goalA1);
        ld(V.res->frameArc, "frame00c_00.brlan", fcA0); ld(V.res->frameArc, "frame00c_01.brlan", fcA1);
        ok = cl && fl; reset();
        attach(V); return ok;
    }
    void attach(GameView& V) { V.drawExtra = [this](GameView& v) { drawField(v); }; V.drawTop = [this](GameView& v) { drawOver(v); }; }
    static void detach(GameView& V) { V.drawExtra = nullptr; V.drawTop = nullptr; }
    void reset() { curAnim = -1; frame = 0; flagFrame = 0; goalOn = false; fcAllTaken = false; fcFrame = 0; if (fc && fcA0.frameCount) fcA0.apply(*fc, 0); setAnim(11); }
    void setAnim(int i) {
        if (i == curAnim || i < 0 || i >= 14) return; curAnim = i; frame = 0; if (an[i].frameCount) an[i].apply(*cl, 0);
        if (onSe) { if (i == 8 || i == 9) onSe(0x72); else if (i < 2) onSe(0x5a); }   // FUN_800c93e8: anims 8/9 (correr) SE 0x72, 0/1 (escalar) SE 0x5a
    }
    // anim según estado (FUN_800c9034)
    void pickAnim() {
        game::ClimberMode& m = *M; int s = m.state;
        if (s == game::CS_STAND || s == game::CS_RUN) setAnim(m.tx < m.cx ? 8 : m.cx < m.tx ? 9 : 11);
        else if (s == game::CS_CLIMB) { if (m.tx < m.cx) setAnim(0); else if (m.cx < m.tx) setAnim(1); }
        else if (s == game::CS_FLY) setAnim(m.tx < m.cx ? 5 : 3);
        else if (s == game::CS_FALL) setAnim(2);
        else if (s == game::CS_GOAL) setAnim(7);
        else if (s == game::CS_DEAD) { if (m.deadPhase == 0) { setAnim(12); if (onSe) onSe(0x5b); } else if (m.deadPhase == 1 && curAnim == 12 && anyDone()) { setAnim(13); } }
    }
    bool anyDone() const { return an[12].frameCount == 0 || frame >= an[12].frameCount - 1; }
    void update(float dt) {
        if (!ok) return; pickAnim();
        const lyt::Anim& A = an[curAnim]; if (A.frameCount > 0) { frame += dt * 60.0f; if (A.loop) { while (frame >= A.frameCount) frame -= A.frameCount; } else if (frame > A.frameCount - 1) frame = (float)(A.frameCount - 1); A.apply(*cl, frame); }
        if (M->state == game::CS_DEAD && M->deadPhase == 1 && curAnim == 13) M->deadAnimDone = true;
        if (flagAnim.frameCount) { flagFrame += dt * 60.0f; while (flagFrame >= flagAnim.frameCount) flagFrame -= flagAnim.frameCount; }
        if (M->goalRunning) { if (!goalOn) { goalOn = true; goalFrame = 0; } goalFrame += dt * 60.0f; }
        else goalOn = false;
        if (fc) { if (M->allFlags && !fcAllTaken) { fcAllTaken = true; fcFrame = 0; } if (fcAllTaken && fcA1.frameCount) { fcFrame += dt * 60.0f; if (fcFrame > fcA1.frameCount - 1) fcFrame = (float)(fcA1.frameCount - 1); fcA1.apply(*fc, fcFrame); } else if (fcA0.frameCount) fcA0.apply(*fc, 0); }
    }
    // posición en el espacio 640x480 del centro de la celda (con interpolación hacia el destino)
    void cellCenter(GameView& v, float x, float y, float& X, float& Y) { X = v.curFx + v.curBs / 2 + x * v.curBs; Y = v.curFy + v.curBs / 2 + (y - v.curTop) * v.curBs; }
    void place(lyt::Layout& L, float X, float Y, float sc) { if (!L.root) return; L.root->x = X - 320; L.root->y = 240 - Y; L.root->sx = L.root->sy = sc; }
    void drawField(GameView& v) {
        if (!ok) return; game::ClimberMode& m = *M; float sc = v.curBs / 16.0f;
        for (auto& f : m.flags) { if (f.state != 1 && f.state != 2) continue; if (f.y < v.curTop) continue; float X, Y; cellCenter(v, (float)f.x, (float)f.y, X, Y); place(*fl, X, Y, sc); if (flagAnim.frameCount) flagAnim.apply(*fl, flagFrame); R->draw(*fl); }
        if (m.state == game::CS_DEAD && m.deadPhase >= 2) { float X, Y; cellCenter(v, (float)m.cx, (float)m.cy, X, Y); place(*cl, X, Y - m.deadRise * sc, sc); R->draw(*cl); return; }
        if (m.state == game::CS_DEAD && m.deadPhase == 3) return;
        float fx = (float)m.cx, fy = (float)m.cy; if (m.cx != m.tx) fx += (m.tx - m.cx) * m.progress; if (m.cy != m.ty) fy += (m.ty - m.cy) * m.progress;
        if (m.ty < v.curTop && m.cy < v.curTop) return;
        float X, Y; cellCenter(v, fx, fy, X, Y); place(*cl, X, Y, sc); R->draw(*cl);
    }
    void drawOver(GameView& v) {
        if (!ok) return;
        if (fc && fc->root) { fc->root->x = v.pc.centerX - 320; fc->root->y = 240 - v.pc.centerY; fc->root->sx = (v.curBs * 10) / 160.0f; fc->root->sy = (v.curBs * 20) / 320.0f; R->draw(*fc); }
        if (goal && goalOn) {
            if (goalA0.frameCount) goalA0.apply(*goal, std::min(goalFrame, (float)goalA0.frameCount - 1)); if (goalA1.frameCount) goalA1.apply(*goal, std::min(goalFrame, (float)goalA1.frameCount - 1));
            place(*goal, v.pc.centerX, v.pc.centerY, 1.0f); R->draw(*goal);
        }
    }
};

} // namespace view
