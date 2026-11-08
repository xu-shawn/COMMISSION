// Driver for the CPU port of COMMISSION's pipeline (cpipe.hpp).
//   cpipe probe < "seed x z" lines        which stage drops each known island (x z in blocks)
//   cpipe rate THREADS N [key=value]      N raw seeds: stage counts per phase-gate survivor, time per stage
// keys: phase gv1gate gv1final gv2gate gv2final amin bmin m2a m2b m2c m2d
#include "cpipe.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
using namespace cp;

static Thresholds parse(int argc, char **argv, int from) {
    Thresholds T;
    for (int i = from; i < argc; i++) {
        std::string kv = argv[i]; auto e = kv.find('='); std::string k = kv.substr(0, e); double v = std::stod(kv.substr(e + 1));
        if (k == "phase") T.phase = float(v); else if (k == "gv1gate") T.gv1.z0_gate = float(v); else if (k == "gv1final") T.gv1.final_ = float(v);
        else if (k == "gv2gate") T.gv2.gate = float(v); else if (k == "gv2final") T.gv2.final_ = float(v);
        else if (k == "amin") T.a_min = int(v); else if (k == "bmin") T.b_min = int(v);
        else if (k == "m2a") T.m2a = int(v); else if (k == "m2b") T.m2b = int(v); else if (k == "m2c") T.m2c = int(v); else if (k == "m2d") T.m2d = int(v);
        else { fprintf(stderr, "unknown key %s\n", k.c_str()); exit(2); }
    }
    return T;
}

// distance between two quart positions, the first taken modulo the octave-0A period (A-only stages)
static double dist_mod(int ax, int az, int bx, int bz) {
    const int P = 131072;
    auto d = [&](int a, int b) { int t = ((a - b) % P + P) % P; return double(std::min(t, P - t)); };
    return std::hypot(d(ax, bx), d(az, bz)) * 4;
}

