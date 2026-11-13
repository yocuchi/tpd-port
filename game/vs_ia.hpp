// Tetris vs. IA (modo 1, notes_modes_basic.md §1.2 / §2.2): telops de fin (win/lose/draw) y menú de resultado RETRY / CONTINUE / QUIT.
// La partida reutiliza el flujo de Tetris VS de 2 jugadores (VersusState + CpuInput); aquí solo va lo propio del modo.
#pragma once
#include "../gfx/telop.hpp"
#include "../gfx/msg.hpp"
#include <vector>
#include <cstdio>

namespace game {

enum VsIaChoice { VIA_RETRY = 0, VIA_CONTINUE = 1, VIA_QUIT = 2 };

// Telop de fin: gana J1 → win.brlyt/win_anim01 (+ JINGLE_WINNER), gana la IA → lose.brlyt/lose_anim01 (+ BGM_GAMEOVER), empate → draw.brlyt/draw_anim01 (sin música).
inline void vsIaEndTelop(std::vector<view::Telop>& TL, const view::Telop& base, bool draw, bool j1Wins) {
    TL.resize(1); view::Telop& t = TL[0]; t.stop(); t.share(base);
    if (draw) t.play("draw.brlyt", "draw_anim01.brlan"); else if (j1Wins) t.play("win.brlyt", "win_anim01.brlan"); else t.play("lose.brlyt", "lose_anim01.brlan");
}

// Menú de resultado (máscara +0x1ae4: 6 = RETRY + QUIT si pierde o empata; 7 = RETRY + CONTINUE + QUIT si gana y nivelIA+1 < 15).
// Botones del telop: pause00 RETRY (014_ingame #41), pause01 CONTINUE (#15), pause04 QUIT (#16); el elegido usa la anim 01, los demás la 02.
struct VsIaMenu {
    view::Telop B[3]; int kind[3] = {VIA_RETRY, VIA_QUIT, VIA_QUIT}; int count = 2, sel = 0; msg::Catalog* M = nullptr; const view::Telop* base = nullptr;
    static const char* layoutOf(int k) { return k == VIA_RETRY ? "pause00.brlyt" : k == VIA_CONTINUE ? "pause01.brlyt" : "pause04.brlyt"; }
    static int animNo(int k) { return k == VIA_RETRY ? 0 : k == VIA_CONTINUE ? 1 : 4; }
    void apply(bool init) {
        float y0 = count == 3 ? -85.0f : -80.0f;
        for (int i = 0; i < count; i++) {
            int k = kind[i]; char an[32], pn[24]; snprintf(an, sizeof an, "pause%02d_anim0%d.brlan", animNo(k), i == sel ? 1 : 2); snprintf(pn, sizeof pn, "text_pause%02d", animNo(k));
            std::u16string tx;
            if (init) { B[i].share(*base); if (M) tx = M->get(14, k == VIA_RETRY ? 41 : k == VIA_CONTINUE ? 15 : 16); }
            else if (B[i].L) { if (lyt::Pane* t = B[i].L->find(pn)) tx = t->text; }
            B[i].play(layoutOf(k), an, 0, y0 - 50.0f * i); if (B[i].L) if (lyt::Pane* t = B[i].L->find(pn)) t->text = tx;
        }
    }
    void open(const view::Telop& baseTelop, msg::Catalog* cat, bool canContinue) {
        base = &baseTelop; M = cat; sel = 0;
        if (canContinue) { count = 3; kind[0] = VIA_RETRY; kind[1] = VIA_CONTINUE; kind[2] = VIA_QUIT; } else { count = 2; kind[0] = VIA_RETRY; kind[1] = VIA_QUIT; }
        apply(true);
    }
    bool move(int d) { int ns = sel + d; if (ns < 0 || ns >= count) return false; sel = ns; apply(false); return true; }
    int choice() const { return kind[sel]; }
    void update(float dt) { for (int i = 0; i < count; i++) B[i].update(dt); }
    void draw() { for (int i = 0; i < count; i++) B[i].draw(); }
};

}  // namespace game
