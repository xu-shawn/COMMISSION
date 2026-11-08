// A joint (both noise stacks) replacement for filter_2_01b. At a gradvecs_2 output, on filter_2_01b's 16x16 grid
// (512-block step), count the samples where
//   J0(L)  = octave 0A + 0B          < L,  L = -0.6, -0.7, -0.8
//   J01(L) = octaves 0A + 0B + 1A + 1B < L,  L = -0.95, -1.05
// and, for comparison, the filter_2_01b count (0B + 1B < -0.55).
//   joint islands < "seed x z area"      per island: the maxima over the gradvecs_2 outputs of its lineages
//   joint random N                       every gradvecs_2 output of random phase-gate survivors
// Upstream: gradvecs_1 with gate -5 and final -14.5 (plus the -16 lineages for islands), filter_2_01a >= 27.
#include "cpipe.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
using namespace cp;

struct Stat { int b, j0[3], j01[2]; };
static Stat stats(Seed &s, int x, int z) {
    Stat r{};
    const float L0[3] = {-0.6f, -0.7f, -0.8f}, L01[2] = {-0.95f, -1.05f};
    for (int zi = 0; zi < 16; zi++) for (int xi = 0; xi < 16; xi++) {
        int px = x + xi * 128 - 960, pz = z + zi * 128 - 960;
        float a0 = octave(s, 0, px, pz), b0 = octave(s, 1, px, pz), a1 = octave(s, 2, px, pz), b1 = octave(s, 3, px, pz);
        r.b += b0 + b1 < -0.55f;
        for (int k = 0; k < 3; k++) r.j0[k] += a0 + b0 < L0[k];
        for (int k = 0; k < 2; k++) r.j01[k] += a0 + b0 + a1 + b1 < L01[k];
    }
    return r;
}
static void print(const char *tag, long long sd, const std::string &area, const Stat &r) {
    printf("%s %lld %s %d %d %d %d %d %d\n", tag, sd, area.c_str(), r.b, r.j0[0], r.j0[1], r.j0[2], r.j01[0], r.j01[1]);
}

int main(int argc, char **argv) {
    std::string mode = argv[1];
    if (mode == "islands") {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream is(line); long long sd; int bx, bz; std::string area;
            if (!(is >> sd >> bx >> bz >> area)) continue;
            Seed s{uint64_t(sd)}; int qx = bx >> 2, qz = bz >> 2;
            GV1 g; g.z0_gate = -1e9f;
            std::vector<Pos> g1; gradvecs_1(s, g1, g);
            const int P = 131072;
            auto dm = [&](int a, int b) { int t = ((a - b) % P + P) % P; return std::min(t, P - t); };
            Stat best{-1, {-1, -1, -1}, {-1, -1}};
            for (auto &p : g1) {
                if (std::hypot(double(dm(p.x, qx)), double(dm(p.z, qz))) * 4 > 3000) continue;
                if (two_octave_count(s, 0, p.x, p.z) < 27) continue;
                std::vector<Pos> g2; gradvecs_2(s, p, g2);
                for (auto &q : g2) {
                    if (std::hypot(double(q.x - qx), double(q.z - qz)) * 4 > 3000) continue;
                    Stat r = stats(s, q.x, q.z);
                    best.b = std::max(best.b, r.b);
                    for (int k = 0; k < 3; k++) best.j0[k] = std::max(best.j0[k], r.j0[k]);
                    for (int k = 0; k < 2; k++) best.j01[k] = std::max(best.j01[k], r.j01[k]);
                }
            }
            print("I", sd, area, best);
        }
        return 0;
    }
    uint64_t N = std::stoull(argv[2]);
    for (uint64_t k = 0; k < N; k++) {
        Seed s{standalone::mix(k * 0x9e3779b97f4a7c15ULL + 31)};
        if (!(phase_score(s) < 1.5f)) continue;
        GV1 g; g.z0_gate = -5.0f; g.final_ = -14.5f;
        std::vector<Pos> g1; gradvecs_1(s, g1, g);
        for (auto &p : g1) {
            if (two_octave_count(s, 0, p.x, p.z) < 27) continue;
            std::vector<Pos> g2; gradvecs_2(s, p, g2);
            for (auto &q : g2) print("R", (long long)s.seed, "0", stats(s, q.x, q.z));
        }
    }
}
