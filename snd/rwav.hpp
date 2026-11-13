// RWAV (onda, DSP-ADPCM/PCM) y RWAR (archivo de ondas) de NW4R.
#pragma once
#include "brsar.hpp"
#include <memory>

namespace snd {

struct Sample {
    std::vector<int16_t> pcm;   // mono (canal 0)
    int rate = 32000; bool loop = false; uint32_t loopStart = 0, loopEnd = 0;  // en muestras
};

inline int16_t clamp16(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }

// nibble DSP → índice de muestra (16 nibbles = 1 cabecera + 14 muestras)
inline uint32_t nibbleToSample(uint32_t n) { uint32_t fr = n / 16, r = n % 16; return fr * 14 + (r >= 2 ? r - 2 : 0); }

inline bool decodeRWAV(const uint8_t* d, size_t n, Sample& out) {
    if (n < 0x20 || memcmp(d, "RWAV", 4)) return false;
    uint32_t infoOff = be32(d + 16), dataOff = be32(d + 24);
    const uint8_t* info = d + infoOff + 8; const uint8_t* dat = d + dataOff + 8;
    int fmt = info[0]; out.loop = info[1] != 0; int ch = info[2]; (void)ch; out.rate = be16(info + 4);
    uint32_t ls = be32(info + 8), le = be32(info + 12); uint32_t chTab = be32(info + 16);
    uint32_t ch0 = be32(info + chTab); const uint8_t* cp = info + ch0; uint32_t sampOff = be32(cp), adpOff = be32(cp + 4);
    const uint8_t* src = dat + sampOff;
    if (fmt == 2) {
        const uint8_t* ap = info + adpOff; int16_t coef[16]; for (int i = 0; i < 16; i++) coef[i] = (int16_t)be16(ap + i * 2);
        int hist1 = (int16_t)be16(ap + 36), hist2 = (int16_t)be16(ap + 38);
        uint32_t nibbles = le; uint32_t total = nibbleToSample(nibbles) + 1; out.pcm.reserve(total);
        uint32_t frames = (nibbles + 15) / 16;
        for (uint32_t f = 0; f < frames; f++) {
            const uint8_t* fr = src + f * 8; int ps = fr[0]; int scale = 1 << (ps & 15); int pi = (ps >> 4) & 7; int c1 = coef[pi * 2], c2 = coef[pi * 2 + 1];
            for (int s = 0; s < 14; s++) {
                int nib = (s & 1) ? (fr[1 + s / 2] & 15) : (fr[1 + s / 2] >> 4); if (nib >= 8) nib -= 16;
                int v = (nib * scale * 2048 + c1 * hist1 + c2 * hist2 + 1024) >> 11; int16_t o = clamp16(v); hist2 = hist1; hist1 = o; out.pcm.push_back(o);
                if (out.pcm.size() >= total) break;
            }
            if (out.pcm.size() >= total) break;
        }
        out.loopStart = nibbleToSample(ls); out.loopEnd = nibbleToSample(le);
    } else if (fmt == 1) { uint32_t cnt = le; for (uint32_t i = 0; i <= cnt && (i + 1) * 2 <= n; i++) out.pcm.push_back((int16_t)be16(src + i * 2)); out.loopStart = ls; out.loopEnd = le; }
    else { uint32_t cnt = le; for (uint32_t i = 0; i <= cnt; i++) out.pcm.push_back((int16_t)((int8_t)src[i] << 8)); out.loopStart = ls; out.loopEnd = le; }
    if (out.loopEnd >= out.pcm.size()) out.loopEnd = out.pcm.empty() ? 0 : (uint32_t)out.pcm.size() - 1;
    return !out.pcm.empty();
}

struct WaveArc {
    const uint8_t* d = nullptr; size_t n = 0; std::vector<std::pair<uint32_t, uint32_t>> entries;  // (offset, len) absolutos
    std::vector<std::shared_ptr<Sample>> cache;
    bool load(const uint8_t* data, size_t len) {
        d = data; n = len; if (n < 32 || memcmp(d, "RWAR", 4)) return false;
        uint32_t tbl = be32(d + 16), dat = be32(d + 24); uint32_t cnt = be32(d + tbl + 8);
        for (uint32_t i = 0; i < cnt; i++) { uint32_t o = be32(d + tbl + 16 + i * 12), l = be32(d + tbl + 20 + i * 12); entries.push_back({dat + o, l}); }
        cache.resize(cnt); return true;
    }
    std::shared_ptr<Sample> get(uint32_t i) {
        if (i >= entries.size()) return nullptr;
        if (!cache[i]) { auto s = std::make_shared<Sample>(); if (decodeRWAV(d + entries[i].first, entries[i].second, *s)) cache[i] = s; }
        return cache[i];
    }
};

} // namespace snd
