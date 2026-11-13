// PAUSA: menú de pausa original (CPauseGame, notes_modes_basic.md §6): pause.brlyt + pause_in/out (telop.arc), ítems pause01 CONTINUE, pause00 RETRY,
// pause06 BGM, pause05 SE, pause07 Ghost, pause04 QUIT, y diálogo "¿Salir de la partida actual?" (010_pause#3, window_a de window.arc).
// SE: 0x6f al abrir, 0xd cursor, 0x15 cambio de valor, 0xe confirmar, 0x10 cancelar, 0x11 ítem bloqueado.
#pragma once
#include "menu.hpp"

namespace pausemenu {

class PauseMenu {
public:
    enum Result { None, Continue, Retry, Quit };
    lyt::Renderer* R = nullptr; msg::Catalog* M = nullptr; const gx::Arc* telopArc = nullptr; const gx::Arc* winArc = nullptr; menu::AnimCache cacheT, cacheW;
    menu::Part title, item[6], dlg; std::function<void(int)> onSe;
    // filas del menú (orden de pantalla): 0 CONTINUE, 1 RETRY, 2 BGM, 3 SE, 4 Ghost, 5 QUIT
    enum Row { R_CONTINUE, R_RETRY, R_BGM, R_SE, R_GHOST, R_QUIT };
    bool active = false, finished = false;   // active: pausa abierta; finished: se acaba de cerrar (result válido, un solo frame)
    bool bgm = true, se = true, ghost = true, ghostLocked = false;  // interruptores (el llamador los aplica con onChange)
    std::function<void()> onChange;          // se llama al cambiar bgm/se/ghost
    Result result = None; int sel = 0, dlgSel = 1;
    enum St { Opening, Idle, Closing, DlgIn, Dlg, DlgOut } st = Idle;
    static const char* nameOf(int r) { static const char* N[6] = {"pause01", "pause00", "pause06", "pause05", "pause07", "pause04"}; return N[r]; }
    static bool isToggle(int r) { return r == R_BGM || r == R_SE || r == R_GHOST; }
    void se_(int id) { if (onSe) onSe(id); }

    void load(lyt::Renderer* r, msg::Catalog* m, const gx::Arc* tArc, const gx::Arc* wArc) { R = r; M = m; telopArc = tArc; winArc = wArc; }
    bool toggleValue(int r) const { return r == R_BGM ? bgm : r == R_SE ? se : ghost; }
    void refreshTexts() {
        for (int r = 0; r < 6; r++) {
            if (!item[r].L) continue; std::string n = nameOf(r);
            if (r == R_CONTINUE) item[r].setText("text_pause01", M->get(14, 15)); else if (r == R_RETRY) item[r].setText("text_pause00", M->get(14, 41)); else if (r == R_QUIT) item[r].setText("text_pause04", M->get(14, 16));
            else { if (r == R_GHOST) item[r].setText("text_pause07a", M->get(9, 7)); item[r].setText("text_" + n + "b", M->get(9, toggleValue(r) ? 26 : 27)); }
        }
    }
    void refreshAnims() {
        for (int r = 0; r < 6; r++) {
            if (!item[r].L) continue; char an[48]; int n = (r == R_GHOST && ghostLocked) ? (r == sel ? 3 : 4) : (r == sel ? 1 : 2);
            snprintf(an, sizeof an, "%s_anim0%d.brlan", nameOf(r), n); item[r].play(an);
        }
    }
    void open() {
        title.init(R, *telopArc, cacheT, "pause.brlyt", 0xb0); R->bindTextures(*title.L, *telopArc); title.setPos(0, 40); title.play("pause_in.brlan");
        for (int r = 0; r < 6; r++) { std::string n = nameOf(r); item[r].init(R, *telopArc, cacheT, n + ".brlyt", 0xb0); R->bindTextures(*item[r].L, *telopArc); item[r].setPos(0, 0 - 40.0f * r + 10); }
        sel = 0; refreshTexts(); refreshAnims(); for (int r = 0; r < 6; r++) item[r].update(1.0f);  // aplica el estado final de las animaciones de selección enseguida
        dlg.reset(); active = true; finished = false; result = None; st = Opening; se_(0x6f);
    }
    void showItems(bool v) { for (auto& p : item) p.shown = v; }
    void close(Result r) { result = r; st = Closing; title.play("pause_out.brlan"); }
    void openDialog() {
        dlg.init(R, *winArc, cacheW, "window_a.brlyt", 0xb4); R->bindTextures(*dlg.L, *winArc);
        dlg.setText("text_word_l", M->get(10, 3)); dlg.setText("text_yes", M->get(0, 0)); dlg.setText("text_no", M->get(0, 1));
        if (lyt::Pane* ok = dlg.L->find("button_ok")) ok->flags &= ~1; dlgSel = 1; updateDlg(); dlg.play("window_a_l00_on.brlan", "window_a_l00.brlan"); st = DlgIn; se_(19); showItems(false);
    }
    void updateDlg() { for (int i = 0; i < 2; i++) dlg.setTextColor(i == 0 ? "text_yes" : "text_no", dlgSel == i ? 255 : 160, dlgSel == i ? 224 : 160, dlgSel == i ? 0 : 160); }
    void flip(int r) { if (r == R_BGM) bgm = !bgm; else if (r == R_SE) se = !se; else ghost = !ghost; refreshTexts(); se_(0x15); if (onChange) onChange(); }
    void update(float dt, const menu::Input& in) {
        finished = false; if (!active) return;
        title.update(dt); for (auto& p : item) p.update(dt); dlg.update(dt);
        switch (st) {
        case Opening: if (title.idle()) st = Idle; break;
        case Idle: {
            int d = (in.down ? 1 : 0) - (in.up ? 1 : 0);
            if (d) { sel = (sel + d + 6) % 6; se_(13); refreshAnims(); }
            else if ((in.left || in.right) && isToggle(sel)) { if (sel == R_GHOST && ghostLocked) se_(0x11); else flip(sel); }
            else if (in.a) {
                if (sel == R_CONTINUE) { se_(14); close(Continue); } else if (sel == R_RETRY) { se_(14); close(Retry); }
                else if (sel == R_QUIT) openDialog(); else if (sel == R_GHOST && ghostLocked) se_(0x11); else flip(sel);
            } else if (in.b || in.plus) { se_(16); close(Continue); }
        } break;
        case Closing: if (title.idle()) { active = false; finished = true; } break;
        case DlgIn: if (dlg.queue.size() <= 1) st = Dlg; break;
        case Dlg:
            if (in.left || in.right) { dlgSel ^= 1; se_(13); updateDlg(); }
            else if (in.a && dlgSel == 0) { se_(14); dlg.play("window_a_l00_off.brlan"); st = DlgOut; result = Quit; }
            else if (in.b || (in.a && dlgSel == 1)) { se_(16); dlg.play("window_a_l00_off.brlan"); st = DlgOut; result = None; }
            break;
        case DlgOut: if (dlg.idle()) { dlg.reset(); if (result == Quit) { active = false; finished = true; } else { st = Idle; showItems(true); } } break;
        }
    }
    void draw() { if (!active) return; title.draw(); for (auto& p : item) p.draw(); dlg.draw(); }
};

} // namespace pausemenu
