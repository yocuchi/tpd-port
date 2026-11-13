// Expresiones del lenguaje de reglas de Tetris Party Deluxe.
// Semántica reconstruida de FUN_800e2a8c (nombres), FUN_800e2e54 (tokens), FUN_800e3ea4 (precedencia)
// y FUN_800e26cc (evaluación). Todo en float de simple precisión, como el original.
#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace rules {

// Quién provee el valor de cada variable. kind:
//  's' scratch d<n> (s9 = result)   'm' mino (idx: n = mino.d<n>, 100 x, 101 y, 102 mark, 103 mark2, 104 fresh, 105 id)
//  'p' phase.d<n>   'u' player (n = pl.d<n>, 100 score, 101 goal, 102 gain)
//  'f' playfield (n = pf.d<n>, 100 level, 101 lines, 102 height, 103 held, 104 cascades)
//  't' tiempo (0 gametime, 1 leveltime, 2 phasetime)   'P' pieza (0 x, 1 y, 2 ghost.y, 3 dir, 4 id, 5 spin)
//  'c' client.d<n>   'x' aleatorio [0,1)   'S' estadísticas del cliente st<k>.d<n> (idx = k*100+n)
struct VarProvider {
    virtual ~VarProvider() {}
    virtual float get(char kind, int idx) = 0;
};

class Expr {
public:
    enum Op : uint8_t { MUL, DIV, IDIV, MOD, POW, ADD, SUB, TERN, EQ, NE, GT, LT, GE, LE, AND, OR, NOT, INT, ABS, NEG, NUM, VAR };
    struct Tok { Op op; float num; char kind; int idx; };

    Expr() {}
    explicit Expr(const std::string& s) { compile(s); }
    bool empty() const { return rpn_.empty(); }
    const std::string& source() const { return src_; }

    void compile(const std::string& src) {
        src_ = src; rpn_.clear();
        std::string s;
        for (char ch : src) { if ((unsigned char)ch > 0x20) s += (char)std::tolower((unsigned char)ch); }
        size_t p = 0;
        parseTernary(s, p);
        if (p < s.size() && s[p] != ';') throw std::runtime_error("expresión: resto sin analizar en '" + src + "'");
    }

    float eval(VarProvider& v) const {
        float st[64]; int sp = 0;
        for (const Tok& t : rpn_) {
            switch (t.op) {
            case NUM: st[sp++] = t.num; break;
            case VAR: st[sp++] = v.get(t.kind, t.idx); break;
            case NEG: st[sp - 1] = -st[sp - 1]; break;
            case NOT: st[sp - 1] = (st[sp - 1] == 0.0f) ? 1.0f : 0.0f; break;
            case INT: st[sp - 1] = (float)(int)st[sp - 1]; break;
            case ABS: if (st[sp - 1] < 0.0f) st[sp - 1] = -st[sp - 1]; break;
            case TERN: { float c = st[sp - 3], a = st[sp - 2], b = st[sp - 1]; sp -= 2; st[sp - 1] = (c == 0.0f) ? b : a; break; }
            default: {
                float b = st[--sp]; float& a = st[sp - 1];
                switch (t.op) {
                case MUL: a = a * b; break;
                case DIV: if (b != 0.0f) a = a / b; break;
                case IDIV: a = (float)(int)(b == 0.0f ? a : a / b); break;
                case MOD: { float q = (float)(int)(b == 0.0f ? a : a / b); a = a - q * b; break; }
                case POW: { int n = (int)b; float base = a, r = a;
                    if (n == 0) r = 1.0f;
                    else if (n > 0) { for (int i = 1; i < n; i++) r = r * base; }
                    else { for (int i = 1; i < -n; i++) r = r * base; r = 1.0f / r; }
                    a = r; break; }
                case ADD: a = a + b; break;
                case SUB: a = a - b; break;
                case EQ: a = (a == b) ? 1.0f : 0.0f; break;
                case NE: a = (a != b) ? 1.0f : 0.0f; break;
                case GT: a = (a > b) ? 1.0f : 0.0f; break;
                case LT: a = (a < b) ? 1.0f : 0.0f; break;
                case GE: a = (a >= b) ? 1.0f : 0.0f; break;
                case LE: a = (a <= b) ? 1.0f : 0.0f; break;
                case AND: a = (a != 0.0f && b != 0.0f) ? 1.0f : 0.0f; break;
                case OR: a = (a != 0.0f || b != 0.0f) ? 1.0f : 0.0f; break;
                default: break;
                }
            }
            }
        }
        return sp ? st[sp - 1] : 0.0f;
    }

private:
    std::string src_;
    std::vector<Tok> rpn_;

    void emit(Op op) { rpn_.push_back({op, 0, 0, 0}); }
    static bool starts(const std::string& s, size_t p, const char* w) { size_t n = std::strlen(w); return s.compare(p, n, w) == 0; }
    static int digits(const std::string& s, size_t& p) { int n = 0; bool any = false; while (p < s.size() && std::isdigit((unsigned char)s[p])) { n = n * 10 + (s[p] - '0'); p++; any = true; } return any ? n : -1; }

