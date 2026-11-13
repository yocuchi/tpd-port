// RBNK (banco de instrumentos) de NW4R: programa → (rango de tecla, rango de velocidad) → parámetros de instrumento.
#pragma once
#include "brsar.hpp"

namespace snd {

struct InstParam {
    uint32_t wave = 0; uint8_t attack = 127, decay = 127, sustain = 127, release = 127, hold = 0, waveType = 0, noteOff = 0, altAssign = 0;
    uint8_t origKey = 60, volume = 127, pan = 64, surround = 0; float pitch = 1.0f; bool valid = false;
};

struct Bank {
    const uint8_t* body = nullptr; size_t n = 0; uint32_t count = 0;
    bool load(const uint8_t* d, size_t len) {
        if (len < 0x30 || memcmp(d, "RBNK", 4)) return false; uint32_t dataOff = be32(d + 16); body = d + dataOff + 8; n = len; count = be32(body); return true;
    }
    // ref: tipo (0 null, 1 inst, 2 rango, 3 índice), offset relativo al cuerpo
    InstParam resolve(const uint8_t* refp, int key, int vel, int depth = 0) const {
        InstParam ip; int type = refp[0], dtype = refp[1]; uint32_t off = be32(refp + 4);  // ref: u8 tipo (0 null, 1 offset), u8 tipo de dato, u16 pad, u32 offset
        if (type == 0 || depth > 4) return ip;
        const uint8_t* p = body + off;
        if (dtype == 1) {
            ip.wave = be32(p); ip.attack = p[4]; ip.decay = p[5]; ip.sustain = p[6]; ip.release = p[7]; ip.hold = p[8]; ip.waveType = p[9]; ip.noteOff = p[10]; ip.altAssign = p[11];
            ip.origKey = p[12]; ip.volume = p[13]; ip.pan = p[14]; ip.surround = p[15]; uint32_t f = be32(p + 16); memcpy(&ip.pitch, &f, 4); ip.valid = true; return ip;
        }
        int sel = depth == 0 ? key : vel;  // primer nivel por tecla, segundo por velocidad
        if (dtype == 2) {  // RangeTable: u8 count, u8 keys[count], pad a 4, refs
            int cnt = p[0]; int idx = -1; for (int i = 0; i < cnt; i++) if (sel <= p[1 + i]) { idx = i; break; } if (idx < 0) idx = cnt - 1; if (idx < 0) return ip;
            const uint8_t* refs = p + ((1 + cnt + 3) & ~3); return resolve(refs + idx * 8, key, vel, depth + 1);
        }
        if (dtype == 3) {  // IndexTable: u8 min, u8 max, pad 2, refs[max-min+1]
            int mn = p[0], mx = p[1]; if (sel < mn || sel > mx) return ip; return resolve(p + 4 + (sel - mn) * 8, key, vel, depth + 1);
        }
        return ip;
    }
    // tabla: u32 count, refs (8 bytes) desde body+4
    InstParam lookup(int program, int key, int vel) const { if (!body || (uint32_t)program >= count) return InstParam(); return resolve(body + 4 + program * 8, key, vel); }
};

} // namespace snd
