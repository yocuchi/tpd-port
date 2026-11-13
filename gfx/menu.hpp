// Menús originales (menu.arc + system/window.arc), según notes_menu.md: fondo menu_bg, barra de título menu2_top,
// barra inferior menu2_back/back2, menú principal (menu2_sub00/04 x10) y selección de modo (menu2_mode_icon + menu2_mode + window_c + menu_play).
#pragma once
#include "render.hpp"
#include "brlan.hpp"
#include "msg.hpp"
#include "../game/vs_config.hpp"
#include "../game/settings.hpp"  // AJUSTES
#include "../game/modes.hpp"     // AJUSTES (valores por defecto de fondo/música por modo)
#include <memory>
#include <cstring>
#include <algorithm>
#include <functional>

namespace menu {

struct AnimCache {
    std::map<std::string, lyt::Anim> anims;
    const lyt::Anim* get(const gx::Arc& arc, const std::string& name) {
        auto it = anims.find(name); if (it != anims.end()) return &it->second;
        size_t len; const uint8_t* d = arc.get(name, len); lyt::Anim A; if (!d || !A.parse(d, len)) return nullptr;
        return &(anims[name] = A);
    }
};

// Una instancia de layout con cola de animaciones (equivalente al objeto "layout" del juego).
struct Part {
    lyt::Renderer* R = nullptr; const gx::Arc* arc = nullptr; AnimCache* cache = nullptr;
    std::unique_ptr<lyt::Layout> L; int prio = 0x100; bool shown = true;
    struct Q { const lyt::Anim* a; float frame; };
    std::vector<Q> queue;

    bool init(lyt::Renderer* r, const gx::Arc& a, AnimCache& c, const std::string& brlyt, int priority) {
        R = r; arc = &a; cache = &c; prio = priority; size_t len; const uint8_t* d = a.get(brlyt, len); L = std::make_unique<lyt::Layout>();
        return d && L->parse(d, len);
    }
    void clearQueue() { queue.clear(); }
    void queueAnim(const std::string& name) { const lyt::Anim* a = cache->get(*arc, name); if (a) queue.push_back({a, 0}); }
    void play(const std::string& name) { clearQueue(); queueAnim(name); if (!queue.empty()) queue[0].a->apply(*L, 0); }
    void play(const std::string& a, const std::string& b) { play(a); queueAnim(b); }
    void play(const std::string& a, const std::string& b, const std::string& c) { play(a, b); queueAnim(c); }
    // salta al estado final de una animación (sin reproducirla)
    void setEnd(const std::string& name) { const lyt::Anim* a = cache->get(*arc, name); if (a) a->apply(*L, (float)std::max(0, a->frameCount - 1)); clearQueue(); }
    bool idle() const { return queue.empty() || (queue.size() == 1 && queue[0].a->loop); }  // no quedan anims no cíclicas
    void update(float dt) {
        if (queue.empty()) return;
        Q& q = queue[0]; q.frame += dt * 60.0f; int n = q.a->frameCount;
        if (q.a->loop) { if (n > 0) while (q.frame >= n) q.frame -= n; q.a->apply(*L, q.frame); return; }
        if (q.frame >= n - 1) { q.a->apply(*L, (float)std::max(0, n - 1)); queue.erase(queue.begin()); if (!queue.empty()) queue[0].a->apply(*L, 0); return; }
        q.a->apply(*L, q.frame);
    }
    void setPos(float x, float y) { if (L && L->root) { L->root->x = x; L->root->y = y; } }
    void setText(const std::string& pane, const std::u16string& s) { if (L) if (lyt::Pane* p = L->find(pane)) p->text = s; }
    void setVertexColor(const std::string& pane, uint8_t r, uint8_t g, uint8_t b) { if (L) if (lyt::Pane* p = L->find(pane)) for (int i = 0; i < 4; i++) { p->vcol[i][0] = r; p->vcol[i][1] = g; p->vcol[i][2] = b; } }
    void setTextColor(const std::string& pane, uint8_t r, uint8_t g, uint8_t b) { if (L) if (lyt::Pane* p = L->find(pane)) for (int i = 0; i < 2; i++) { p->tcol[i][0] = r; p->tcol[i][1] = g; p->tcol[i][2] = b; } }
    void setRootAlpha(uint8_t a) { if (L && L->root) L->root->alpha = a; }
    void reset() { L.reset(); queue.clear(); shown = true; }
    void draw() { if (shown && L) R->draw(*L); }
};

struct Input { bool up = false, down = false, left = false, right = false, a = false, b = false, x = false, plus = false; };  // flancos

// Tabla de colores del juego (0x803F0C68): id → color2 (barra)
inline void colorOf(int id, uint8_t c[3]) {
    static const uint8_t T[][3] = {{217,31,31},{217,109,0},{206,206,0},{103,206,0},{22,188,22},{0,206,123},{61,204,204},{54,136,217},{69,69,217},{130,43,217},{174,35,174},{217,0,109}};
    if (id >= 0x23 && id <= 0x2e) { memcpy(c, T[id - 0x23], 3); return; }
    if (id == 0x0b) { c[0] = 64; c[1] = 224; c[2] = 255; return; }
    if (id == 2) { c[0] = 255; c[1] = 224; c[2] = 0; return; }
    c[0] = c[1] = c[2] = 255;
}

// Tablas por modo (0..26), notes_menu.md §5
struct ModeInfo { int msgFile, nameIdx, icon, descIdx; };
inline const ModeInfo& modeInfo(int m) {
    static const ModeInfo T[27] = {
        {2,1,1,0},{2,2,2,1},{2,3,3,22},{2,4,4,3},{2,5,5,23},{2,20,15,5},{2,18,10,31},{2,12,6,24},{2,14,7,25},{2,16,8,26},
        {3,1,17,6},{3,2,18,27},{3,3,16,8},{3,4,20,9},{3,5,19,28},{3,7,13,12},{3,6,11,11},{3,15,21,29},{3,17,12,30},
        {32,7,22,13},{32,21,9,16},{32,8,23,15},{4,1,1,0},{4,2,1,0},{4,1,1,0},{3,19,24,32},{3,20,25,33}};
    return T[m >= 0 && m < 27 ? m : 0];
}

class MenuSystem {
public:
    lyt::Renderer* R = nullptr; msg::Catalog* M = nullptr; gx::Arc menuArc, winArc; AnimCache cache;
    enum Screen { None, Main, Mode, Settings /* AJUSTES */ }; Screen screen = None, nextScreen = None;
    enum Phase { In, Idle, Out }; Phase phase = In;
    // resultado para el exterior
    std::function<void(int)> onSe;  // efectos: 13 cursor, 14 confirmar, 16 cancelar, 17 error
    void se(int id) { if (onSe) onSe(id); }
    int startMode = -1;   // >=0: arrancar partida con este modo
    bool backToTitle = false;

