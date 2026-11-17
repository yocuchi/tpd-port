// Decodificador de texturas de Wii (TPL y TEX0 de BRRES) a RGBA8.
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

namespace gx {

struct Image { int w = 0, h = 0; std::vector<uint8_t> rgba; int magFilter = 1; };  // magFilter: GX_NEAR 0 / GX_LINEAR 1 (cabecera TPL; TEX0 no lo trae → lineal, como GX por defecto)

inline uint16_t be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
inline uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

inline void rgb565(uint16_t v, uint8_t* o) { o[0] = ((v >> 11) & 31) * 255 / 31; o[1] = ((v >> 5) & 63) * 255 / 63; o[2] = (v & 31) * 255 / 31; }
inline void rgb5a3(uint16_t v, uint8_t* o) {
    if (v & 0x8000) { o[0] = ((v >> 10) & 31) * 255 / 31; o[1] = ((v >> 5) & 31) * 255 / 31; o[2] = (v & 31) * 255 / 31; o[3] = 255; }
    else { o[0] = ((v >> 8) & 15) * 17; o[1] = ((v >> 4) & 15) * 17; o[2] = (v & 15) * 17; o[3] = ((v >> 12) & 7) * 255 / 7; }
}

// Decodifica una imagen en formato GX (0 I4, 1 I8, 2 IA4, 3 IA8, 4 RGB565, 5 RGB5A3, 6 RGBA32, 8 C4, 9 C8, 14 CMPR).
inline Image decode(const uint8_t* d, int w, int h, int fmt, const std::vector<uint32_t>* pal = nullptr) {
    Image im; im.w = w; im.h = h; im.rgba.assign((size_t)w * h * 4, 0);
    auto put = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) { if (x < w && y < h) { uint8_t* o = &im.rgba[((size_t)y * w + x) * 4]; o[0] = r; o[1] = g; o[2] = b; o[3] = a; } };
    auto putpal = [&](int x, int y, unsigned i) { uint32_t c = (pal && i < pal->size()) ? (*pal)[i] : 0; put(x, y, c >> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255); };
    size_t p = 0;
    if (fmt == 14) {
        for (int by = 0; by < h; by += 8) for (int bx = 0; bx < w; bx += 8) for (int sb = 0; sb < 4; sb++) {
            uint16_t c0 = be16(d + p), c1 = be16(d + p + 2); uint32_t bits = be32(d + p + 4); p += 8;
            uint8_t cols[4][4]; rgb565(c0, cols[0]); rgb565(c1, cols[1]); cols[0][3] = cols[1][3] = 255;
            if (c0 > c1) { for (int k = 0; k < 3; k++) { cols[2][k] = (2 * cols[0][k] + cols[1][k]) / 3; cols[3][k] = (cols[0][k] + 2 * cols[1][k]) / 3; } cols[2][3] = cols[3][3] = 255; }
            else { for (int k = 0; k < 3; k++) { cols[2][k] = (cols[0][k] + cols[1][k]) / 2; cols[3][k] = 0; } cols[2][3] = 255; cols[3][3] = 0; }
            int ox = bx + (sb & 1) * 4, oy = by + (sb >> 1) * 4;
            for (int yy = 0; yy < 4; yy++) for (int xx = 0; xx < 4; xx++) { int idx = (bits >> (30 - (yy * 4 + xx) * 2)) & 3; put(ox + xx, oy + yy, cols[idx][0], cols[idx][1], cols[idx][2], cols[idx][3]); }
        }
        return im;
    }
    int bw = 4, bh = 4;
    if (fmt == 0 || fmt == 8) { bw = 8; bh = 8; } else if (fmt == 1 || fmt == 2 || fmt == 9) { bw = 8; bh = 4; }
    for (int by = 0; by < h; by += bh) for (int bx = 0; bx < w; bx += bw) {
        if (fmt == 6) {
            for (int yy = 0; yy < 4; yy++) for (int xx = 0; xx < 4; xx++) { int i = (yy * 4 + xx) * 2; put(bx + xx, by + yy, d[p + i + 1], d[p + 32 + i], d[p + 32 + i + 1], d[p + i]); }
            p += 64; continue;
        }
        for (int yy = 0; yy < bh; yy++) for (int xx = 0; xx < bw; xx++) {
            int i = yy * bw + xx;
            switch (fmt) {
            case 0: { uint8_t v = d[p + i / 2]; v = (xx & 1) ? (v & 15) : (v >> 4); v *= 17; put(bx + xx, by + yy, v, v, v, v); break; }  // GX I4: alfa = intensidad
            case 1: { uint8_t v = d[p + i]; put(bx + xx, by + yy, v, v, v, v); break; }  // GX I8: alfa = intensidad
            case 2: { uint8_t v = d[p + i]; uint8_t in = (v & 15) * 17, a = (v >> 4) * 17; put(bx + xx, by + yy, in, in, in, a); break; }
            case 3: { uint8_t a = d[p + i * 2], in = d[p + i * 2 + 1]; put(bx + xx, by + yy, in, in, in, a); break; }
            case 4: { uint8_t o[4]; rgb565(be16(d + p + i * 2), o); put(bx + xx, by + yy, o[0], o[1], o[2], 255); break; }
            case 5: { uint8_t o[4]; rgb5a3(be16(d + p + i * 2), o); put(bx + xx, by + yy, o[0], o[1], o[2], o[3]); break; }
            case 8: { uint8_t v = d[p + i / 2]; v = (xx & 1) ? (v & 15) : (v >> 4); putpal(bx + xx, by + yy, v); break; }
            case 9: putpal(bx + xx, by + yy, d[p + i]); break;
            }
        }
        p += 32;
    }
    return im;
}

