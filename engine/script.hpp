// Scripts de reglas de Tetris Party Deluxe (compilador FUN_800d079c + normalizador FUN_800d068c
// + ejecutor FUN_800cf814). Los opcodes son los del ejecutor original.
#pragma once
#include "expr.hpp"
#include <string>
#include <vector>
#include <cstring>

namespace rules {

enum Cmd : int {
    NOP = 0, EXIT = 1, IF = 2, ELSE = 3, END = 4,
    SET_D = 5, SET_PH = 6, SET_PF = 7, SET_PL = 8, SET_CL = 9, SET_GOAL = 10,
    WIN = 0xb, LOSE = 0xc, NEXT_LEVEL = 0xd,
    SET_MINO_USING_STICKY = 0xe, SET_MINO_USING = 0xf, SET_MINO = 0x10,
    KILL_USING_STICKY = 0x11, KILL_MINO_USING = 0x12, KILL_MINO = 0x13,
    MARK_USING_STICKY = 0x14, MARK_MINO_USING = 0x15, MARK_MINO = 0x16,
    MARK2_USING_STICKY = 0x17, MARK2_MINO_USING = 0x18, MARK2_MINO = 0x19,
    MUTATE_MINO = 0x1a, MARK_RESET = 0x1b, MARK2_RESET = 0x1c, SCORE_RESET = 0x1d, SCORE = 0x1e,
    SOUND = 0x1f, LOOP_SOUND = 0x20, MUSIC = 0x21, LOOP_MUSIC = 0x22, MUSIC_SPEED = 0x23,
    SEQ_INSERT = 0x24, SEQ_FULL_INSERT = 0x25, SEQ_PICK_INSERT = 0x26, SEQUENCE = 0x27, SEQ_FULL = 0x28, SEQ_PICK = 0x29,
    SET_BAG = 0x2a, PATTERN = 0x2b, IPATTERN = 0x2c, SHAPE = 0x2d, TRASH = 0x2e,
    MIX_GARBAGE = 0x2f, GARBAGE = 0x30, MIXED_GARBAGE = 0x31, SET_GHOST = 0x32, EVENT = 0x33, SEND = 0x34, ITERATE = 0x35,
    SHIFT_LEFT = 0x36, SHIFT_RIGHT = 0x37, SHIFT_UP = 0x38, SHIFT_DOWN = 0x39,
    ROTATE_LEFT = 0x3a, ROTATE_RIGHT = 0x3b, ROTATE_UP = 0x3c, ROTATE_DOWN = 0x3d,
    SET_PATTERN = 0x3e, SET_GRAVITY = 0x3f, HOLD = 0x40, HOLD_RESET = 0x41, TOUCH = 0x42, SOLIDIFY = 0x43, ANIMATE = 0x44,
    CLIENT = 0x45, LAYOUT = 0x46,
};

struct Line {
    Cmd cmd = NOP;
    int idx = 0;              // índice d# / nº de expresiones en SEND
    std::string name;         // argumento entre comillas, o línea cruda para CLIENT
    Expr e[2];                // hasta dos expresiones
};

// Lo que el motor ofrece al script. Las operaciones de tablero reciben el contexto (mino actual, etc.) aparte.
struct ScriptHost : VarProvider {
    virtual void setVar(char kind, int idx, float v) = 0;       // 's' 'p' 'f' 'u' 'c'
    virtual void setGoal(float v) = 0;
    virtual void score(float v) = 0;                           // suma a score y gain
    virtual void scoreReset() = 0;
    virtual void win() = 0; virtual void lose() = 0; virtual void nextLevel() = 0;
    virtual void clientLine(const std::string& raw) = 0;        // líneas que empiezan por '!'
    // Resto de comandos: el motor los despacha por opcode.
    virtual void command(Cmd c, const std::string& name, float a, float b, int nargs) = 0;
};

class Script {
public:
    Script() {}
    explicit Script(const std::string& src) { compile(src); }
    bool empty() const { return lines_.empty(); }
    const std::vector<Line>& lines() const { return lines_; }

    // Normalización de FUN_800d068c: minúsculas, '*' = comentario, se quitan espacios salvo
    // entre comillas o en líneas '!', separador de línea '\r'.
    static std::vector<std::string> normalize(const std::string& src) {
        std::vector<std::string> out; std::string cur; bool comment = false, keep = false; int col = 0;
        auto flush = [&]() { if (!cur.empty()) out.push_back(cur); cur.clear(); col = 0; keep = false; comment = false; };
        for (size_t i = 0; i <= src.size(); i++) {
            unsigned char c = i < src.size() ? (unsigned char)src[i] : 0;
            if (c >= 'A' && c <= 'Z') c += 0x20;
            if (c == '*' && col == 0) comment = true;
            if (c == 0 || c == '\r' || c == '\n') { if (!comment) flush(); else { cur.clear(); col = 0; keep = false; comment = false; } if (c == 0) break; continue; }
            if (comment) continue;
            if (c < 0x20) continue;
            if (c == '!' && col == 0) keep = true;
            if (c == '"') keep = !keep;
            if (!keep && c == ' ') continue;
            cur += (char)c; col++;
        }
        return out;
    }