    Part bg, titleBar, statusBar, arrowUp, arrowDown, arrowL, arrowR, modeName, playBar, winC, rulesWin, msgBox;
    int modeState = 0;  // 0 normal, 2 reglas entrando, 3 reglas abiertas, 4 reglas saliendo, 5 msgbox abierto, 6 msgbox cerrando, 7-12 ventana de jugadores (Tetris VS)
    // ---- ventana de jugadores de Tetris VS (menu2_win + menu2_win_cursor, notes_vs §1): estado 0xE de la selección de modo ----
    game::VsConfig vsCfg;      // configuración editada (persiste entre aperturas); main_sdl.cpp la lee al recibir startMode 10/25
    int joyCount = 0;          // mandos conectados: PER/INVITADO solo para J1 o si hay mando para ese jugador (J k = mando k-1)
    Part pwin, pcur; int pRow = 0, pCol = 1; bool pStart = false; game::VsConfig pBackup;  // 7 abriendo, 8 navegando, 9 editando celda, 10 cerrando, 11 aviso abierto, 12 aviso cerrando
    static bool isVsMode(int m) { return m == 10 || m == 25; }
    int msgSel = 0; int ruleType = 0;
    // id de reglas y tipo de animación de la ventana de reglas (notes_menu §4.4/§4.6)
    static int rulesIdOf(int m) { if (m <= 0xe) return m + 1; switch (m) { case 0xf: return 0x11; case 0x10: return 0x10; case 0x11: return 0x13; case 0x12: return 0x14; case 0x13: return 0x17; case 0x14: return 0x19; case 0x15: return 0x18; case 0x19: return 0xb; case 0x1a: return 0x13; } return 1; }
    static int ruleTypeOf(int id) { switch (id) { case 3: case 0xb: case 0xc: case 0xd: case 0x13: case 0x16: return 3; case 4: case 0xe: return 1; case 5: case 0xf: return 2; case 10: return 6; case 0x11: case 0x12: return 4; case 0x17: case 0x18: case 0x19: return 5; } return 0; }
    static int ruleTextOf(int id) { static const int T[0x1a] = {0, 0, 1, 22, 3, 23, 5, 31, 24, 25, 26, 6, 27, 8, 9, 28, 11, 12, 30, 29, 30, 14, 14, 13, 15, 16}; return id >= 0 && id < 0x1a ? T[id] : 0; }
    static const char* ruleAnim(int type, int which) {  // which: 0 in, 1 loop, 2 out
        static const int In[8] = {1, 11, 21, 26, 36, 46, 36, 41}, Lp[8] = {2, 12, 22, 27, 37, 47, 37, 42}, Out[8] = {5, 15, 25, 30, 40, 50, 40, 45};
        static char buf[48]; int n = which == 0 ? In[type & 7] : which == 1 ? Lp[type & 7] : Out[type & 7]; snprintf(buf, sizeof buf, "operation_anim%02d.brlan", n); return buf;
    }
    void openRules(int mode) {
        int id = rulesIdOf(mode); ruleType = ruleTypeOf(id);
        rulesWin.init(R, menuArc, cache, "operation.brlyt", 0x109); rulesWin.setText("text_ope_main", M->get(11, ruleTextOf(id)));
        const char* icons[] = {"text_icon01", "text_icon02", "text_icon03", "text_icon04", "text_icon05", "text_icon06"}; for (int i = 0; i < 6; i++) rulesWin.setText(icons[i], M->get(13, i));
        rulesWin.setText("text_icon13", M->get(13, 11)); rulesWin.setText("text_icon08", M->get(13, 7));
        if (ruleType == 5) { rulesWin.setText("text_icon01", M->get(13, 8)); rulesWin.setText("text_icon03", M->get(13, 9)); rulesWin.setText("text_icon05", M->get(13, 10)); }
        rulesWin.play(ruleAnim(ruleType, 0), ruleAnim(ruleType, 1)); modeState = 2; se(19); winC.shown = false; playBar.shown = false; modeName.shown = false;
    }
    void openMsgBox() {
        msgBox.init(R, winArc, cache, "window_a.brlyt", 0xb4); msgBox.setText("text_word_l", M->get(9, 207)); msgBox.setText("text_yes", M->get(0, 0)); msgBox.setText("text_no", M->get(0, 1));
        if (lyt::Pane* ok = msgBox.L->find("button_ok")) ok->flags &= ~1; msgSel = 1; updateMsgSel(); msgBox.play("window_a_l00_on.brlan", "window_a_l00.brlan"); modeState = 5; se(19);
    }
    void updateMsgSel() { msgBox.setTextColor("text_yes", msgSel == 0 ? 255 : 160, msgSel == 0 ? 224 : 160, msgSel == 0 ? 0 : 160); msgBox.setTextColor("text_no", msgSel == 1 ? 255 : 160, msgSel == 1 ? 224 : 160, msgSel == 1 ? 0 : 160); }
    std::vector<std::unique_ptr<Part>> items;   // menú principal / lista de Ajustes
    std::vector<int> itemSub;                   // AJUSTES: nº de menu2_subNN de cada ítem de la lista (0 = botón sub00, 1 = ítem con ventana de valor sub01, 4 = sub04)
    std::vector<std::unique_ptr<Part>> icons;   // selección de modo
    std::vector<int> modeIds; int listType = 0; int sel = 0, first = 0, prevSel = -1; float scrollY = 0, scrollTarget = 0;
    static constexpr int VISIBLE = 6; static constexpr float BASE_Y = 132, SPACING = 40;
    int lastSel[4] = {0, 0, 0, 0}; int mainSel = 0;