inline std::vector<uint32_t> decodePalette(const uint8_t* d, int n, int pfmt) {
    std::vector<uint32_t> pal(n);
    for (int k = 0; k < n; k++) {
        uint16_t v = be16(d + k * 2); uint8_t o[4] = {0, 0, 0, 255};
        if (pfmt == 0) { o[0] = o[1] = o[2] = v & 255; o[3] = v >> 8; } else if (pfmt == 1) rgb565(v, o); else rgb5a3(v, o);
        pal[k] = ((uint32_t)o[0] << 24) | (o[1] << 16) | (o[2] << 8) | o[3];
    }
    return pal;
}

// TPL: devuelve la primera imagen.
inline bool loadTPL(const uint8_t* d, size_t n, Image& out) {
    if (n < 12 || be32(d) != 0x0020AF30) return false;
    uint32_t toff = be32(d + 8); uint32_t ioff = be32(d + toff), poff = be32(d + toff + 4);
    int h = be16(d + ioff), w = be16(d + ioff + 2); uint32_t fmt = be32(d + ioff + 4), doff = be32(d + ioff + 8);
    std::vector<uint32_t> pal;
    if (poff) { int pn = be16(d + poff); uint32_t pfmt = be32(d + poff + 4), pdoff = be32(d + poff + 8); pal = decodePalette(d + pdoff, pn, pfmt); }
    out = decode(d + doff, w, h, fmt, pal.empty() ? nullptr : &pal); out.magFilter = (int)be32(d + ioff + 0x18) == 0 ? 0 : 1;  // filtro de ampliación del TPL (+0x18)
    return true;
}

// TEX0 dentro de un BRRES: busca por nombre.
inline bool loadTEX0(const uint8_t* d, size_t n, const std::string& name, Image& out) {
    for (size_t i = 0; i + 64 < n; i++) {
        if (memcmp(d + i, "TEX0", 4)) continue;
        uint32_t ver = be32(d + i + 8); int nsec = ver == 2 ? 2 : 1;
        uint32_t doff = be32(d + i + 16), noff = be32(d + i + 16 + 4 * nsec); size_t q = i + 20 + 4 * nsec;
        int w = be16(d + q + 4), h = be16(d + q + 6); uint32_t fmt = be32(d + q + 8);
        std::string nm((const char*)d + i + noff);
        if (nm == name) { out = decode(d + i + doff, w, h, fmt); return true; }
        i += 3;
    }
    return false;
}

} // namespace gx