int main(int argc, char **argv) {
    std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "probe" || mode == "probeq") {
        const bool quiet = mode == "probeq";
        Thresholds T = parse(argc, argv, 2);
        std::string line;
        printf("seed phase | gv1 best-score near | 2_01a count | gv2 near | 2_01b count | 2a 2b 2c 2d counts\n");
        while (std::getline(std::cin, line)) {
            std::istringstream is(line); long long sd; int bx, bz; if (!(is >> sd >> bx >> bz)) continue;
            std::string area_s = "0"; { std::string a, b2, c2, d2; if (is >> a >> b2 >> c2 >> d2) { bx = std::stoi(b2); bz = std::stoi(c2); area_s = d2; } }
            Seed s{uint64_t(sd)}; int qx = bx >> 2, qz = bz >> 2;
            float ph = phase_score(s);
            if (!quiet) printf("%lld %.2f |", sd, ph);
            // gradvecs_1 with every threshold off: the best final score within 1500 blocks (A period)
            GV1 all; all.z0_gate = -1e9f; all.final_ = -1e9f;
            std::vector<Pos> g1; gradvecs_1(s, g1, all);
            // recompute scores: rerun with thresholds to classify; simpler to scan candidates near the island
            std::vector<Pos> g1t; gradvecs_1(s, g1t, T.gv1);
            double bestd = 1e18; Pos near{};
            for (auto &p : g1t) { double d = dist_mod(p.x, p.z, qx, qz); if (d < bestd) { bestd = d; near = p; } }
            int nclose = 0; for (auto &p : g1t) nclose += dist_mod(p.x, p.z, qx, qz) < 3000;
            if (!quiet) printf(" gv1 %d near (closest %.0f) |", nclose, bestd);
            // follow every gv1 output within 3000 blocks through the pipeline, report the best progress
            int best_stage = 0; int acnt = -1, bcnt = -1; int casc[4] = {-1, -1, -1, -1};
            for (auto &p : g1t) {
                if (dist_mod(p.x, p.z, qx, qz) >= 3000) continue;
                int a = two_octave_count(s, 0, p.x, p.z); acnt = std::max(acnt, a);
                if (a < T.a_min) { best_stage = std::max(best_stage, 1); continue; }
                best_stage = std::max(best_stage, 2);
                std::vector<Pos> g2; gradvecs_2(s, p, g2, T.gv2);
                for (auto &q : g2) {
                    if (std::hypot(double(q.x - qx), double(q.z - qz)) * 4 >= 3000) continue;
                    best_stage = std::max(best_stage, 3);
                    int b = two_octave_count(s, 1, q.x, q.z); bcnt = std::max(bcnt, b);
                    if (b < T.b_min) continue;
                    best_stage = std::max(best_stage, 4);
                    Pos r = q; int st = 0;
                    const int no[4] = {12, 14, 16, 18}, rg[4] = {8192, 8192, 10240, 10240}, sm[4] = {256, 1024, 4096, 16384}; const bool mv[4] = {true, true, false, false};
                    const int mn[4] = {T.m2a, T.m2b, T.m2c, T.m2d};
                    for (; st < 4; st++) { Pos r0 = r; bool ok = cascade(s, no[st], rg[st], sm[st], 0, mv[st], r0); casc[st] = std::max(casc[st], r0.count);
                                           if (!ok || r0.count < mn[st]) break; r = r0; }
                    best_stage = std::max(best_stage, 4 + st);
                }
            }
            const char *names[] = {"gv1", "2_01a", "gv2", "2_01b", "2a", "2b", "2c", "2d", "CPU"};
            if (quiet) printf("%lld %s %.3f %d %d %d %d %d %d %s\n", sd, area_s.c_str(), ph, acnt, bcnt, casc[0], casc[1], casc[2], casc[3], names[best_stage]);
            else printf(" 2_01a %d | 2_01b %d | %d %d %d %d | reached %s\n", acnt, bcnt, casc[0], casc[1], casc[2], casc[3], names[best_stage]);
            fflush(stdout);
        }
        return 0;
    }
    if (mode == "rate") {
        int threads = std::stoi(argv[2]); uint64_t N = std::stoull(argv[3]);
        Thresholds T = parse(argc, argv, 4);
        std::atomic<uint64_t> next(0); std::mutex mu; Counts tot; long pass = 0;
        std::vector<std::pair<uint64_t, Pos>> finals;
        auto t0 = std::chrono::steady_clock::now();
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; t++) pool.emplace_back([&] {
            Counts c; long p = 0; std::vector<Pos> fo; std::vector<std::pair<uint64_t, Pos>> mine;
            for (uint64_t base; (base = next.fetch_add(1 << 16)) < N;)
                for (uint64_t k = base; k < std::min(N, base + (1 << 16)); k++) {
                    uint64_t seed = standalone::mix(k * 0x9e3779b97f4a7c15ULL + 0x5eed);
                    Seed s(seed);
                    if (!(phase_score(s) < T.phase)) continue;
                    p++; fo.clear(); run(s, T, c, fo);
                    for (auto &f : fo) mine.push_back({seed, f});
                }
            std::lock_guard<std::mutex> g(mu);
            tot.seeds += c.seeds; tot.gv1 += c.gv1; tot.a += c.a; tot.gv2 += c.gv2; tot.b += c.b;
            tot.c2a += c.c2a; tot.c2b += c.c2b; tot.c2c += c.c2c; tot.c2d += c.c2d; pass += p;
            finals.insert(finals.end(), mine.begin(), mine.end());
        });
        for (auto &t : pool) t.join();
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        double ps = double(tot.seeds);
        printf("%llu raw seeds, %.0f pass phase (1 in %.0f), %.1f s\n", (unsigned long long)N, ps, N / ps, sec);
        printf("per phase survivor: gv1 %.3f  2_01a %.4f  gv2 %.3f  2_01b %.4f  2a %.5f  2b %.6f  2c %.6f  2d %.7f\n",
               tot.gv1 / ps, tot.a / ps, tot.gv2 / ps, tot.b / ps, tot.c2a / ps, tot.c2b / ps, tot.c2c / ps, tot.c2d / ps);
        for (auto &f : finals) printf("FINAL %lld %d %d %d\n", (long long)f.first, f.second.x, f.second.z, f.second.count);
        return 0;
    }
    fprintf(stderr, "usage: cpipe probe | rate THREADS N [key=value]\n");
    return 2;
}