    bool load(lyt::Renderer* r, msg::Catalog* m, const std::string& dir) {
        R = r; M = m; if (!menuArc.load(dir + "menu.arc")) return false; winArc.load(dir + "window.arc");
        R->loadArcTextures(menuArc); R->loadArcTextures(winArc);
        bg.init(R, menuArc, cache, "menu_bg.brlyt", 0x118); bg.play("menu_bg_scroll.brlan");
        return true;
    }
    void open() { startMode = -1; backToTitle = false; enterMain(0); }

    // ---------- menú principal ----------
    struct MainItem { int res, textIdx, descFile, descIdx, color; };
    static const MainItem* mainItems() {
        static const MainItem T[10] = {{13,39,2,7,0x23},{9,3,1,9,0x24},{9,4,1,10,0x25},{9,28,1,29,0x26},{9,5,1,11,0x28},{9,21,1,22,0x29},{9,24,1,25,0x2b},{9,6,1,12,0x2c},{9,7,1,13,0x2d},{9,8,1,14,0x2e}};
        return T;
    }
    static bool mainEnabled(int i) { return i == 0 || i == 1 || i == 2 || i == 3; }  // Wi-Fi, Balance Board, datos, tutorial, récords y opciones: pendientes

    void enterMain(int restoreSel) {
        screen = Main; phase = In; items.clear(); icons.clear(); for (Part* p : {&modeName, &playBar, &winC, &arrowL, &arrowR}) p->reset();
        titleBar.init(R, menuArc, cache, "menu2_top.brlyt", 0x104); titleBar.setText("text_mode", M->get(1, 2)); titleBar.play("menu2_top_in.brlan");
        statusBar.init(R, menuArc, cache, "menu2_back.brlyt", 0x104); statusBar.setText("text_control", M->get(1, 15)); statusBar.play("menu2_back_in.brlan");
        arrowUp.init(R, menuArc, cache, "menu2_arrow.brlyt", 0xff); arrowDown.init(R, menuArc, cache, "menu2_arrow.brlyt", 0xff); arrowUp.shown = arrowDown.shown = false;
        itemSub.clear(); for (int i = 0; i < 10; i++) itemSub.push_back(mainItems()[i].res - 9);
        sel = restoreSel; first = std::max(0, std::min(sel - VISIBLE + 1, 10 - VISIBLE)); if (sel < VISIBLE) first = 0; scrollY = scrollTarget = SPACING * first; prevSel = -1;
        for (int i = 0; i < 10; i++) {
            const MainItem& it = mainItems()[i]; auto p = std::make_unique<Part>();
            char ly[32]; snprintf(ly, sizeof ly, "menu2_sub%02d.brlyt", it.res - 9); p->init(R, menuArc, cache, ly, 0x10e);
            char tp[32]; snprintf(tp, sizeof tp, "text_sub%02d", it.res - 9); p->setText(tp, M->get(1, it.textIdx));
            uint8_t c[3]; colorOf(it.color, c); char b0[32], b1[32]; snprintf(b0, sizeof b0, "sub%02dbar00", it.res - 9); snprintf(b1, sizeof b1, "sub%02dbar01", it.res - 9);
            p->setVertexColor(b0, c[0], c[1], c[2]); p->setVertexColor(b1, c[0], c[1], c[2]);
            if (!mainEnabled(i)) p->setRootAlpha(0x40);
            p->setPos(0, 240 - (BASE_Y + SPACING * (i - first)));
            char an[48]; snprintf(an, sizeof an, "menu2_sub%02d_in.brlan", it.res - 9); p->play(an);
            p->shown = (i >= first && i < first + VISIBLE);
            items.push_back(std::move(p));
        }
        updateMainSelection(true);
    }
    void subAnim(int i, const char* suffix) { int res = itemSub[i]; char an[48]; snprintf(an, sizeof an, "menu2_sub%02d_%s.brlan", res, suffix); items[i]->play(an); }
    void updateMainSelection(bool initial) {
        if (prevSel >= 0 && prevSel != sel) subAnim(prevSel, "out2");
        if (prevSel != sel) {
            if (initial) { int res = itemSub[sel]; char an[48]; snprintf(an, sizeof an, "menu2_sub%02d_in.brlan", res); items[sel]->play(an); snprintf(an, sizeof an, "menu2_sub%02d_in2.brlan", res); items[sel]->queueAnim(an); snprintf(an, sizeof an, "menu2_sub%02d_in3.brlan", res); items[sel]->queueAnim(an); }
            else { int res = itemSub[sel]; char an[48]; snprintf(an, sizeof an, "menu2_sub%02d_in2.brlan", res); items[sel]->play(an); snprintf(an, sizeof an, "menu2_sub%02d_in3.brlan", res); items[sel]->queueAnim(an); }
            prevSel = sel;
        }
        if (screen == Main) { const MainItem& it = mainItems()[sel]; statusBar.setText("text_comment", M->get(it.descFile, it.descIdx)); }
        arrowUp.shown = first > 0; arrowDown.shown = first + VISIBLE < (int)items.size();
        if (arrowUp.shown && arrowUp.queue.empty()) arrowUp.play("menu2_arrow_up_in.brlan", "menu2_arrow_up_on.brlan");
        if (arrowDown.shown && arrowDown.queue.empty()) arrowDown.play("menu2_arrow_down_in.brlan", "menu2_arrow_down_on.brlan");
        if (!arrowUp.shown) arrowUp.clearQueue(); if (!arrowDown.shown) arrowDown.clearQueue();
    }
    void mainInput(const Input& in) {
        if (in.a) {
            if (!mainEnabled(sel)) { se(17); return; }
            se(14);
            subAnim(sel, "on"); mainSel = sel; phase = Out; outTimer = 0;
            if (sel == 0) { pendingStart = 0; nextScreen = None; }
            else { nextScreen = Mode; listType = (sel == 1) ? 0 : (sel == 2) ? 1 : 2; }
            leaveMain(); return;
        }
        if (in.b) { se(16); phase = Out; outTimer = 0; nextScreen = None; pendingBack = true; leaveMain(); return; }
        int ns = sel;
        if (in.down) ns = std::min(9, sel + 1); else if (in.up) ns = std::max(0, sel - 1);
        if (ns != sel) {
            se(13); sel = ns; if (sel >= first + VISIBLE) first = sel - VISIBLE + 1; if (sel < first) first = sel; scrollTarget = SPACING * first;
            updateMainSelection(false);
        }
    }
    void leaveMain() {
        for (int i = 0; i < (int)items.size(); i++) if (items[i]->shown) subAnim(i, "out");
        statusBar.play("menu2_back_out.brlan"); arrowUp.clearQueue(); arrowDown.clearQueue(); arrowUp.shown = arrowDown.shown = false;
        if (nextScreen != Mode) titleBar.play("menu2_top_out.brlan");
    }