    // Nombres (tabla de FUN_800e2a8c, aplicada sobre texto en minúsculas sin espacios).
    bool parseVar(const std::string& s, size_t& p) {
        struct N { const char* name; char kind; int idx; bool num; };
        static const N table[] = {
            {"mino.mark2", 'm', 103, false}, {"mino.mark", 'm', 102, false}, {"mino.fresh", 'm', 104, false}, {"mino.id", 'm', 105, false},
            {"mino.x", 'm', 100, false}, {"mino.y", 'm', 101, false}, {"mino.d", 'm', 0, true},
            {"phase.d", 'p', 0, true}, {"ph.d", 'p', 0, true},
            {"playfield.d", 'f', 0, true}, {"pf.d", 'f', 0, true},
            {"lines", 'f', 101, false}, {"height", 'f', 102, false}, {"held", 'f', 103, false}, {"level", 'f', 100, false}, {"cascades", 'f', 104, false},
            {"player.d", 'u', 0, true}, {"pl.d", 'u', 0, true}, {"score", 'u', 100, false}, {"goal", 'u', 101, false}, {"gain", 'u', 102, false},
            {"gametime", 't', 0, false}, {"leveltime", 't', 1, false}, {"phasetime", 't', 2, false},
            {"piece.x", 'P', 0, false}, {"piece.y", 'P', 1, false}, {"piece.dir", 'P', 3, false}, {"piece.id", 'P', 4, false},
            {"ghost.x", 'P', 0, false}, {"ghost.y", 'P', 2, false}, {"spin", 'P', 5, false}, {"result", 's', 9, false},
            {"client.d", 'c', 0, true}, {"cl.d", 'c', 0, true},
        };
        for (const N& n : table) {
            if (starts(s, p, n.name)) {
                size_t q = p + std::strlen(n.name); int idx = n.idx;
                if (n.num) { idx = digits(s, q); if (idx < 0) continue; }
                rpn_.push_back({VAR, 0, n.kind, idx}); p = q; return true;
            }
        }
        if (starts(s, p, "st")) { size_t q = p + 2; int k = digits(s, q); if (k >= 0 && starts(s, q, ".d")) { q += 2; int n = digits(s, q); if (n >= 0) { rpn_.push_back({VAR, 0, 'S', k * 100 + n, }); p = q; return true; } } }
        if (s[p] == 'd') { size_t q = p + 1; int n = digits(s, q); if (n >= 0) { rpn_.push_back({VAR, 0, 's', n}); p = q; return true; } }
        if (s[p] == 'x' && (p + 1 >= s.size() || !std::isalnum((unsigned char)s[p + 1]))) { rpn_.push_back({VAR, 0, 'x', 0}); p++; return true; }
        return false;
    }

    void parsePrimary(const std::string& s, size_t& p) {
        if (p >= s.size()) throw std::runtime_error("expresión incompleta: '" + src_ + "'");
        char c = s[p];
        if (c == '(') { p++; parseTernary(s, p); if (p >= s.size() || s[p] != ')') throw std::runtime_error("falta ')' en '" + src_ + "'"); p++; return; }
        if (c == '-') { p++; parsePrimary(s, p); emit(NEG); return; }
        if (c == '!') { p++; parsePrimary(s, p); emit(NOT); return; }
        if (c == '#') { p++; parsePrimary(s, p); emit(INT); return; }
        if (c == '@') { p++; parsePrimary(s, p); emit(ABS); return; }
        if (std::isdigit((unsigned char)c) || c == '.') {
            // Misma acumulación que el original: parte entera *10, decimales con pesos .1, .01 ... en float.
            float v = 0.0f; bool frac = false; float w = 0.1f;
            while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p] == '.')) {
                if (s[p] == '.') { frac = true; w = 0.1f; }
                else if (frac) { v = v + (float)((s[p] - '0') * w); w = w * 0.1f; }
                else v = v * 10.0f + (float)(s[p] - '0');
                p++;
            }
            rpn_.push_back({NUM, v, 0, 0}); return;
        }
        if (!parseVar(s, p)) throw std::runtime_error(std::string("símbolo desconocido '") + c + "' en '" + src_ + "'");
    }
    // Misma precedencia que FUN_800e3ea4: * / \ % ^ (iguales, izq→der) < + - < comparaciones < = != < & | < ?
    void parseMul(const std::string& s, size_t& p) {
        parsePrimary(s, p);
        while (p < s.size()) {
            char c = s[p]; Op op;
            if (c == '*') op = MUL; else if (c == '/') op = DIV; else if (c == '\\') op = IDIV; else if (c == '%') op = MOD; else if (c == '^') op = POW; else break;
            p++; parsePrimary(s, p); emit(op);
        }
    }
    void parseAdd(const std::string& s, size_t& p) {
        parseMul(s, p);
        while (p < s.size() && (s[p] == '+' || s[p] == '-')) { Op op = s[p] == '+' ? ADD : SUB; p++; parseMul(s, p); emit(op); }
    }
    void parseRel(const std::string& s, size_t& p) {
        parseAdd(s, p);
        while (p < s.size() && (s[p] == '<' || s[p] == '>')) {
            Op op; if (s[p] == '<') { if (p + 1 < s.size() && s[p + 1] == '=') { op = LE; p += 2; } else { op = LT; p++; } }
            else { if (p + 1 < s.size() && s[p + 1] == '=') { op = GE; p += 2; } else { op = GT; p++; } }
            parseAdd(s, p); emit(op);
        }
    }
    void parseEq(const std::string& s, size_t& p) {
        parseRel(s, p);
        while (p < s.size() && (s[p] == '=' || (s[p] == '!' && p + 1 < s.size() && s[p + 1] == '='))) {
            Op op = s[p] == '=' ? EQ : NE; p += (op == EQ) ? 1 : 2; parseRel(s, p); emit(op);
        }
    }
    void parseLogic(const std::string& s, size_t& p) {
        parseEq(s, p);
        while (p < s.size() && (s[p] == '&' || s[p] == '|')) { Op op = s[p] == '&' ? AND : OR; p++; parseEq(s, p); emit(op); }
    }
    void parseTernary(const std::string& s, size_t& p) {
        parseLogic(s, p);
        if (p < s.size() && s[p] == '?') { p++; parseLogic(s, p); if (p < s.size() && s[p] == ':') p++; parseLogic(s, p); emit(TERN); }
    }
};

} // namespace rules
