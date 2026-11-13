// Fuentes de bits NW4R (BRFNT / BRFNA): hojas de glifos, anchos y mapa de caracteres.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "tpl.hpp"
#include "huff.hpp"

namespace lyt {

struct Font {
    int cellW = 0, cellH = 0, baseline = 0, maxW = 0; int sheetW = 0, sheetH = 0, perRow = 1, perCol = 1; int fmt = 0; int nsheets = 0; int height = 0, ascent = 0, lineFeed = 0;
    std::vector<gx::Image> sheets;
    struct Width { int8_t left, glyph, adv; }; std::vector<Width> widths; Width defW{0, 0, 0};
    std::map<char16_t, int> cmap; int altGlyph = 0;

    bool parse(const uint8_t* d, size_t n) {
        using gx::be16; using gx::be32;
        if (n < 16 || (memcmp(d, "RFNT", 4) && memcmp(d, "RFNA", 4))) return false;
        size_t finf = 0; for (size_t i = 0; i + 4 < n; i += 4) if (!memcmp(d + i, "FINF", 4)) { finf = i; break; }
        if (!finf) return false;
        const uint8_t* f = d + finf; lineFeed = f[9]; altGlyph = be16(f + 10); defW = {(int8_t)f[12], (int8_t)f[13], (int8_t)f[14]};
        uint32_t tglp = be32(f + 16), cwdh = be32(f + 20), cmapo = be32(f + 24); height = f[28]; ascent = f[30];
        const uint8_t* t = d + tglp - 8; cellW = t[8]; cellH = t[9]; baseline = t[10]; maxW = t[11];
        uint32_t sheetSize = be32(t + 12); nsheets = be16(t + 16); fmt = be16(t + 18) & 0x7fff; perRow = be16(t + 20); perCol = be16(t + 22); sheetW = be16(t + 24); sheetH = be16(t + 26); uint32_t doff = be32(t + 28);
        bool compressed = memcmp(d, "RFNA", 4) == 0;  // BRFNA: cada hoja va como u32 tamaño + Huffman
        size_t p = doff;
        for (int i = 0; i < nsheets; i++) {
            gx::Image im;
            if (compressed) { uint32_t sz = be32(d + p); std::vector<uint8_t> raw = gx::huffDecompress(d + p + 4, sz); if (raw.size() < sheetSize) raw.resize(sheetSize, 0); im = gx::decode(raw.data(), sheetW, sheetH, fmt); p = (p + 4 + sz + 3) & ~(size_t)3; }
            else { im = gx::decode(d + p, sheetW, sheetH, fmt); p += sheetSize; }
            if (fmt == 0 || fmt == 1) for (size_t k = 0; k < im.rgba.size(); k += 4) { im.rgba[k + 3] = im.rgba[k]; im.rgba[k] = im.rgba[k + 1] = im.rgba[k + 2] = 255; }  // intensidad → alfa
            sheets.push_back(im);
        }
        // anchos
        for (uint32_t o = cwdh; o; ) { const uint8_t* c = d + o - 8; int first = be16(c + 8), last = be16(c + 10); uint32_t next = be32(c + 12);
            if ((int)widths.size() < last + 1) widths.resize(last + 1, defW);
            for (int g = first; g <= last; g++) { const uint8_t* w = c + 16 + (g - first) * 3; widths[g] = {(int8_t)w[0], (int8_t)w[1], (int8_t)w[2]}; }
            o = next; }
        // mapa de caracteres
        for (uint32_t o = cmapo; o; ) { const uint8_t* c = d + o - 8; int first = be16(c + 8), last = be16(c + 10), typ = be16(c + 12); uint32_t next = be32(c + 16);
            if (typ == 0) { int g0 = be16(c + 20); for (int ch = first; ch <= last; ch++) cmap[(char16_t)ch] = g0 + (ch - first); }
            else if (typ == 1) { for (int ch = first; ch <= last; ch++) { int g = be16(c + 20 + (ch - first) * 2); if (g != 0xffff) cmap[(char16_t)ch] = g; } }
            else { int cnt = be16(c + 20); for (int i = 0; i < cnt; i++) { int ch = be16(c + 22 + i * 4), g = be16(c + 24 + i * 4); cmap[(char16_t)ch] = g; } }
            o = next; }
        return !sheets.empty();
    }
    int glyph(char16_t c) const { auto it = cmap.find(c); return it == cmap.end() ? altGlyph : it->second; }
    Width width(int g) const { return g >= 0 && g < (int)widths.size() ? widths[g] : defW; }
    // hoja y rectángulo de un glifo
    void rect(int g, int& sheet, int& x, int& y) const { int per = perRow * perCol; sheet = g / per; int i = g % per; x = (i % perRow) * (cellW + 1) + 1; y = (i / perRow) * (cellH + 1) + 1; }
};

} // namespace lyt
