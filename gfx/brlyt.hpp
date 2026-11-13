// Layouts NW4R (BRLYT v0x0a) de Wii.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <cstring>
#include "tpl.hpp"

namespace lyt {

struct TexMap { uint16_t tex; uint8_t wrapS, wrapT; };
struct TexSRT { float tx, ty, rot, sx, sy; };
struct Material {
    std::string name; int16_t fore[4], back[4], colreg[4]; uint8_t kcol[4][4]; uint32_t flags;
    std::vector<TexMap> texmaps; std::vector<TexSRT> texsrt; int blend = 0; int alphaCompare = 0;
};
struct UserData { std::string name; int type; std::vector<int> ints; std::vector<float> floats; std::string str; };
struct Pane {
    std::string type;  // pan1 pic1 txt1 wnd1 bnd1
    std::string name; uint8_t flags = 1, origin = 4, alpha = 255;
    float x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, w = 0, h = 0;
    // pic1 / wnd1 / txt1
    uint8_t vcol[4][4]; int material = -1; std::vector<std::array<float, 8>> uv;
    // txt1
    std::u16string text; int font = -1; uint8_t fontSize = 0; float fsx = 0, fsy = 0, charSpace = 0, lineSpace = 0; uint8_t align = 0; uint8_t tcol[2][4];
    // wnd1
    float inset[4] = {0, 0, 0, 0}; std::vector<std::pair<int, uint8_t>> frames;  // (material, flip)
    std::vector<UserData> userdata;
    std::vector<Pane*> children; Pane* parent = nullptr;
    // estado de animación
    bool visible() const { return flags & 1; }
    const UserData* ud(const std::string& n) const { for (auto& u : userdata) if (u.name == n) return &u; return nullptr; }
    float udf(const std::string& n, float def = 0) const { const UserData* u = ud(n); if (!u) return def; if (u->type == 2 && !u->floats.empty()) return u->floats[0]; if (u->type == 1 && !u->ints.empty()) return (float)u->ints[0]; return def; }
};
struct Group { std::string name; std::vector<std::string> panes; };

struct Layout {
    float width = 640, height = 480; bool centered = true;
    std::vector<std::string> textures, fonts; std::vector<Material> materials;
    std::vector<void*> boundTex;  // texturas resueltas por arc (Renderer::bindTextures) para evitar colisiones de nombre entre arcs
    std::vector<Pane*> panes; Pane* root = nullptr; std::vector<Group> groups;
    ~Layout() { for (Pane* p : panes) delete p; }
    Pane* find(const std::string& n) const { for (Pane* p : panes) if (p->name == n) return p; return nullptr; }
    int materialIndex(const std::string& n) const { for (size_t i = 0; i < materials.size(); i++) if (materials[i].name == n) return (int)i; return -1; }

