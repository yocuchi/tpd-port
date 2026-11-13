// Archivos U8 (.arc) de Wii en memoria.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "tpl.hpp"

namespace gx {

struct Arc {
    std::vector<uint8_t> data;
    std::map<std::string, std::pair<size_t, size_t>> files;  // ruta "dir/sub/fichero" -> (offset, tamaño)

    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); if (!f) return false;
        data.assign((std::istreambuf_iterator<char>(f)), {});
        return parse();
    }
    bool parse() {
        if (data.size() < 32 || be32(&data[0]) != 0x55AA382D) return false;
        uint32_t root = be32(&data[4]); uint32_t n0 = be32(&data[root + 8]); uint32_t st = root + n0 * 12;
        std::vector<std::string> stack; std::vector<uint32_t> ends;
        for (uint32_t i = 0; i < n0; i++) {
            const uint8_t* e = &data[root + i * 12];
            uint8_t t = e[0]; uint32_t no = (e[1] << 16) | (e[2] << 8) | e[3]; uint32_t a = be32(e + 4), b = be32(e + 8);
            while (!ends.empty() && i >= ends.back()) { ends.pop_back(); stack.pop_back(); }
            std::string name((const char*)&data[st + no]);
            if (t == 1) { if (i) { stack.push_back(name); ends.push_back(b); } }
            else { std::string p; for (auto& s : stack) p += s + "/"; p += name; files[p] = {a, b}; }
        }
        return true;
    }
    const uint8_t* get(const std::string& suffix, size_t& len) const {  // busca por final de ruta
        for (auto& kv : files) { const std::string& k = kv.first; if (k.size() >= suffix.size() && k.compare(k.size() - suffix.size(), suffix.size(), suffix) == 0) { len = kv.second.second; return &data[kv.second.first]; } }
        len = 0; return nullptr;
    }
};

} // namespace gx
