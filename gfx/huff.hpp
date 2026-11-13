// Descompresor Huffman de Nintendo (cabecera 0x24 / 0x28: 4 u 8 bits por símbolo), como HuffUnComp.
#pragma once
#include <cstdint>
#include <vector>

namespace gx {

inline std::vector<uint8_t> huffDecompress(const uint8_t* d, size_t n) {
    std::vector<uint8_t> out; if (n < 5) return out;
    uint32_t hdr = d[0] | (d[1] << 8) | (d[2] << 16) | ((uint32_t)d[3] << 24); int bits = hdr & 0xf; uint32_t size = hdr >> 8;
    if ((hdr & 0xf0) != 0x20 || (bits != 4 && bits != 8)) return out;
    size_t tree = 4; uint8_t treeSize = d[tree]; size_t data = tree + 1 + (size_t)treeSize * 2 + 1;
    size_t node = tree + 1, pos = data; uint32_t acc = 0; int accn = 0; uint32_t cur = 0; int nbits = 0;
    out.reserve(size);
    while (out.size() < size) {
        if (accn == 0) { if (pos + 4 > n) break; acc = d[pos] | (d[pos + 1] << 8) | (d[pos + 2] << 16) | ((uint32_t)d[pos + 3] << 24); pos += 4; accn = 32; }
        int bit = (acc >> 31) & 1; acc <<= 1; accn--;
        uint8_t val = d[node]; size_t base = (node & ~(size_t)1) + (val & 0x3f) * 2 + 2;
        size_t nxt = base + bit; bool leaf = bit ? (val & 0x40) : (val & 0x80);
        if (leaf) {
            uint8_t sym = d[nxt];
            if (bits == 8) out.push_back(sym); else { cur = (cur << 4) | sym; nbits += 4; if (nbits == 8) { out.push_back((uint8_t)cur); cur = 0; nbits = 0; } }
            node = tree + 1;
        } else node = nxt;
    }
    return out;
}

} // namespace gx