    // ---------- selección de modo ----------
    static const std::vector<int>& modeList(int type) {
        static const std::vector<int> T[4] = {{0, 1, 2, 3, 4, 7, 8, 9}, {10, 25, 11, 12, 13, 14, 15, 16, 18, 17, 26}, {5, 6}, {19, 20, 21, 0}};
        return T[type & 3];
    }
    static int iconOf(int mode) { return modeInfo(mode).icon; }
    void enterMode(int type) {
        screen = Mode; phase = In; listType = type; items.clear(); icons.clear(); modeIds = modeList(type); for (Part* p : {&arrowUp, &arrowDown, &rulesWin, &msgBox}) p->reset(); modeState = 0;
        int titleFile = type == 0 ? 2 : type == 1 ? 3 : 2, titleIdx = type == 2 ? 6 : 0;
        titleBar.init(R, menuArc, cache, "menu2_top.brlyt", 0x104); titleBar.setText("text_mode", M->get(titleFile, titleIdx)); titleBar.play("menu2_top_in.brlan");
        statusBar.init(R, menuArc, cache, "menu2_back2.brlyt", 0x104); statusBar.setText("text_comment", M->get(1, 38)); statusBar.play("menu2_back2_in.brlan");
        sel = lastSel[type]; if (sel < 0 || sel >= (int)modeIds.size()) sel = 0; prevSel = -1;
        modeName.init(R, menuArc, cache, "menu2_mode.brlyt", 0x103); modeName.play("menu2_mode_in.brlan");
        playBar.init(R, menuArc, cache, "menu_play.brlyt", 0x101); playBar.setEnd("menu_play_off.brlan");
        winC.init(R, winArc, cache, "window_c.brlyt", 0x104); winC.play("window_c_frame_in.brlan");
        arrowL.init(R, menuArc, cache, "menu2_arrow.brlyt", 0xff); arrowR.init(R, menuArc, cache, "menu2_arrow.brlyt", 0xff);
        arrowL.play("menu2_arrow_left_in.brlan", "menu2_arrow_left_on.brlan"); arrowR.play("menu2_arrow_right_in.brlan", "menu2_arrow_right_on.brlan");
        int n = (int)modeIds.size();
        for (int i = 0; i < n; i++) {
            auto p = std::make_unique<Part>(); p->init(R, menuArc, cache, "menu2_mode_icon.brlyt", 0x10e); p->setPos(-25.0f * (n - 1) + 50.0f * i, 0);
            if (i == sel) p->play(iconAnim(modeIds[i], "in"), iconAnim(modeIds[i], "on"), iconAnim(modeIds[i], "on02")); else p->play(iconAnim(modeIds[i], "in"));
            icons.push_back(std::move(p));
        }
        refreshMode();
    }
    static std::string iconAnim(int mode, const char* suffix) { char an[48]; snprintf(an, sizeof an, "menu2_mode_icon_%02d_%s.brlan", iconOf(mode), suffix); return an; }
    void refreshMode() {
        if (prevSel >= 0 && prevSel != sel) { icons[prevSel]->play(iconAnim(modeIds[prevSel], "off")); icons[sel]->play(iconAnim(modeIds[sel], "on"), iconAnim(modeIds[sel], "on02")); }
        prevSel = sel; int m = modeIds[sel]; const ModeInfo& mi = modeInfo(m);
        modeName.setText("TextBox_01", M->get(mi.msgFile, mi.nameIdx)); winC.setText("text_ope_main_00", M->get(11, mi.descIdx));
        if (phase == Idle) playBar.setEnd("menu_play_on.brlan");
    }
    void finishStart() { lastSel[listType] = sel; pendingStart = modeIds[sel]; nextScreen = None; phase = Out; outTimer = 0; leaveMode(); }
    void startSelectedMode() { if (isVsMode(modeIds[sel])) { openPlayers(); return; } finishStart(); }

