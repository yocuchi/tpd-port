// Animaciones NW4R (BRLAN): claves por panel/material y evaluación a un frame.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include "brlyt.hpp"

namespace lyt {

struct Key { float frame, value, slope; };
struct Target { int type; int dataType; bool step; std::vector<Key> keys;
    float eval(float f) const {
        if (keys.empty()) return 0;
        if (step) { float v = keys[0].value; for (auto& k : keys) if (f >= k.frame) v = k.value; return v; }
        if (f <= keys[0].frame) return keys[0].value; if (f >= keys.back().frame) return keys.back().value;
        for (size_t i = 0; i + 1 < keys.size(); i++) { const Key& a = keys[i]; const Key& b = keys[i + 1]; if (f >= a.frame && f <= b.frame) {
            float t = (b.frame - a.frame) > 0 ? (f - a.frame) / (b.frame - a.frame) : 0, d = b.frame - a.frame;
            float t2 = t * t, t3 = t2 * t;  // hermite
            return (2 * t3 - 3 * t2 + 1) * a.value + (t3 - 2 * t2 + t) * d * a.slope + (-2 * t3 + 3 * t2) * b.value + (t3 - t2) * d * b.slope; } }
        return keys.back().value;
    }
};
struct Tag { std::string magic; std::vector<Target> targets; };
struct AnimEntry { std::string name; bool isMaterial; std::vector<Tag> tags; };

struct Anim {
    int frameCount = 0; bool loop = false; std::vector<AnimEntry> entries; std::vector<std::string> textures;  // tabla de texturas para RLTP
    bool parse(const uint8_t* d, size_t n) {
        using gx::be16; using gx::be32;
        if (n < 16 || memcmp(d, "RLAN", 4)) return false;
        uint16_t hdr = be16(d + 12), nsec = be16(d + 14); size_t off = hdr;
        for (int s = 0; s < nsec && off + 8 <= n; s++) {
            std::string tag((const char*)d + off, 4); uint32_t ln = be32(d + off + 4); const uint8_t* b = d + off + 8;
            if (tag == "pai1") {
                frameCount = be16(b); loop = b[2] != 0; int nent = be16(b + 6); uint32_t entoff = be32(b + 8);
                int ntex = be16(b + 4); for (int i = 0; i < ntex; i++) { uint32_t o = be32(b + 12 + i * 4); textures.emplace_back((const char*)b + 12 + o); }
                for (int i = 0; i < nent; i++) {
                    const uint8_t* e = b + be32(b + entoff - 8 + i * 4) - 8; AnimEntry E; E.name.assign((const char*)e, strnlen((const char*)e, 20)); int ntag = e[20]; E.isMaterial = e[21] != 0;
                    for (int t = 0; t < ntag; t++) {
                        const uint8_t* tg = e + be32(e + 24 + t * 4); Tag T; T.magic.assign((const char*)tg, 4); int nt = tg[4];
                        bool step = (T.magic == "RLVI" || T.magic == "RLTP");
                        for (int k = 0; k < nt; k++) {
                            const uint8_t* tr = tg + be32(tg + 8 + k * 4); Target R; R.type = tr[1]; R.dataType = tr[2]; R.step = step; int cnt = be16(tr + 4); uint32_t koff = be32(tr + 8);
                            for (int j = 0; j < cnt; j++) { const uint8_t* kk = tr + koff + j * (step ? 8 : 12); Key K; K.frame = Layout::bef(kk); if (step) { K.value = (float)be16(kk + 4); K.slope = 0; } else { K.value = Layout::bef(kk + 4); K.slope = Layout::bef(kk + 8); } R.keys.push_back(K); }
                            T.targets.push_back(R);
                        }
                        E.tags.push_back(T);
                    }
                    entries.push_back(E);
                }
            }
            off += ln;
        }
        return !entries.empty();
    }
    // Aplica el frame f a los paneles/materiales del layout.
    void apply(Layout& L, float f) const {
        for (const AnimEntry& E : entries) {
            if (E.isMaterial) {
                int mi = L.materialIndex(E.name); if (mi < 0) continue; Material& M = L.materials[mi];
                for (const Tag& T : E.tags) for (const Target& R : T.targets) {
                    float v = R.eval(f);
                    if (T.magic == "RLTS" && !M.texsrt.empty()) { TexSRT& s = M.texsrt[0]; switch (R.type) { case 0: s.tx = v; break; case 1: s.ty = v; break; case 2: s.rot = v; break; case 3: s.sx = v; break; case 4: s.sy = v; break; } }
                    else if (T.magic == "RLMC") { if (R.type < 4) M.fore[R.type] = (int16_t)v; else if (R.type < 8) M.back[R.type - 4] = (int16_t)v; }
                    else if (T.magic == "RLTP" && !M.texmaps.empty()) {
                        int ti = (int)v; if (ti >= 0 && ti < (int)textures.size()) {  // índice en la tabla del brlan → índice en txl1 del layout
                            const std::string& nm = textures[ti]; int li = -1; for (size_t i = 0; i < L.textures.size(); i++) if (L.textures[i] == nm) { li = (int)i; break; }
                            if (li < 0) { L.textures.push_back(nm); li = (int)L.textures.size() - 1; }
                            M.texmaps[0].tex = (uint16_t)li;
                        } else M.texmaps[0].tex = (uint16_t)v;
                    }
                }
                continue;
            }
            Pane* p = L.find(E.name); if (!p) continue;
            for (const Tag& T : E.tags) for (const Target& R : T.targets) {
                float v = R.eval(f);
                if (T.magic == "RLPA") { switch (R.type) { case 0: p->x = v; break; case 1: p->y = v; break; case 2: p->z = v; break; case 3: p->rx = v; break; case 4: p->ry = v; break; case 5: p->rz = v; break; case 6: p->sx = v; break; case 7: p->sy = v; break; case 8: p->w = v; break; case 9: p->h = v; break; } }
                else if (T.magic == "RLVI") { if (v != 0) p->flags |= 1; else p->flags &= ~1; }
                else if (T.magic == "RLVC") { if (R.type < 16) p->vcol[R.type / 4][R.type % 4] = (uint8_t)std::fmax(0, std::fmin(255, v)); else if (R.type == 16) p->alpha = (uint8_t)std::fmax(0, std::fmin(255, v)); }
            }
        }
    }
};

} // namespace lyt
