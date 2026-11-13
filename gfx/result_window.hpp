// RECORDS: fin de partida de un jugador (notes_modes_basic.md §2.2-2.3): texto del cartel NEW RECORD (new_rec de telop.arc),
// ventana de resultados CSingleResultWindow (single_result.brlyt de window.arc) y menú de resultado CONTINUE/RETRY/QUIT (pause01/pause00/pause04).
#pragma once
#include "menu.hpp"
#include "telop.hpp"
#include "../game/modes.hpp"
#include <string>

namespace result {

inline std::u16string u16(const std::string& s) { std::u16string u; for (char c : s) u.push_back((char16_t)(unsigned char)c); return u; }
inline std::string fmtTime(float sec) { int ms = (int)(sec * 1000); char b[32]; snprintf(b, sizeof b, "%02d'%02d\"%02d", ms / 60000, (ms / 1000) % 60, (ms / 10) % 100); return b; }

// Cartel NEW RECORD: new_rec.brlyt + new_rec_anim01 (70 f); text_score "%d" y text_time mm'ss"cc (vacío = ese campo no se muestra; score < 0 = solo tiempo, FUN_80129778)
inline bool playNewRecord(view::Telop& TL, int score, float time) {
    if (!TL.play("new_rec.brlyt", "new_rec_anim01.brlan")) return false;
    if (TL.L) { if (lyt::Pane* p = TL.L->find("text_score")) p->text = score >= 0 ? u16(std::to_string(score)) : std::u16string(); if (lyt::Pane* p = TL.L->find("text_time")) p->text = time >= 0 ? u16(fmtTime(time)) : std::u16string(); }
    return true;
}

// Datos de la partida para la ventana de resultados
struct Data { game::Kind kind = game::Kind::Marathon; bool victory = false; int bestScore = 0, score = 0; float bestTime = -1, time = 0; };
struct Rows { std::u16string label[9], val[9]; };

// Filas por modo (§2.3). Maratón/Maestro/Principiantes/All Clear/Sprint/Bombliss: HIGH SCORE (cabecera) · SCORE · BEST TIME · CURRENT SCORE (cabecera) · SCORE · TIME.
// Sombra/Corredor/Escalador (sin puntuación) [inferido]: BEST TIME y TIME.
inline Rows makeRows(msg::Catalog& M, const Data& d) {
    Rows r; auto best = [&](float t) { return t >= 0 ? u16(fmtTime(t)) : M.get(9, 235); };  // 009#0xeb "－－'－－”－－"
    switch (d.kind) {
    case game::Kind::Shadow: case game::Kind::Drop: case game::Kind::Climber:
        r.label[2] = M.get(14, 34); r.val[2] = best(d.bestTime);
        r.label[4] = M.get(14, 5); r.val[4] = d.victory ? u16(fmtTime(d.time)) : M.get(9, 235); break;
    default:
        r.label[1] = M.get(14, 4);
        r.label[2] = M.get(14, 3); r.val[2] = u16(std::to_string(d.bestScore));
        r.label[3] = M.get(14, 34); r.val[3] = best(d.bestTime);
        r.label[5] = M.get(14, 55);
        r.label[6] = M.get(14, 3); r.val[6] = u16(std::to_string(d.score));
        r.label[7] = M.get(14, 5); r.val[7] = u16(fmtTime(d.time)); break;
    }
    return r;
}

// Ventana de resultados: single_result_on → single_result_00 (bucle) → A → single_result_off
class ResultWindow {
public:
    lyt::Renderer* R = nullptr; const gx::Arc* arc = nullptr; menu::AnimCache cache; menu::Part win; std::function<void(int)> onSe;
    enum St { Closed, In, Idle, Out } st = Closed;
    void load(lyt::Renderer* r, const gx::Arc* winArc) { R = r; arc = winArc; }
    bool open(const Rows& rows) {
        if (!arc || !win.init(R, *arc, cache, "single_result.brlyt", 0xb4)) return false;
        R->bindTextures(*win.L, *arc);
        static const char* A[9] = {"text_00", "text_01", "text_02", "text_03", "text_04", "text_05", "text_06", "text_07", "text_08"};
        static const char* B[9] = {"text_00b", "text_01b", "text_02b", "text_03b", "text_04b", "text_05b", "text_06b", "text_07b", "text_08b"};
        for (int i = 0; i < 9; i++) { win.setText(A[i], rows.label[i]); win.setText(B[i], rows.val[i]); }
        win.setText("text_ok", u"OK");
        win.play("single_result_on.brlan", "single_result_00.brlan"); st = In; if (onSe) onSe(19); return true;
    }
    bool active() const { return st != Closed; }
    // a: A pulsado este frame; devuelve true al cerrarse del todo
    bool update(float dt, bool a) {
        if (st == Closed) return false; win.update(dt);
        if (st == In && win.queue.size() == 1) st = Idle;
        if (st == Idle && a) { if (onSe) onSe(14); win.play("single_result_off.brlan"); st = Out; }
        else if (st == Out && win.idle()) { win.reset(); st = Closed; return true; }
        return false;
    }
    void draw() { if (st != Closed) win.draw(); }
};

// Menú de resultado (máscara 5 = CONTINUE + QUIT tras completar; 6 = RETRY + QUIT tras game over)
class ResultMenu {
public:
    enum Choice { None, Continue, Retry, Quit };
    lyt::Renderer* R = nullptr; msg::Catalog* M = nullptr; const gx::Arc* arc = nullptr; menu::AnimCache cache; menu::Part it[2]; std::function<void(int)> onSe;
    int kind[2] = {Continue, Quit}; int sel = 0; bool shown = false;
    void load(lyt::Renderer* r, msg::Catalog* m, const gx::Arc* telopArc) { R = r; M = m; arc = telopArc; }
    static const char* nameOf(int k) { return k == Continue ? "pause01" : k == Retry ? "pause00" : "pause04"; }
    void open(bool victory) {
        kind[0] = victory ? Continue : Retry; kind[1] = Quit; sel = 0; shown = true;
        for (int i = 0; i < 2; i++) {
            std::string n = nameOf(kind[i]); it[i].init(R, *arc, cache, n + ".brlyt", 0xb4); R->bindTextures(*it[i].L, *arc);
            it[i].setText("text_" + n, M->get(14, kind[i] == Continue ? 15 : kind[i] == Retry ? 41 : 16)); it[i].setPos(0, i == 0 ? -80 : -130);
        }
        refresh();
    }
    void refresh() { for (int i = 0; i < 2; i++) { std::string n = nameOf(kind[i]); char an[40]; snprintf(an, sizeof an, "%s_anim0%d.brlan", n.c_str(), i == sel ? 1 : 2); it[i].play(an); } }
    // Devuelve la elección al confirmar; B/+ = QUIT
    Choice update(float dt, const menu::Input& in) {
        if (!shown) return None; for (auto& p : it) p.update(dt);
        if ((in.up || in.down) && sel == (in.up ? 1 : 0)) { sel = in.up ? 0 : 1; if (onSe) onSe(13); refresh(); }
        if (in.a) { if (onSe) onSe(sel == 0 ? 14 : 16); shown = false; return (Choice)kind[sel]; }
        if (in.b || in.plus) { if (onSe) onSe(16); shown = false; return Quit; }
        return None;
    }
    void draw() { if (shown) for (auto& p : it) p.draw(); }
};

} // namespace result