    // ---------- ventana de jugadores (CMenuMultiPlayerWindow, notes_vs §1.2-1.4) ----------
    static std::u16string num16(int v) { std::string t = std::to_string(v); return std::u16string(t.begin(), t.end()); }
    bool canHuman(int j) const { return j == 0 || joyCount > j; }
    static void teamColor(int t, uint8_t c[3]) { static const uint8_t T[4][3] = {{255, 70, 60}, {80, 140, 255}, {70, 220, 70}, {255, 225, 40}}; memcpy(c, T[t & 3], 3); }
    void openPlayers() {
        pwin.init(R, menuArc, cache, "menu2_win.brlyt", 0xB4); pcur.init(R, menuArc, cache, "menu2_win_cursor.brlyt", 0xB3);
        pwin.setText("Player", M->get(9, 388)); pwin.setText("Team", M->get(9, 389)); pwin.setText("Handlcap", M->get(9, 393)); pwin.setText("text_ok", M->get(9, 11));
        for (int j = 0; j < 4; j++) { char nm[8]; snprintf(nm, sizeof nm, "P%d", j + 1); pwin.setText(nm, M->get(9, 83 + j)); }
        vsCfg.type[0] = game::VT_HUMAN; for (int j = 1; j < 4; j++) if ((vsCfg.type[j] == game::VT_HUMAN || vsCfg.type[j] == game::VT_GUEST) && !canHuman(j)) vsCfg.type[j] = game::VT_CPU;
        pBackup = vsCfg; pRow = 0; pCol = 1; pStart = false; pwin.shown = pcur.shown = true; pwin.play("menu2_win_in.brlan"); se(19); modeState = 7; refreshPlayers();
    }
    static const char* colName(int c) { return c == 1 ? "PLAYER" : c == 2 ? "TEAM" : "HANDICAP"; }
    void cellText(int j, int col, std::u16string& txt, uint8_t c[3], bool& dim) {
        using namespace game; c[0] = c[1] = c[2] = 255; dim = vsCfg.type[j] == VT_OFF && col != 1;
        if (col == 1) { int t = vsCfg.type[j]; txt = M->get(9, t == VT_OFF ? 0x5A : t == VT_HUMAN ? 0x57 : t == VT_GUEST ? 0x58 : 0x59); if (t == VT_OFF) c[0] = c[1] = c[2] = 170; }
        else if (col == 2) { txt = M->get(9, 0x2B + (vsCfg.team[j] & 3)); teamColor(vsCfg.team[j], c); }
        else { int h = vsCfg.handicap[j]; txt = h == 0 ? M->get(9, 0x1B) : num16(2 * h); }
        if (dim) c[0] = c[1] = c[2] = 110;
    }
    void refreshPlayers() {
        if (!pwin.L) return;
        for (int j = 0; j < 4; j++) for (int col = 1; col <= 3; col++) {
            char nm[24]; snprintf(nm, sizeof nm, "%dP_%s", j + 1, colName(col)); std::u16string t; uint8_t c[3]; bool dim; cellText(j, col, t, c, dim);
            pwin.setText(nm, t); pwin.setTextColor(nm, c[0], c[1], c[2]);
        }
        if (pRow == 0) { pcur.shown = false; if (modeState != 10) pwin.play("menu2_win_on.brlan"); return; }
        pwin.play("menu2_win_off.brlan"); char nm[24]; snprintf(nm, sizeof nm, "%dP_%s", pRow, colName(pCol)); pcur.shown = true;
        if (lyt::Pane* cell = pwin.L->find(nm)) { float x = 0, y = 0; for (const lyt::Pane* q = cell; q; q = q->parent) { x += q->x; y += q->y; } pcur.setPos(x, y); }
        std::u16string t; uint8_t c[3]; bool dim; cellText(pRow - 1, pCol, t, c, dim); pcur.setText("TextBox_00", t); pcur.setTextColor("TextBox_00", c[0], c[1], c[2]);
        char an[48]; snprintf(an, sizeof an, "menu2_win_cursor_%s_%s.brlan", colName(pCol), modeState == 9 ? "on" : "off"); pcur.play(an);
    }
    void closePlayers(bool start) { pStart = start; pcur.shown = false; pwin.play("menu2_win_out.brlan"); se(20); modeState = 10; }
    void openInfo(int textIdx) {
        msgBox.init(R, winArc, cache, "window_a.brlyt", 0xb2); msgBox.setText("text_word_l", M->get(9, textIdx)); msgBox.setText("text_ok", M->get(0, 2));
        for (const char* nm : {"button_yes", "button_no"}) if (lyt::Pane* b = msgBox.L->find(nm)) b->flags &= ~1;
        if (lyt::Pane* ok = msgBox.L->find("button_ok")) ok->flags |= 1;
        msgBox.play("window_a_l00_on.brlan", "window_a_l00.brlan"); modeState = 11; se(19);
    }
    // valor de la celda en edición: izquierda/derecha (d = -1/+1); PER/INVITADO solo con mando (salta a 3 hacia delante / a 0 hacia atrás)
    void changeCell(int d) {
        using namespace game; int j = pRow - 1;
        if (pCol == 1) { int v = (vsCfg.type[j] + d + 4) & 3; if ((v == VT_HUMAN || v == VT_GUEST) && !canHuman(j)) v = d > 0 ? VT_CPU : VT_OFF; vsCfg.type[j] = v; }
        else if (pCol == 2) vsCfg.team[j] = (vsCfg.team[j] + d + 4) & 3;
        else vsCfg.handicap[j] = (vsCfg.handicap[j] + d + 6) % 6;
    }
    void playersInput(const Input& in) {
        using namespace game;
        if (modeState == 7) { if (pwin.idle()) { modeState = 8; refreshPlayers(); } return; }
        if (modeState == 10) { if (pwin.idle()) { pwin.reset(); pcur.reset(); modeState = 0; if (pStart) finishStart(); } return; }
        if (modeState == 11) { if (!msgBox.idle()) return; if (in.a || in.b) { se(14); msgBox.play("window_a_l00_off.brlan"); modeState = 12; } else if (msgBox.L) { for (const char* nm : {"button_yes", "button_no"}) if (lyt::Pane* b = msgBox.L->find(nm)) b->flags &= ~1; if (lyt::Pane* ok = msgBox.L->find("button_ok")) ok->flags |= 1; } return; }
        if (modeState == 12) { if (msgBox.idle()) { msgBox.reset(); modeState = 8; } return; }
        if (modeState == 8) {
            if (in.b) { se(16); vsCfg = pBackup; closePlayers(false); return; }
            if (in.a) {
                if (pRow == 0) { if (vsCfg.valid()) { se(14); closePlayers(true); } else { se(17); openInfo(461); } return; }  // #461: ajustes de equipo no válidos
                if (pRow == 1 && pCol == 1) { se(17); openInfo(159); return; }                                               // #159: J1 no editable
                se(0x15); pBackup = vsCfg; modeState = 9; refreshPlayers(); return;
            }
            int nr = pRow, nc = pCol;
            if (in.down) nr = (pRow + 1) % 5; else if (in.up) nr = (pRow + 4) % 5;
            else if (pRow != 0 && in.right) nc = std::min(3, pCol + 1); else if (pRow != 0 && in.left) nc = std::max(1, pCol - 1);
            if (nr != pRow || nc != pCol) { se(13); pRow = nr; pCol = nc; refreshPlayers(); }
            return;
        }
        if (modeState == 9) {
            if (in.a) { se(0x15); modeState = 8; refreshPlayers(); return; }
            if (in.b) { se(16); vsCfg = pBackup; modeState = 8; refreshPlayers(); return; }
            if (in.left || in.right) { se(13); changeCell(in.right ? 1 : -1); refreshPlayers(); }
        }
    }
    void modeInput(const Input& in) {
        int n = (int)modeIds.size();
        if (modeState >= 7) { playersInput(in); return; }
        if (modeState == 2) { if (rulesWin.idle()) modeState = 3; return; }
        if (modeState == 3) { if (in.a || in.b || in.plus) { se(14); rulesWin.play(ruleAnim(ruleType, 2)); modeState = 4; } return; }
        if (modeState == 4) { if (rulesWin.idle()) { rulesWin.reset(); winC.shown = true; playBar.shown = true; modeName.shown = true; openMsgBox(); } return; }
        if (modeState == 5) {
            if (!msgBox.idle()) return;
            if (in.left || in.right) { se(13); msgSel ^= 1; updateMsgSel(); }
            if (in.a) { se(14); msgBox.play("window_a_l00_off.brlan"); modeState = 6; }   // sí = ver reglas (flujo de reglas pendiente) / no = partida: ambos arrancan la partida
            if (in.b) { se(16); msgSel = 1; msgBox.play("window_a_l00_off.brlan"); modeState = 6; }
            return;
        }
        if (modeState == 6) { if (msgBox.idle()) { msgBox.reset(); modeState = 0; startSelectedMode(); } return; }
        if (in.x && cfg) { se(14); lastSel[listType] = sel; setMode = modeIds[sel]; setPage = 0; setSel = 0; levelWarned = false; nextScreen = Settings; phase = Out; outTimer = 0; leaveMode(); return; }  // AJUSTES: (-) = X abre Ajustes (notes_menu §4.5)
        if (in.a || in.plus) { se(14); openRules(modeIds[sel]); return; }
        if (in.b) { se(16); lastSel[listType] = sel; nextScreen = Main; phase = Out; outTimer = 0; leaveMode(); return; }
        int ns = sel; if (in.right) ns = (sel + 1) % n; else if (in.left) ns = (sel + n - 1) % n;
        if (ns != sel) { se(13); sel = ns; refreshMode(); }
    }
    void leaveMode() {
        statusBar.play("menu2_back2_out.brlan"); winC.play("window_c_frame_out.brlan"); arrowL.play("menu2_arrow_left_out.brlan"); arrowR.play("menu2_arrow_right_out.brlan");
        playBar.setEnd("menu_play_off.brlan"); modeName.play("menu2_mode_out.brlan"); for (size_t i = 0; i < icons.size(); i++) icons[i]->play(iconAnim(modeIds[i], "out"));
        titleBar.play("menu2_top_out.brlan");
    }