    void compile(const std::string& src) {
        lines_.clear();
        for (const std::string& ln : normalize(src)) {
            Line L;
            if (ln[0] == '!') { L.cmd = CLIENT; L.name = ln.substr(1); lines_.push_back(L); continue; }
            const char* s = ln.c_str();
            size_t p = 0;
            if (!match(ln, p, L)) throw std::runtime_error("script: comando desconocido: '" + ln + "'");
            (void)s;
            lines_.push_back(L);
        }
    }

    void run(ScriptHost& h) const {
        for (size_t i = 0; i < lines_.size(); i++) {
            const Line& L = lines_[i];
            switch (L.cmd) {
            case NOP: break;
            case EXIT: return;
            case IF:
                if (L.e[0].eval(h) == 0.0f) {       // saltar al else/end del mismo nivel
                    int depth = 1;
                    for (i++; i < lines_.size(); i++) {
                        Cmd c = lines_[i].cmd;
                        if (c == IF) depth++;
                        else if (c == ELSE && depth == 1) break;
                        else if (c == END) { if (--depth < 1) break; }
                    }
                }
                break;
            case ELSE: {                           // llegamos aquí tras ejecutar la rama if: saltar al end
                int depth = 1;
                for (i++; i < lines_.size(); i++) { Cmd c = lines_[i].cmd; if (c == IF) depth++; else if (c == END && --depth < 1) break; }
                break; }
            case END: break;
            case SET_D: h.setVar('s', L.idx, L.e[0].eval(h)); break;
            case SET_PH: h.setVar('p', L.idx, L.e[0].eval(h)); break;
            case SET_PF: h.setVar('f', L.idx, L.e[0].eval(h)); break;
            case SET_PL: h.setVar('u', L.idx, L.e[0].eval(h)); break;
            case SET_CL: h.setVar('c', L.idx, L.e[0].eval(h)); break;
            case SET_GOAL: h.setGoal(L.e[0].eval(h)); break;
            case WIN: h.win(); break;
            case LOSE: h.lose(); break;
            case NEXT_LEVEL: h.nextLevel(); break;
            case SCORE_RESET: h.scoreReset(); break;
            case SCORE: h.score(L.e[0].eval(h)); break;
            case CLIENT: h.clientLine(L.name); break;
            default: {
                float a = L.e[0].empty() ? 0.0f : L.e[0].eval(h);
                float b = L.e[1].empty() ? 0.0f : L.e[1].eval(h);
                h.command(L.cmd, L.name, a, b, L.idx);
                break; }
            }
        }
    }

private:
    std::vector<Line> lines_;

    struct Pat { const char* text; Cmd cmd; int args; };  // args: 0 nada, 1 expr, 2 "nombre", 3 "nombre",expr, 4 "nombre",expr,expr opcional (send), 5 "nombre",x,y
    static bool kw(const std::string& s, size_t& p, const char* w) { size_t n = std::strlen(w); if (s.compare(p, n, w) == 0) { p += n; return true; } return false; }

