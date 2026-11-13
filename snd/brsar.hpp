// Índice del archivo de sonido NW4R (.brsar): nombres (SYMB), sonidos/bancos/ficheros/grupos (INFO), datos (FILE).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace snd {

inline uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
inline uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

struct SoundInfo { std::string name; uint32_t fileId, playerId, seqOffset, bankId, allocTrack; uint8_t volume, prio, type; };
struct FileLoc { uint32_t off = 0, len = 0, waveOff = 0, waveLen = 0; };

struct Brsar {
    std::vector<uint8_t> data;
    std::vector<std::string> names;
    std::map<std::string, SoundInfo> sounds;
    std::map<uint32_t, uint32_t> bankFile;   // bankId -> fileId
    std::map<uint32_t, FileLoc> files;       // fileId -> posición en FILE

    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); if (!f) return false; data.assign((std::istreambuf_iterator<char>(f)), {});
        if (data.size() < 40 || memcmp(&data[0], "RSAR", 4)) return false;
        uint32_t symb = be32(&data[16]), info = be32(&data[24]);
        // SYMB
        const uint8_t* s = &data[symb]; uint32_t nameOff = be32(s + 8); uint32_t n = be32(s + 8 + nameOff);
        for (uint32_t i = 0; i < n; i++) { uint32_t o = be32(s + 12 + nameOff + i * 4) + 8; names.emplace_back((const char*)s + o); }
        // INFO
        const uint8_t* inf = &data[info]; const uint8_t* base = inf + 8;
        auto ref = [&](const uint8_t* p, int& type, int& dtype) { type = p[0]; dtype = p[2]; return be32(p + 4); };
        auto table = [&](uint32_t off) { std::vector<uint32_t> v; uint32_t cnt = be32(base + off); for (uint32_t i = 0; i < cnt; i++) { int t, dt; v.push_back(ref(base + off + 4 + i * 8, t, dt)); } return v; };
        int t, dt; uint32_t soundTab = ref(inf + 8, t, dt), bankTab = ref(inf + 16, t, dt), fileTab = ref(inf + 32, t, dt), groupTab = ref(inf + 40, t, dt);
        for (uint32_t o : table(soundTab)) {
            const uint8_t* e = base + o; SoundInfo si; uint32_t strId = be32(e); si.fileId = be32(e + 4); si.playerId = be32(e + 8); si.volume = e[0x14]; si.prio = e[0x15]; si.type = e[0x16];
            uint32_t det = ref(e + 0x18, t, dt); const uint8_t* de = base + det; si.seqOffset = be32(de); si.bankId = be32(de + 4); si.allocTrack = be32(de + 8);
            si.name = strId < names.size() ? names[strId] : std::to_string(strId); sounds[si.name] = si;
        }
        { uint32_t i = 0; for (uint32_t o : table(bankTab)) { const uint8_t* e = base + o; bankFile[i++] = be32(e + 4); } }
        (void)fileTab;
        for (uint32_t o : table(groupTab)) {
            const uint8_t* e = base + o; uint32_t goff = be32(e + 16), woff = be32(e + 24); uint32_t itab = ref(e + 32, t, dt);
            for (uint32_t io : table(itab)) { const uint8_t* ie = base + io; FileLoc fl; uint32_t fid = be32(ie); fl.off = goff + be32(ie + 4); fl.len = be32(ie + 8); fl.waveOff = woff + be32(ie + 12); fl.waveLen = be32(ie + 16); files[fid] = fl; }
        }
        return true;
    }
    const SoundInfo* sound(const std::string& name) const { auto it = sounds.find(name); return it == sounds.end() ? nullptr : &it->second; }
    const uint8_t* file(uint32_t id, size_t& len) const { auto it = files.find(id); if (it == files.end()) { len = 0; return nullptr; } len = it->second.len; return &data[it->second.off]; }
    const uint8_t* waveArc(uint32_t id, size_t& len) const { auto it = files.find(id); if (it == files.end() || !it->second.waveLen) { len = 0; return nullptr; } len = it->second.waveLen; return &data[it->second.waveOff]; }
};

} // namespace snd
