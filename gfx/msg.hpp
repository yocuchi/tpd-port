// Bancos de mensajes del juego (mess/NNN_*.bin): u32 BE nº de cadenas, u32 BE offsets, cadenas UTF-16BE terminadas en 0.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace msg {

struct Bank {
    std::vector<std::u16string> items;
    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
        if (d.size() < 4) return false;
        auto be32 = [&](size_t o) { return (uint32_t)d[o] << 24 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 8 | d[o + 3]; };
        uint32_t n = be32(0); if (4 + n * 4 > d.size()) return false;
        for (uint32_t i = 0; i < n; i++) {
            size_t o = be32(4 + i * 4); std::u16string s;
            while (o + 1 < d.size()) { char16_t c = (char16_t)(d[o] << 8 | d[o + 1]); o += 2; if (!c) break; s.push_back(c); }
            items.push_back(s);
        }
        return true;
    }
    const std::u16string& get(size_t i) const { static const std::u16string e; return i < items.size() ? items[i] : e; }
};

// Catálogo: número de fichero → banco (001 = mainmenu, 002 = singleplay, 003 = multiplay, 005 = option, 007 = records, 010 = pause, 014 = ingame...)
struct Catalog {
    std::map<int, Bank> banks; std::string dir;
    static const char* nameOf(int id) {
        switch (id) { case 0: return "000_defwin"; case 1: return "001_mainmenu"; case 2: return "002_singleplay"; case 3: return "003_multiplay"; case 4: return "004_onlineplay";
            case 5: return "005_option"; case 6: return "006_staff"; case 7: return "007_records"; case 8: return "008_tutorial"; case 9: return "009_config"; case 10: return "010_pause";
            case 11: return "011_inst"; case 12: return "012_ranking"; case 13: return "013_handling"; case 14: return "014_ingame"; case 15: return "015_error"; case 29: return "029_chat_mess";
            case 31: return "031_wifilogin"; case 32: return "032_wiiboard"; case 33: return "033_yokomochi"; case 34: return "034_banner"; case 35: return "035_congress"; case 50: return "050_edit"; }
        return nullptr;
    }
    // Glifos de botones de la fuente (tetrisfont00/01): e040 power, e041 cruceta, e042 A, e043 B, e044 home, e045 +, e046 -, e047 1, e048 2,
    // e04a C, e04b Z, e04c a, e04d b, e04e x, e04f y, e052 L, e053 R, e054 ZL, e055 ZR. El juego usa el mando de Wii de lado:
    // 2 = confirmar, 1 = atrás, - = ayuda. En Switch: 2→A, 1→B, -→X.
    bool switchButtons = true;
    const std::u16string& get(int file, size_t idx) {
        auto it = banks.find(file);
        if (it == banks.end()) {
            Bank b; const char* n = nameOf(file); if (n) b.load(dir + n + ".bin");
            if (switchButtons) for (auto& s : b.items) for (auto& c : s) { if (c == 0xe048) c = 0xe042; else if (c == 0xe047) c = 0xe043; else if (c == 0xe046) c = 0xe04e; }
            it = banks.emplace(file, std::move(b)).first;
        }
        return it->second.get(idx);
    }
};

} // namespace msg