    bool match(const std::string& s, size_t& p, Line& L) {
        static const Pat pats[] = {
            {"exit", EXIT, 0}, {"if", IF, 1}, {"else", ELSE, 0}, {"end", END, 0},
            {"setgoalto", SET_GOAL, 1}, {"win", WIN, 0}, {"lose", LOSE, 0}, {"nextlevel", NEXT_LEVEL, 0},
            {"setminousingstickyto", SET_MINO_USING_STICKY, 0}, {"setminousing", SET_MINO_USING, 0}, {"setminoto", SET_MINO, 0},
            {"killusingsticky", KILL_USING_STICKY, 0}, {"killminousing", KILL_MINO_USING, 0}, {"killmino", KILL_MINO, 0},
            {"markusingsticky", MARK_USING_STICKY, 0}, {"markminousing", MARK_MINO_USING, 0}, {"markmino", MARK_MINO, 0},
            {"mark2usingsticky", MARK2_USING_STICKY, 0}, {"mark2minousing", MARK2_MINO_USING, 0}, {"mark2mino", MARK2_MINO, 0},
            {"markreset", MARK_RESET, 0}, {"mark2reset", MARK2_RESET, 0}, {"mutatemino", MUTATE_MINO, 0},
            {"scorereset", SCORE_RESET, 0}, {"score", SCORE, 1},
            {"sound", SOUND, 2}, {"loopsound", LOOP_SOUND, 3}, {"musicspeed", MUSIC_SPEED, 1}, {"music", MUSIC, 2}, {"loopmusic", LOOP_MUSIC, 2},
            {"sequenceinsert", SEQ_INSERT, 3}, {"seqfullinsert", SEQ_FULL_INSERT, 3}, {"seqpickinsert", SEQ_PICK_INSERT, 3},
            {"sequence", SEQUENCE, 3}, {"seqfull", SEQ_FULL, 3}, {"seqpick", SEQ_PICK, 2},
            {"setbag", SET_BAG, 1}, {"resetbag", SET_BAG, 0}, {"shuffle", SET_BAG, 0},
            {"pattern", PATTERN, 5}, {"ipattern", IPATTERN, 5}, {"shape", SHAPE, 5}, {"trash", TRASH, 2}, {"layout", LAYOUT, 2},
            {"mixgarbage", MIX_GARBAGE, 0}, {"garbage", GARBAGE, 1}, {"mixedgarbage", MIXED_GARBAGE, 1},
            {"setghostto", SET_GHOST, 1}, {"event", EVENT, 2}, {"send", SEND, 4}, {"iterate", ITERATE, 0},
            {"shiftleft", SHIFT_LEFT, 0}, {"shiftright", SHIFT_RIGHT, 0}, {"shiftup", SHIFT_UP, 0}, {"shiftdown", SHIFT_DOWN, 0},
            {"rotateleft", ROTATE_LEFT, 0}, {"rotateright", ROTATE_RIGHT, 0}, {"rotateup", ROTATE_UP, 0}, {"rotatedown", ROTATE_DOWN, 0},
            {"setpatternto", SET_PATTERN, 1}, {"setgravityto", SET_GRAVITY, 1},
            {"holdreset", HOLD_RESET, 0}, {"hold", HOLD, 0}, {"touch", TOUCH, 0}, {"solidify", SOLIDIFY, 0}, {"animate", ANIMATE, 2},
        };
        // set X.d# to <expr>
        if (kw(s, p, "set")) {
            size_t q = p; char kind = 0; Cmd c = NOP;
            if (kw(s, q, "d")) { kind = 's'; c = SET_D; }
            else if (kw(s, q, "phase.d") || kw(s, q, "ph.d")) { kind = 'p'; c = SET_PH; }
            else if (kw(s, q, "playfield.d") || kw(s, q, "pf.d")) { kind = 'f'; c = SET_PF; }
            else if (kw(s, q, "player.d") || kw(s, q, "pl.d")) { kind = 'u'; c = SET_PL; }
            else if (kw(s, q, "client.d") || kw(s, q, "cl.d")) { kind = 'c'; c = SET_CL; }
            if (kind) {
                int n = 0; bool any = false; while (q < s.size() && std::isdigit((unsigned char)s[q])) { n = n * 10 + (s[q] - '0'); q++; any = true; }
                if (any && kw(s, q, "to")) { L.cmd = c; L.idx = n; L.e[0].compile(s.substr(q)); p = s.size(); return true; }
            }
            p -= 3;
        }
        for (const Pat& pt : pats) {
            size_t q = p;
            if (!kw(s, q, pt.text)) continue;
            L.cmd = pt.cmd; L.idx = 0;
            std::string rest = s.substr(q);
            switch (pt.args) {
            case 0: break;
            case 1: if (!rest.empty()) L.e[0].compile(rest); break;
            case 2: L.name = quoted(rest); break;
            case 3: case 4: case 5: {
                size_t r = 0; L.name = quoted(rest, &r);
                int n = 0;
                while (n < 2 && r < rest.size() && rest[r] == ',') { r++; size_t e = rest.find(',', r); std::string ex = rest.substr(r, e == std::string::npos ? std::string::npos : e - r); L.e[n++].compile(ex); r = (e == std::string::npos) ? rest.size() : e; }
                L.idx = n; break; }
            }
            p = s.size(); return true;
        }
        return false;
    }
    static std::string quoted(const std::string& s, size_t* endp = nullptr) {
        size_t a = s.find('"'); if (a == std::string::npos) { if (endp) *endp = 0; return s; }
        size_t b = s.find('"', a + 1); if (b == std::string::npos) b = s.size();
        if (endp) *endp = b + 1 <= s.size() ? b + 1 : s.size();
        return s.substr(a + 1, b - a - 1);
    }
};

} // namespace rules
