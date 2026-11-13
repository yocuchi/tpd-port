// Lector del formato TPF2 (reglas de un modo de Tetris Party Deluxe).
// Port directo de tpf2.py; cada campo sigue el orden en que lo lee el juego (FUN_800e8348 y parsers de chunk).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <array>

namespace tpf {

struct Reader {
    const uint8_t* d; size_t n; size_t p = 0;
    Reader(const uint8_t* data, size_t len) : d(data), n(len) {}
    void need(size_t k) { if (p + k > n) throw std::runtime_error("TPF2 truncado"); }
    uint8_t u8() { need(1); return d[p++]; }
    int8_t s8() { return (int8_t)u8(); }
    uint16_t u16() { need(2); uint16_t v = (d[p] << 8) | d[p + 1]; p += 2; return v; }
    uint32_t u32() { need(4); uint32_t v = ((uint32_t)d[p] << 24) | (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3]; p += 4; return v; }
    float f32() { uint32_t v = u32(); float f; static_assert(sizeof f == 4, ""); __builtin_memcpy(&f, &v, 4); return f; }
    bool b() { return u8() != 0; }
    std::string str8() { size_t k = u8(); need(k); std::string s((const char*)d + p, k); p += k; return s; }
    std::string str16() { size_t k = u16(); need(k); std::string s((const char*)d + p, k); p += k; return s; }
    char tag() { return (char)u8(); }
    std::string id4() { need(4); std::string s((const char*)d + p, 4); p += 4; return s; }
    uint32_t mask32() { uint32_t m = 0; for (int i = 0; i < 32; i++) if (b()) m |= 1u << i; return m; }
};

struct Entry { std::string name; uint32_t id = 0; };
static inline Entry entryHead(Reader& r) { uint8_t sv = r.u8(); if (sv > 1) r.u32(); Entry e; e.name = r.str8(); e.id = r.u32(); return e; }

struct Data { uint16_t ver; int8_t a; bool bflag; std::string gamesec, levelsec; int8_t c; bool hasImage = false; std::string image, start, topout; uint16_t d; };
struct Field { uint16_t ver; int width, height; /* el juego usa height*2 filas (zona oculta encima) */ bool flag; uint32_t g1; std::string garbage1_count; uint32_t g2; std::string garbage2_count; };
struct PieceEvents { uint16_t ver; std::array<std::string, 12> s; };  // generated, move-ok, move-bad, rotate-ok, rotate-bad, held, soft-drop, soft-step, hard-drop, hard-step, touchdown, ev12
struct Controls { uint16_t ver; float f[4]; std::string enable_hold, enable_hard_drop; };
struct Mino : Entry { uint32_t mask; float f[10]; std::string image, locked_image, dying_image; bool flag; uint32_t u = 0; uint8_t b8 = 0; uint32_t v; };
struct MinoChunk { uint16_t ver; bool b1, b2; uint8_t grid[32][32]; std::vector<Mino> items; };
struct Tetrimino : Entry { uint8_t flag; uint32_t grid[7][7]; int marks; uint8_t markx[4][16], marky[4][16], markz[4][16]; };
struct Pattern : Entry { bool flag; uint32_t mask; std::string begin, permino, end; uint8_t n; bool recursive; int8_t dir = 0; std::string min_count; uint32_t shape_id = 0; };
struct Level : Entry { uint32_t u; std::string begin, end; };
struct Levels { uint16_t ver; std::string start, end; std::vector<Level> items; };
struct Event : Entry { std::string script; uint8_t b8; std::string exec_time; };
struct AnimTrack { std::vector<uint8_t> vals; uint8_t type; std::string image; };
struct Anim : Entry { uint8_t n; std::array<AnimTrack, 3> tracks; };
struct LayoutCell { uint32_t mino, u; int8_t flags[24]; };
struct LayoutPiece { uint32_t tetr; uint32_t grid[49]; };
struct Layout : Entry { uint32_t w, h; std::vector<LayoutCell> cells; std::vector<LayoutPiece> pieces; };
struct Sequence : Entry { std::vector<uint32_t> ids; };
struct Named : Entry { std::string file; };
struct Shape : Entry { bool grid[16][16]; uint8_t a, b8; };
struct Trash : Entry { int w, h; std::vector<uint32_t> cells; };

struct Phase { std::string begin, end; uint16_t ver; };
struct PGen : Phase { std::string delay, start_x, start_y; uint8_t b8; };
struct PFall : Phase { std::string fall_speed, soft_speed, tolerance, hard_speed; };
struct PLock : Phase { std::string delay; uint8_t type; std::string extended_count; };
struct PSolid : Phase { bool flag = false; uint32_t mask = 0; std::string minoiterate; };
struct PIterItem { bool flag; uint32_t mask; std::string minoiterate; bool run; std::string run_exp; };
struct PIter : Phase { PIterItem it[2]; };
struct PAnim : Phase { uint32_t anim; };
struct PElim : Phase { std::string collapse, fall_speed, fall; bool flag; std::string land; };
struct PGrav : Phase { bool flag; std::string fall_speed, land; };

struct Rules {
    Data data; Field fldd; PieceEvents pcev; Controls ctrl; MinoChunk mino;
    std::vector<Tetrimino> tetr; std::vector<Shape> shap; std::vector<Pattern> ptrn;
    std::vector<Sequence> seqs; std::vector<Named> snds, song; std::vector<Trash> trsh;
    Levels lvls; std::vector<Event> evnt; std::vector<Anim> anim; std::vector<Layout> plyt;
    PGen pgen; PFall pfal; PLock plok; Phase ppat; PIter ptr8; PAnim panm; PElim pelm; PSolid psol; PGrav pgvt; Phase pcmp;
    size_t bytesUsed = 0;
};

namespace detail {
template <class T, class F> void listChunk(Reader& r, char tagc, std::vector<T>& out, F fn) {
    r.u8(); r.u32(); uint16_t ver = r.u16();
    for (;;) { char t = r.tag(); if (t == 'x') break; if (t != tagc) throw std::runtime_error(std::string("tag inesperado ") + t); out.push_back(fn(r, ver)); }
}
inline void phaseBase(Reader& r, Phase& p) { r.u8(); p.begin = r.str16(); p.end = r.str16(); p.ver = r.u16(); }
inline Pattern patternBase(Reader& r) {
    Pattern p; (Entry&)p = entryHead(r); p.flag = r.b(); p.mask = r.mask32();
    p.begin = r.str16(); p.permino = r.str16(); p.end = r.str16(); p.n = r.u8(); return p;
}
}

inline Rules parse(const uint8_t* data, size_t len) {
    using namespace detail;
    Reader r(data, len); Rules R;
    if (r.id4() != "tpf2") throw std::runtime_error("no es TPF2");
    for (;;) {
        std::string id = r.id4();
        if (id == "xxxx") break;
        else if (id == "data") { Data& d = R.data; d.ver = r.u16(); d.a = r.s8(); d.bflag = r.b(); d.gamesec = r.str16(); d.levelsec = r.str16(); d.c = r.s8();
            if ((d.hasImage = r.b())) d.image = r.str8(); d.start = r.str16(); d.topout = r.str16(); d.d = r.u16(); }
        else if (id == "fldd") { Field& f = R.fldd; f.ver = r.u16(); f.width = r.s8(); f.height = r.s8(); f.flag = r.b(); f.g1 = r.u32(); f.garbage1_count = r.str8(); f.g2 = r.u32(); f.garbage2_count = r.str8(); }
        else if (id == "pcev") { R.pcev.ver = r.u16(); for (auto& s : R.pcev.s) s = r.str16(); }
        else if (id == "ctrl") { Controls& c = R.ctrl; c.ver = r.u16(); for (float& f : c.f) f = r.f32(); if (c.ver >= 3) { c.enable_hold = r.str8(); c.enable_hard_drop = r.str8(); } }
        else if (id == "mino") { MinoChunk& m = R.mino; r.u8(); r.u32(); m.ver = r.u16(); m.b1 = r.b(); m.b2 = r.b();
            for (auto& row : m.grid) for (auto& c : row) c = r.u8();
            for (;;) { char t = r.tag(); if (t == 'x') break; if (t != 'm') throw std::runtime_error("mino tag");
                Mino e; (Entry&)e = entryHead(r); e.mask = r.mask32(); for (float& f : e.f) f = r.f32(); e.image = r.str8();
                if (r.b()) e.locked_image = r.str8(); if (r.b()) e.dying_image = r.str8(); e.flag = r.b();
                if (m.ver >= 2) { e.u = r.u32(); e.b8 = r.u8(); } e.v = r.u32(); m.items.push_back(e); } }
        else if (id == "tetr") listChunk(r, 't', R.tetr, [](Reader& r, uint16_t ver) { Tetrimino t; (Entry&)t = entryHead(r); t.flag = r.u8();
            for (auto& row : t.grid) for (auto& c : row) c = r.u32(); t.marks = ver < 3 ? 5 : r.u8();
            for (int d = 0; d < 4; d++) for (int i = 0; i < t.marks; i++) { t.markx[d][i] = r.u8(); t.marky[d][i] = r.u8(); }
            for (int d = 0; d < 4; d++) for (int i = 0; i < t.marks; i++) t.markz[d][i] = r.u8(); return t; });
        else if (id == "shap") listChunk(r, 's', R.shap, [](Reader& r, uint16_t) { Shape s; (Entry&)s = entryHead(r); for (auto& row : s.grid) for (bool& c : row) c = r.b(); s.a = r.u8(); s.b8 = r.u8(); return s; });
        else if (id == "ptrn") { r.u8(); r.u32(); r.u16();
            for (;;) { char t = r.tag(); if (t == 'x') break; Pattern p = patternBase(r);
                if (t == 'r') { p.recursive = true; p.dir = r.s8(); p.min_count = r.str8(); }
                else if (t == 's') { p.recursive = false; p.shape_id = r.u32(); }
                else throw std::runtime_error("ptrn tag"); R.ptrn.push_back(p); } }
        else if (id == "seqs") listChunk(r, 's', R.seqs, [](Reader& r, uint16_t) { Sequence s; (Entry&)s = entryHead(r); int n = r.s8(); for (int i = 0; i < n; i++) s.ids.push_back(r.u32()); return s; });
        else if (id == "snds") listChunk(r, 's', R.snds, [](Reader& r, uint16_t) { Named s; (Entry&)s = entryHead(r); s.file = r.str8(); return s; });
        else if (id == "song") listChunk(r, 'm', R.song, [](Reader& r, uint16_t) { Named s; (Entry&)s = entryHead(r); s.file = r.str8(); return s; });
        else if (id == "trsh") listChunk(r, 't', R.trsh, [](Reader& r, uint16_t) { Trash t; (Entry&)t = entryHead(r); t.w = r.s8(); t.h = r.s8(); for (int i = 0; i < t.w * t.h; i++) t.cells.push_back(r.u32()); return t; });
        else if (id == "lvls") { Levels& l = R.lvls; r.u8(); r.u32(); l.ver = r.u16(); if (l.ver > 1) { l.start = r.str16(); l.end = r.str16(); }
            for (;;) { char t = r.tag(); if (t == 'x') break; if (t != 'l') throw std::runtime_error("lvls tag"); Level e; (Entry&)e = entryHead(r); e.u = r.u32(); e.begin = r.str16(); e.end = r.str16(); l.items.push_back(e); } }
        else if (id == "evnt") listChunk(r, 'e', R.evnt, [](Reader& r, uint16_t) { Event e; (Entry&)e = entryHead(r); e.script = r.str16(); e.b8 = r.u8(); e.exec_time = r.str8(); return e; });
        else if (id == "anim") listChunk(r, 'a', R.anim, [](Reader& r, uint16_t) { Anim a; (Entry&)a = entryHead(r); a.n = r.u8();
            for (auto& t : a.tracks) { for (int i = 0; i < a.n; i++) t.vals.push_back(r.u8()); t.type = r.u8(); if (t.type == 3) t.image = r.str8(); } return a; });
        else if (id == "plyt") listChunk(r, 'L', R.plyt, [](Reader& r, uint16_t) { Layout L; (Entry&)L = entryHead(r); L.w = r.u32(); L.h = r.u32();
            for (uint32_t i = 0; i < L.w * L.h; i++) { LayoutCell c; c.mino = r.u32(); c.u = r.u32(); for (auto& f : c.flags) f = r.s8(); L.cells.push_back(c); }
            uint32_t np = r.u32(); for (uint32_t i = 0; i < np; i++) { LayoutPiece p; p.tetr = r.u32(); for (auto& g : p.grid) g = r.u32(); L.pieces.push_back(p); } return L; });
        else if (id == "pgen") { PGen& p = R.pgen; phaseBase(r, p); p.delay = r.str8(); p.start_x = r.str8(); p.start_y = r.str8(); p.b8 = r.u8(); }
        else if (id == "pfal") { PFall& p = R.pfal; phaseBase(r, p); p.fall_speed = r.str8(); p.soft_speed = r.str8(); p.tolerance = r.str8(); p.hard_speed = r.str8(); }
        else if (id == "plok") { PLock& p = R.plok; phaseBase(r, p); p.delay = r.str8(); if (p.ver < 2) p.type = r.b() ? 2 : 0; else { p.type = r.u8(); if (p.type == 1) p.extended_count = r.str8(); } }
        else if (id == "psol") { PSolid& p = R.psol; phaseBase(r, p); if (p.ver >= 2) { p.flag = r.b(); p.mask = r.mask32(); p.minoiterate = r.str16(); } }
        else if (id == "ptr8") { PIter& p = R.ptr8; phaseBase(r, p); for (auto& it : p.it) { it.flag = r.b(); it.mask = r.mask32(); it.minoiterate = r.str16(); it.run = r.b(); if (it.run) it.run_exp = r.str8(); } }
        else if (id == "panm") { PAnim& p = R.panm; phaseBase(r, p); p.anim = r.u32(); }
        else if (id == "pelm") { PElim& p = R.pelm; phaseBase(r, p); p.collapse = r.str16(); p.fall_speed = r.str8(); p.fall = r.str16(); p.flag = r.b(); p.land = r.str16(); }
        else if (id == "pgvt") { PGrav& p = R.pgvt; phaseBase(r, p); p.flag = r.b(); p.fall_speed = r.str8(); p.land = r.str16(); }
        else if (id == "pcmp") phaseBase(r, R.pcmp);
        else if (id == "ppat") phaseBase(r, R.ppat);
        else throw std::runtime_error("chunk desconocido " + id);
    }
    R.bytesUsed = r.p;
    return R;
}

} // namespace tpf
