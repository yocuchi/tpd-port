// Prueba en Mac: lee los 11 TPF2, compila todas las expresiones y scripts, y ejecuta algunos con un host falso.
#include "../engine/tpf2.hpp"
#include "../engine/script.hpp"
#include <cstdio>
#include <fstream>
#include <map>

using namespace rules;

struct MockHost : ScriptHost {
    std::map<std::string, float> v; std::vector<std::string> log;
    float level = 1, lines = 0, score_ = 0, goal = 10, gain = 0;
    float get(char kind, int idx) override {
        if (kind == 'f' && idx == 100) return level;
        if (kind == 'f' && idx == 101) return lines;
        if (kind == 'u' && idx == 100) return score_;
        if (kind == 'u' && idx == 101) return goal;
        if (kind == 'u' && idx == 102) return gain;
        if (kind == 'x') return 0.5f;
        auto it = v.find(std::string(1, kind) + std::to_string(idx)); return it == v.end() ? 0.0f : it->second;
    }
    void setVar(char k, int i, float x) override { v[std::string(1, k) + std::to_string(i)] = x; log.push_back("set " + std::string(1, k) + std::to_string(i) + "=" + std::to_string(x)); }
    void setGoal(float x) override { goal = x; log.push_back("goal=" + std::to_string(x)); }
    void score(float x) override { score_ += x; gain += x; log.push_back("score+=" + std::to_string(x)); }
    void scoreReset() override { score_ = 0; log.push_back("scorereset"); }
    void win() override { log.push_back("WIN"); } void lose() override { log.push_back("LOSE"); } void nextLevel() override { log.push_back("NEXTLEVEL"); }
    void clientLine(const std::string& raw) override { log.push_back("client: " + raw); }
    void command(Cmd c, const std::string& name, float a, float b, int n) override { char buf[200]; snprintf(buf, sizeof buf, "cmd 0x%x \"%s\" %g %g (%d)", c, name.c_str(), a, b, n); log.push_back(buf); }
};

static std::vector<uint8_t> readFile(const char* p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }

int main(int argc, char** argv) {
    int nexpr = 0, nscript = 0, fails = 0;
    for (int i = 1; i < argc; i++) {
        auto buf = readFile(argv[i]);
        tpf::Rules R;
        try { R = tpf::parse(buf.data(), buf.size()); }
        catch (std::exception& e) { printf("%s: FALLO parse: %s\n", argv[i], e.what()); fails++; continue; }
        printf("== %s: usados 0x%zx bytes, campo %dx%d, %zu piezas, %zu niveles\n", argv[i], R.bytesUsed, R.fldd.width, R.fldd.height, R.tetr.size(), R.lvls.items.size());
        auto tryExpr = [&](const char* what, const std::string& s) { if (s.empty()) return; nexpr++; try { Expr e(s); } catch (std::exception& ex) { printf("  expr %s: %s\n", what, ex.what()); fails++; } };
        auto tryScript = [&](const char* what, const std::string& s) { if (s.empty()) return; nscript++; try { Script sc(s); } catch (std::exception& ex) { printf("  script %s: %s\n", what, ex.what()); fails++; } };
        tryExpr("garbage1", R.fldd.garbage1_count); tryExpr("garbage2", R.fldd.garbage2_count);
        tryExpr("ctrl.hold", R.ctrl.enable_hold); tryExpr("ctrl.hd", R.ctrl.enable_hard_drop);
        tryExpr("pgen.delay", R.pgen.delay); tryExpr("pgen.x", R.pgen.start_x); tryExpr("pgen.y", R.pgen.start_y);
        tryExpr("pfal.fall", R.pfal.fall_speed); tryExpr("pfal.soft", R.pfal.soft_speed); tryExpr("pfal.tol", R.pfal.tolerance); tryExpr("pfal.hard", R.pfal.hard_speed);
        tryExpr("plok.delay", R.plok.delay); tryExpr("plok.ext", R.plok.extended_count); tryExpr("pelm.fall", R.pelm.fall_speed); tryExpr("pgvt.fall", R.pgvt.fall_speed);
        for (auto& p : R.ptrn) tryExpr("ptrn.min", p.min_count);
        for (auto& e : R.evnt) tryExpr("evnt.time", e.exec_time);
        for (auto& it : R.ptr8.it) tryExpr("ptr8.run", it.run_exp);
        tryScript("data.gamesec", R.data.gamesec); tryScript("data.levelsec", R.data.levelsec); tryScript("data.start", R.data.start); tryScript("data.topout", R.data.topout);
        for (auto& s : R.pcev.s) tryScript("pcev", s);
        const tpf::Phase* phases[] = {&R.pgen, &R.pfal, &R.plok, &R.ppat, &R.ptr8, &R.panm, &R.pelm, &R.psol, &R.pgvt, &R.pcmp};
        for (const tpf::Phase* ph : phases) { tryScript("phase.begin", ph->begin); tryScript("phase.end", ph->end); }
        tryScript("psol.iter", R.psol.minoiterate); for (auto& it : R.ptr8.it) tryScript("ptr8.iter", it.minoiterate);
        tryScript("pelm.collapse", R.pelm.collapse); tryScript("pelm.fall", R.pelm.fall); tryScript("pelm.land", R.pelm.land); tryScript("pgvt.land", R.pgvt.land);
        tryScript("lvls.start", R.lvls.start); tryScript("lvls.end", R.lvls.end);
        for (auto& l : R.lvls.items) { tryScript("lvl.begin", l.begin); tryScript("lvl.end", l.end); }
        for (auto& p : R.ptrn) { tryScript("ptrn.begin", p.begin); tryScript("ptrn.permino", p.permino); tryScript("ptrn.end", p.end); }
        for (auto& e : R.evnt) tryScript("evnt", e.script);

        if (i == 1) {
            MockHost h;
            Expr fall(R.pfal.fall_speed);
            printf("  gravedad por nivel (s/fila): ");
            for (int lv = 1; lv <= 15; lv++) { h.level = (float)lv; printf("%d:%.4f ", lv, fall.eval(h)); }
            printf("\n");
            h.level = 3; Expr delay(R.pgen.delay); printf("  pgen.delay nivel 3 = %g\n", delay.eval(h));
            Script cmp(R.pcmp.begin); h.lines = 10; h.goal = 10; h.v["f0"] = 42; cmp.run(h);
            Script ls(R.lvls.start); ls.run(h);
            Script pe(R.pelm.end); h.v["p0"] = 4; pe.run(h);
            Script pg(R.pgen.begin); pg.run(h);
            for (auto& s : h.log) printf("  log: %s\n", s.c_str());
        }
    }
    printf("\n%d expresiones y %d scripts compilados, %d fallos\n", nexpr, nscript, fails);
    return fails ? 1 : 0;
}