    // ---------- Ajustes (CMenuContentsGameSetting, notes_modes_basic §1.1, notes_vs §1.7) ----------
    // Lista de menu2_sub01 (ítem con ventana de valor text_sub01win) o menu2_sub00 (botón), una entrada por bit de la máscara del modo. ←/→ cambian el valor.
    game::Settings* cfg = nullptr;        // ajustes (los crea main_sdl.cpp); null = X no hace nada
    std::function<void()> onSettingsLeave;  // se llama al salir de Ajustes (main_sdl.cpp guarda el fichero)
    int setMode = 0, setPage = 0, setState = 0;  // modo editado; página 0 = lista del modo, 1 = ventana "Objeto" (14 filas ON/OFF); estado: 0 normal, 1-3 reglas (abriendo/abierta/cerrando), 4-5 aviso (abierto/cerrando)
    bool levelWarned = false;             // aviso #0xcb ya mostrado en esta visita
    struct SetRow { int id; int sub; };   // id = game::SettingId (página 0) o fila 0..13 de objetos (página 1); sub = 0 botón / 1 con valor
    std::vector<SetRow> rows;
    static int normMode(int m) { return m == 25 ? 10 : m; }
    game::ModeSettings& ms() { return cfg->of(normMode(setMode)); }
    static const int* objBits() { static const int B[13] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 17, 14, 15}; return B; }                       // fila 1..13 → bit de la máscara (id-1)
    static const int* objText() { static const int T[13] = {49, 50, 51, 52, 53, 146, 57, 56, 55, 54, 359, 358, 357}; return T; }          // 009_config
    static const int* objHelp() { static const int H[13] = {59, 60, 61, 62, 63, 147, 67, 66, 65, 64, 362, 361, 360}; return H; }
    static int colorOfRow(int id) { switch (id) { case game::S_ENDLESS: return 0x24; case game::S_STARTLEVEL: case game::S_AILEVEL: return 0x25; case game::S_TETRIMINOS: case game::S_MAP: return 0x2a; case game::S_ITEMS: return 0x29; case game::S_PENALTY: return 0x27; case game::S_RULES: return 0x2d; case game::S_GHOST: return 0x2c; case game::S_BG: return 0x26; case game::S_BGM: return 0x28; case game::S_PLAY: return 0x2e; } return 0x25; }
    static bool hasValue(int id) { return id != game::S_ITEMS && id != game::S_RULES && id != game::S_PLAY; }
    bool anyItem() const { return cfg->itemMask != 0; }
    std::u16string onOff(bool v) { return M->get(9, v ? 0x1a : 0x1b); }
    int effBg() { int b = ms().bg; return b >= 0 ? b : game::modeSpec(normMode(setMode)).bg; }
    int effBgm() { int b = ms().bgm; return b != -1 ? b : game::modeSpec(normMode(setMode)).bgm; }
    std::u16string valueText(const SetRow& r) {
        game::ModeSettings& s = ms();
        if (setPage == 1) return onOff(r.id == 0 ? anyItem() : (cfg->itemMask >> objBits()[r.id - 1]) & 1);
        switch (r.id) {
        case game::S_ENDLESS: return onOff(s.endless);
        case game::S_STARTLEVEL: return M->get(9, 0x1c + s.startLevel);
        case game::S_AILEVEL: return M->get(9, 0x1c + s.aiLevel);
        case game::S_TETRIMINOS: return M->get(9, s.tetriminos ? 0x45 : 0x44);
        case game::S_PENALTY: return M->get(9, s.slippery ? 0x30 : 0x2f);
        case game::S_MAP: return onOff(s.map);
        case game::S_GHOST: return onOff(s.ghost);
        case game::S_BG: return num16(effBg() + 1);
        case game::S_BGM: { std::string t = game::Settings::bgmLabel(effBgm()); return std::u16string(t.begin(), t.end()); }
        }
        return std::u16string();
    }
    void rowTexts(const SetRow& r, std::u16string& name, std::u16string& help) {
        if (setPage == 1) { if (r.id == 0) { name = M->get(9, 0xd3); help = M->get(9, 0x3a); } else { name = M->get(9, objText()[r.id - 1]); help = M->get(9, 0x3a); /* ayuda genérica #58: la descripción larga (objHelp) la muestra la ventana del objeto en el original */ } return; }
        struct T { int id, name, help; };
        static const T tb[] = {{game::S_ENDLESS, 1, 0xc}, {game::S_STARTLEVEL, 0xc6, 0xc7}, {game::S_AILEVEL, 2, 0xd}, {game::S_TETRIMINOS, 0xa, 0x15}, {game::S_ITEMS, 3, 0xe}, {game::S_PENALTY, 6, 0x11}, {game::S_MAP, 0x19f, 0x1a0},
                               {game::S_RULES, 0xc8, 0xc9}, {game::S_GHOST, 7, 0x12}, {game::S_BG, 8, 0x13}, {game::S_BGM, 0x62, 0x63}, {game::S_PLAY, 0xb, 0x16}};
        for (const T& t : tb) if (t.id == r.id) { name = M->get(9, t.name); help = M->get(9, t.help); return; }
    }
    void refreshRow(int i) {
        const SetRow& r = rows[i]; std::u16string n, h; rowTexts(r, n, h); items[i]->setText(r.sub ? "text_sub01" : "text_sub00", n);
        if (r.sub) { items[i]->setText("text_sub01win", valueText(r)); items[i]->setTextColor("text_sub01win", 40, 40, 60); }
    }
    void refreshSettingsHelp() { std::u16string n, h; rowTexts(rows[sel], n, h); statusBar.setText("text_comment", h); }
    void buildRows() {
        rows.clear();
        if (setPage == 1) { for (int i = 0; i < 14; i++) rows.push_back({i, 1}); return; }
        uint32_t k = game::Settings::maskOf(normMode(setMode)); int m = normMode(setMode); if (m == 0 || (m >= 5 && m <= 9)) k |= 1u << game::S_PLAY;  // botón "Partida" (bit 0x40000 de las modos de 1 jugador)
        for (int b = 0; b < 19; b++) if (k & (1u << b)) rows.push_back({b, hasValue(b) ? 1 : 0});
    }
    void enterSettings(int mode) {
        screen = Settings; phase = In; setMode = mode; setState = 0; items.clear(); icons.clear(); for (Part* p : {&modeName, &playBar, &winC, &arrowL, &arrowR, &rulesWin, &msgBox}) p->reset();
        buildRows(); itemSub.clear();
        titleBar.init(R, menuArc, cache, "menu2_top.brlyt", 0x104); titleBar.setText("text_mode", M->get(9, setPage == 1 ? 3 : 0)); titleBar.play("menu2_top_in.brlan");
        statusBar.init(R, menuArc, cache, "menu2_back.brlyt", 0x104); statusBar.setText("text_control", M->get(1, 15)); statusBar.play("menu2_back_in.brlan");
        arrowUp.init(R, menuArc, cache, "menu2_arrow.brlyt", 0xff); arrowDown.init(R, menuArc, cache, "menu2_arrow.brlyt", 0xff); arrowUp.shown = arrowDown.shown = false;
        int n = (int)rows.size(); sel = setPage == 1 ? 0 : std::max(0, std::min(setSel, n - 1)); first = std::max(0, std::min(sel - VISIBLE + 1, n - VISIBLE)); if (sel < VISIBLE) first = 0; scrollY = scrollTarget = SPACING * first; prevSel = -1;
        for (int i = 0; i < n; i++) {
            const SetRow& r = rows[i]; auto p = std::make_unique<Part>(); int sub = r.sub ? 1 : 0; itemSub.push_back(sub);
            char ly[32]; snprintf(ly, sizeof ly, "menu2_sub%02d.brlyt", sub); p->init(R, menuArc, cache, ly, 0x10e);
            char b0[32], b1[32]; snprintf(b0, sizeof b0, "sub%02dbar00", sub); snprintf(b1, sizeof b1, "sub%02dbar01", sub); uint8_t c[3]; colorOf(setPage == 1 ? 0x25 : colorOfRow(r.id), c);
            p->setVertexColor(b0, c[0], c[1], c[2]); p->setVertexColor(b1, c[0], c[1], c[2]);
            p->setPos(0, 240 - (BASE_Y + SPACING * (i - first))); char an[48]; snprintf(an, sizeof an, "menu2_sub%02d_in.brlan", sub); p->play(an); p->shown = (i >= first && i < first + VISIBLE);
            items.push_back(std::move(p)); refreshRow(i);
        }
        updateMainSelection(true); refreshSettingsHelp();
    }
    int setSel = 0;  // última fila elegida de la página 0 (al volver de la ventana de objetos)
    void openSetInfo(int textIdx) {
        msgBox.init(R, winArc, cache, "window_a.brlyt", 0xb2); msgBox.setText("text_word_l", M->get(9, textIdx)); msgBox.setText("text_ok", M->get(0, 2));
        for (const char* nm : {"button_yes", "button_no"}) if (lyt::Pane* b = msgBox.L->find(nm)) b->flags &= ~1;
        if (lyt::Pane* ok = msgBox.L->find("button_ok")) ok->flags |= 1; msgBox.play("window_a_l00_on.brlan", "window_a_l00.brlan"); setState = 4; se(19);
    }
    void leaveSettings(int next) {  // next: Mode (atrás) o None con pendingStart (Partida)
        if (onSettingsLeave) onSettingsLeave();
        nextScreen = (Screen)next; phase = Out; outTimer = 0; leaveMain();
        if (next == Mode) titleBar.play("menu2_top_out.brlan");
    }
    void changeValue(int d) {
        const SetRow& r = rows[sel]; game::ModeSettings& s = ms(); auto wrap = [](int v, int n) { return ((v % n) + n) % n; };
        if (setPage == 1) {
            if (r.id == 0) cfg->itemMask = anyItem() ? 0 : game::Settings::allItemsMask();
            else cfg->itemMask ^= 1u << objBits()[r.id - 1];
            for (int i = 0; i < (int)rows.size(); i++) refreshRow(i); return;
        }
        switch (r.id) {
        case game::S_ENDLESS: s.endless = !s.endless; break;
        case game::S_STARTLEVEL: { int o = s.startLevel; s.startLevel = wrap(o + d, 15); if (o == 0 && s.startLevel > 0 && !levelWarned) { levelWarned = true; openSetInfo(0xcb); } break; }  // aviso #0xcb: con NIV inicial > 1 no hay récord
        case game::S_AILEVEL: s.aiLevel = wrap(s.aiLevel + d, 15); break;
        case game::S_TETRIMINOS: s.tetriminos ^= 1; break;
        case game::S_PENALTY: s.slippery = !s.slippery; break;
        case game::S_MAP: s.map = !s.map; break;
        case game::S_GHOST: s.ghost = !s.ghost; break;
        case game::S_BG: s.bg = wrap(effBg() + d, 16); break;
        case game::S_BGM: { int i = wrap(game::Settings::indexOfBgm(effBgm()) + d, game::Settings::bgmCount()); s.bgm = game::Settings::bgmOfIndex(i); break; }
        }
        refreshRow(sel);
    }
    void settingsInput(const Input& in) {
        if (setState == 1) { if (rulesWin.idle()) setState = 2; return; }
        if (setState == 2) { if (in.a || in.b || in.plus) { se(14); rulesWin.play(ruleAnim(ruleType, 2)); setState = 3; } return; }
        if (setState == 3) { if (rulesWin.idle()) { rulesWin.reset(); setState = 0; } return; }
        if (setState == 4 || setState == 5) { if (msgBox.L) { for (const char* nm : {"button_yes", "button_no"}) if (lyt::Pane* bp = msgBox.L->find(nm)) bp->flags &= ~1; if (lyt::Pane* ok = msgBox.L->find("button_ok")) ok->flags |= 1; } }  // la anim reactiva los botones sí/no
        if (setState == 4) { if (!msgBox.idle()) return; if (in.a || in.b) { se(14); msgBox.play("window_a_l00_off.brlan"); setState = 5; } return; }
        if (setState == 5) { if (msgBox.idle()) { msgBox.reset(); setState = 0; } return; }
        const SetRow& r = rows[sel];
        if (in.b || in.plus) {
            se(16);
            if (setPage == 1) { setPage = 0; phase = Out; outTimer = 0; nextScreen = Settings; leaveMain(); return; }  // vuelve a la lista del modo
            leaveSettings(Mode); return;
        }
        if (in.a) {
            if (setPage == 0 && r.id == game::S_ITEMS) { se(14); setSel = sel; setPage = 1; sel = 0; phase = Out; outTimer = 0; nextScreen = Settings; leaveMain(); return; }
            if (setPage == 0 && r.id == game::S_RULES) { se(14); openRules(normMode(setMode)); setState = 1; winC.shown = playBar.shown = modeName.shown = true; return; }
            if (setPage == 0 && r.id == game::S_PLAY) { se(14); pendingStart = setMode; leaveSettings(None); return; }
            if (r.sub) { se(0x15); changeValue(1); return; }
        }
        if (r.sub && (in.left || in.right)) { se(13); changeValue(in.right ? 1 : -1); return; }
        int n = (int)rows.size(), ns = sel; if (in.down) ns = std::min(n - 1, sel + 1); else if (in.up) ns = std::max(0, sel - 1);
        if (ns != sel) { se(13); sel = ns; if (sel >= first + VISIBLE) first = sel - VISIBLE + 1; if (sel < first) first = sel; scrollTarget = SPACING * first; updateMainSelection(false); refreshSettingsHelp(); if (setPage == 0) setSel = sel; }
    }

