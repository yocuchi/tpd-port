// Modo Sombra (CFlowShadowGame, flow 9): rellenar una silueta con un presupuesto de piezas en 3 minutos (notes_modes_special.md §1).
#pragma once
#include "../engine/engine.hpp"
#include <fstream>
#include <cmath>

namespace game {

struct ShadowData {
    uint8_t stageToSil[50], budget[50]; uint8_t sil[50][200];  // 20 filas x 10 columnas, fila 0 arriba; 0 vacío, 1..0x14 color, 0xff gris (0xb)
    bool ok = false;
    bool load(const std::string& path) {
        std::ifstream f(path, std::ios::binary); std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
        if (d.size() < 10100) return false; memcpy(stageToSil, &d[0], 50); memcpy(budget, &d[50], 50); memcpy(sil, &d[100], 10000); ok = true; return true;
    }
};

class ShadowMode {
public:
    te::Game* g = nullptr; const ShadowData* D = nullptr; int stage = 0; int silhouette = 0; int maxPieces = 0;
    float timeLimit = 180.0f;  // ms en el original (180000); individual
    int total = 0, inside = 0, outside = 0; int pctQ = 0;  // porcentaje cuantizado en pasos de 1.25
    bool cleared = false, timeUp = false, failed = false; float finishTime = 0;
    int sweepRow = -1; float sweepTimer = 0; bool sweeping = false; bool finished = false;
    int beepsLeft = 10; std::function<void(int)> onSe;

    void start(te::Game& game, const ShadowData& data, int st) {
        g = &game; D = &data; stage = st; silhouette = D->stageToSil[st % 50]; maxPieces = D->budget[silhouette];
        g->linesEnabled = false; g->holdEnabled = false; g->lockCount = 0; placeShadow();
        cleared = timeUp = failed = sweeping = finished = false; sweepRow = -1; beepsLeft = 10; pctQ = 0; finishTime = 0;
    }
    // FUN_800c12c0: coloca la silueta en las 20 filas visibles
    void placeShadow() {
        int top = g->h / 2; for (int y = 0; y < 20; y++) for (int x = 0; x < 10; x++) { uint8_t v = D->sil[silhouette][y * 10 + x]; te::Cell& c = g->cells[x + (top + y) * g->w]; c.shadow = v == 0xff ? 0x0b : v; }
    }
    int remaining() const { return maxPieces - g->lockCount; }
    // FUN_800c14a4
    void count() {
        total = inside = outside = 0; int top = g->h / 2;
        for (int y = top; y < g->h; y++) for (int x = 0; x < g->w; x++) { const te::Cell& c = g->cells[x + y * g->w]; if (c.shadow) { total++; if (c.mino) inside++; } else if (c.mino) outside++; }
        float pct = total ? 100.0f * std::max(0, inside - outside) / total : 0; pctQ = (int)(pct / 1.25f);
    }
    int percentShown() const { return (int)(1.25f * pctQ + 0.9f); }
    float timeLeft() const { float t = timeLimit - g->gameTime; return t < 0 ? 0 : t; }

    void update(float dt) {
        if (finished) return;
        if (sweeping) {  // barrido final: cada 50 ms borra los bloques fuera de la sombra de una fila (FUN_800c15cc)
            sweepTimer += dt; if (sweepTimer > 0.05f) { sweepTimer = 0; if (sweepRow >= 0 && sweepRow < g->h) for (int x = 0; x < g->w; x++) { te::Cell& c = g->cells[x + sweepRow * g->w]; if (c.mino && !c.shadow) { c.mino = 0; c.flags = 0; } } sweepRow++; if (sweepRow >= g->h) { sweeping = false; finished = true; } }
            count(); return;
        }
        count();
        int secLeft = (int)ceilf(timeLeft()); if (secLeft <= 10 && secLeft > 0 && beepsLeft > 0 && secLeft <= beepsLeft) { beepsLeft = secLeft - 1; if (onSe) onSe(0x80); }
        if (g->state == 2) { failed = true; finishTime = g->gameTime; finished = true; return; }
        if (total && inside == total) { cleared = true; finishTime = g->gameTime; g->state = 1; sweeping = true; sweepRow = g->h / 2 - 1; sweepTimer = 0; return; }
        if (g->gameTime >= timeLimit) { timeUp = true; finishTime = timeLimit; g->state = 2; finished = true; return; }
        if (remaining() <= 0 && !g->hasPiece) { failed = true; finishTime = g->gameTime; g->state = 2; finished = true; return; }
    }
};

} // namespace game