    bool parse(const uint8_t* d, size_t n) {
        using gx::be16; using gx::be32;
        if (n < 16 || memcmp(d, "RLYT", 4)) return false;
        uint16_t hdr = be16(d + 12), nsec = be16(d + 14); size_t off = hdr;
        std::vector<Pane*> stack; Pane* last = nullptr;
        for (int s = 0; s < nsec && off + 8 <= n; s++) {
            std::string tag((const char*)d + off, 4); uint32_t ln = be32(d + off + 4); const uint8_t* b = d + off + 8; size_t bl = ln - 8;
            if (tag == "lyt1") { centered = b[0] != 0; memcpy(&width, b + 4, 4); memcpy(&height, b + 8, 4); width = bef(b + 4); height = bef(b + 8); }
            else if (tag == "txl1" || tag == "fnl1") { int cnt = be16(b); auto& v = tag == "txl1" ? textures : fonts; for (int k = 0; k < cnt; k++) { uint32_t o = be32(b + 4 + k * 8) + 4; v.emplace_back((const char*)b + o); } }
            else if (tag == "mat1") {
                int cnt = be16(b);
                for (int k = 0; k < cnt; k++) {
                    uint32_t o = be32(b + 4 + k * 4) - 8; const uint8_t* m = b + o; Material M;
                    M.name.assign((const char*)m, strnlen((const char*)m, 20));
                    for (int i = 0; i < 4; i++) { M.fore[i] = (int16_t)be16(m + 20 + i * 2); M.back[i] = (int16_t)be16(m + 28 + i * 2); M.colreg[i] = (int16_t)be16(m + 36 + i * 2); }
                    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) M.kcol[i][j] = m[44 + i * 4 + j];
                    M.flags = be32(m + 60); const uint8_t* q = m + 64;
                    int ntex = M.flags & 15, nsrt = (M.flags >> 4) & 15, ncoord = (M.flags >> 8) & 15, ntevswap = (M.flags >> 12) & 1, nind = (M.flags >> 13) & 3, nindst = (M.flags >> 15) & 7, ntev = (M.flags >> 18) & 31, nalpha = (M.flags >> 23) & 1, nblend = (M.flags >> 24) & 1;
                    for (int i = 0; i < ntex; i++) { M.texmaps.push_back({be16(q), q[2], q[3]}); q += 4; }
                    for (int i = 0; i < nsrt; i++) { M.texsrt.push_back({bef(q), bef(q + 4), bef(q + 8), bef(q + 12), bef(q + 16)}); q += 20; }
                    q += ncoord * 4; if (ntevswap) q += 4; q += nind * 20; q += nindst * 4; q += ntev * 16;
                    if (nalpha) { M.alphaCompare = 1; q += 8; }
                    if (nblend) { M.blend = q[0]; }
                    materials.push_back(M);
                }
            }
            else if (tag == "pan1" || tag == "pic1" || tag == "txt1" || tag == "wnd1" || tag == "bnd1") {
                Pane* p = new Pane(); p->type = tag; p->flags = b[0]; p->origin = b[1]; p->alpha = b[2];
                p->name.assign((const char*)b + 4, strnlen((const char*)b + 4, 16));
                const uint8_t* f = b + 0x1c; p->x = bef(f); p->y = bef(f + 4); p->z = bef(f + 8); p->rx = bef(f + 12); p->ry = bef(f + 16); p->rz = bef(f + 20); p->sx = bef(f + 24); p->sy = bef(f + 28); p->w = bef(f + 32); p->h = bef(f + 36);
                memset(p->vcol, 255, sizeof p->vcol);
                if (tag == "pic1") {
                    const uint8_t* q = b + 0x44; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) p->vcol[i][j] = q[i * 4 + j];
                    p->material = be16(q + 16); int nc = q[18]; q += 20;
                    for (int i = 0; i < nc; i++) { std::array<float, 8> t; for (int j = 0; j < 8; j++) t[j] = bef(q + j * 4); p->uv.push_back(t); q += 32; }
                }
                else if (tag == "txt1") {
                    const uint8_t* q = b + 0x44; uint16_t blen = be16(q), slen = be16(q + 2); p->material = be16(q + 4); p->font = be16(q + 6);
                    p->align = q[8]; p->fontSize = q[9]; uint32_t toff = be32(q + 12);
                    for (int i = 0; i < 2; i++) for (int j = 0; j < 4; j++) p->tcol[i][j] = q[16 + i * 4 + j];
                    p->fsx = bef(q + 24); p->fsy = bef(q + 28); p->charSpace = bef(q + 32); p->lineSpace = bef(q + 36);
                    const uint8_t* t = b + toff - 8; for (int i = 0; i + 1 < slen; i += 2) { char16_t c = be16(t + i); if (!c) break; p->text.push_back(c); }
                    (void)blen;
                }
                else if (tag == "wnd1") {
                    const uint8_t* q = b + 0x44; for (int i = 0; i < 4; i++) p->inset[i] = bef(q + i * 4); int nframes = q[16]; uint32_t coff = be32(q + 20), foff = be32(q + 24);
                    const uint8_t* c = b + coff - 8; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) p->vcol[i][j] = c[i * 4 + j]; p->material = be16(c + 16); int nc = c[18]; const uint8_t* u = c + 20;
                    for (int i = 0; i < nc; i++) { std::array<float, 8> t; for (int j = 0; j < 8; j++) t[j] = bef(u + j * 4); p->uv.push_back(t); u += 32; }
                    const uint8_t* fo = b + foff - 8; for (int i = 0; i < nframes; i++) { const uint8_t* fr = b + be32(fo + i * 4) - 8; p->frames.push_back({be16(fr), fr[2]}); }
                }
                panes.push_back(p); last = p;
                if (!stack.empty()) { p->parent = stack.back(); stack.back()->children.push_back(p); } else if (!root) root = p;
            }
            else if (tag == "pas1") { stack.push_back(last); }
            else if (tag == "pae1") { if (!stack.empty()) stack.pop_back(); }
            else if (tag == "usd1" && last) {
                int cnt = be16(b);
                for (int k = 0; k < cnt; k++) { const uint8_t* e = b + 4 + k * 12; uint32_t no = be32(e), dof = be32(e + 4); int c = be16(e + 8); int typ = e[10]; UserData u; u.name = (const char*)(e + no); u.type = typ;
                    if (typ == 0) u.str.assign((const char*)(e + dof), c); else if (typ == 1) for (int i = 0; i < c; i++) u.ints.push_back((int32_t)be32(e + dof + i * 4)); else for (int i = 0; i < c; i++) u.floats.push_back(bef(e + dof + i * 4));
                    last->userdata.push_back(u); }
            }
            else if (tag == "grp1") { Group g; g.name.assign((const char*)b, strnlen((const char*)b, 16)); int cnt = be16(b + 16); for (int i = 0; i < cnt; i++) g.panes.emplace_back((const char*)b + 20 + i * 16, strnlen((const char*)b + 20 + i * 16, 16)); groups.push_back(g); }
            (void)bl; off += ln;
        }
        return root != nullptr;
    }
    static float bef(const uint8_t* p) { uint32_t v = gx::be32(p); float f; memcpy(&f, &v, 4); return f; }
};

} // namespace lyt