    // ---------- bucle ----------
    int pendingStart = -1; bool pendingBack = false; float outTimer = 0;
    void update(float dt, const Input& in) {
        bg.update(dt); titleBar.update(dt); statusBar.update(dt); arrowUp.update(dt); arrowDown.update(dt); arrowL.update(dt); arrowR.update(dt); modeName.update(dt); playBar.update(dt); winC.update(dt); rulesWin.update(dt); msgBox.update(dt); pwin.update(dt); pcur.update(dt);
        for (auto& p : items) p->update(dt); for (auto& p : icons) p->update(dt);
        if (screen == Main || screen == Settings) {  // scroll animado (100 ms) y visibilidad por rango
            if (scrollY != scrollTarget) { float step = SPACING * dt / 0.1f; if (fabsf(scrollTarget - scrollY) <= step) scrollY = scrollTarget; else scrollY += (scrollTarget > scrollY ? step : -step); }
            for (int i = 0; i < (int)items.size(); i++) { items[i]->setPos(0, 240 - (BASE_Y + SPACING * i) + scrollY); bool vis = (i >= first && i < first + VISIBLE); if (phase != Out) items[i]->shown = vis; }
        }
        if (phase == In) {
            bool ready = titleBar.idle() && statusBar.idle(); for (auto& p : items) ready = ready && p->idle(); if (screen == Mode) ready = ready && modeName.idle();
            if (ready) { phase = Idle; if (screen == Mode) { modeName.play("menu2_mode_on.brlan"); playBar.setEnd("menu_play_on.brlan"); } }
        } else if (phase == Idle) {
            if (screen == Main) mainInput(in); else if (screen == Mode) modeInput(in); else if (screen == Settings) settingsInput(in);  // AJUSTES
        } else if (phase == Out) {
            outTimer += dt; bool done = statusBar.idle() && titleBar.idle() && outTimer > 0.1f; for (auto& p : items) done = done && p->idle(); for (auto& p : icons) done = done && p->idle();
            if (done) {
                if (pendingStart >= 0) { startMode = pendingStart; pendingStart = -1; screen = None; }
                else if (pendingBack) { backToTitle = true; pendingBack = false; screen = None; }
                else if (nextScreen == Mode) enterMode(listType);
                else if (nextScreen == Main) enterMain(mainSel);
                else if (nextScreen == Settings) enterSettings(setMode);  // AJUSTES
            }
        }
    }
    void draw() {
        std::vector<Part*> v = {&bg, &titleBar, &statusBar, &arrowUp, &arrowDown, &arrowL, &arrowR, &modeName, &playBar, &winC, &rulesWin, &msgBox, &pwin, &pcur};
        for (auto& p : items) v.push_back(p.get()); for (auto& p : icons) v.push_back(p.get());
        std::stable_sort(v.begin(), v.end(), [](Part* a, Part* b) { return a->prio > b->prio; });
        for (Part* p : v) if (p->L) p->draw();
    }
};

} // namespace menu
