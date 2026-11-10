// filter_2_01a's 256 samples split into the half a warp could evaluate first: the 8 central columns of all 16 rows
// (each lane walks its half row from the centre outwards), and the central 8x8 block.
//   afirst random THREADS N        N phase-gate survivors (record gate): every gradvecs_1 output (gate -5, final -15)
//   afirst list < "seed nx nz"     the octave-0A lattice positions given
// Output per position:  seed nx nz count256 count128 count64 min128 c1..c8   (min128: lowest value among the first
// half; c_s: count over the central 2s columns, i.e. after s samples per lane walking each half row centre-out)
#include "cpipe.hpp"
#include <atomic>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
using namespace cp;

static float phase_new(const Seed &s) {
    float sc = 0; const float w[3] = {257.3f, 47.4f, 7.5f};
    for (int side = 0; side < 2; side++)
        for (int o = 0; o < 3; o++) {
            const auto &salt = standalone::octave_salt[-9 + o + 10];
            Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
            float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
            float d = std::fabs(f - 0.5f); sc += w[o] * d * d;
        }
    return sc;
}
static void grid(Seed &s, int x, int z, int &c256, int &c128, int &c64, float &mn, int cum[8]) {
    c256 = c128 = c64 = 0; mn = 9; for (int i = 0; i < 8; i++) cum[i] = 0;
    for (int zi = 0; zi < 16; zi++)
        for (int xi = 0; xi < 16; xi++) {
            int px = x + xi * 128 - 960, pz = z + zi * 128 - 960;
            float v = octave(s, 0, px, pz) + octave(s, 2, px, pz);
            bool in = v < -0.55f; c256 += in;
            { int step = xi >= 8 ? xi - 8 : 7 - xi; if (in) for (int q = step; q < 8; q++) cum[q]++; }
            if (xi >= 4 && xi < 12) { c128 += in; mn = std::min(mn, v); if (zi >= 4 && zi < 12) c64 += in; }
        }
}
int main(int argc, char **argv) {
    std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "random") {
        int threads = atoi(argv[2]); long N = atol(argv[3]);
        std::atomic<long> got{0}; std::atomic<uint64_t> next{0}; std::mutex mu;
        std::vector<std::thread> ts;
        for (int t = 0; t < threads; t++) ts.emplace_back([&] {
            std::string out; char buf[240];
            while (got < N) {
                uint64_t base = next.fetch_add(1 << 14);
                for (uint64_t k = base; k < base + (1 << 14); k++) {
                    uint64_t seed = standalone::mix(k * 0x9e3779b97f4a7c15ULL + 0xfab1e);
                    Seed s(seed);
                    if (!(phase_new(s) < 1.6572f)) continue;
                    got++;
                    GV1 g; g.z0_gate = -5.0f; g.final_ = -15.0f;
                    std::vector<Pos> g1; gradvecs_1(s, g1, g);
                    const Oct &o = s.get(0); int xc = int((2.5f - o.xo) * 512), zc = int((2.5f - o.zo) * 512);
                    snprintf(buf, sizeof buf, "S %lld %zu\n", (long long)seed, g1.size()); out += buf;
                    for (auto &p : g1) {
                        int a, b, c; float mn; int cum[8]; grid(s, p.x, p.z, a, b, c, mn, cum);
                        snprintf(buf, sizeof buf, "%lld %d %d %d %d %d %.4f %d %d %d %d %d %d %d %d\n", (long long)seed, (p.x - xc) / 512, (p.z - zc) / 512, a, b, c, mn, cum[0], cum[1], cum[2], cum[3], cum[4], cum[5], cum[6], cum[7]); out += buf;
                    }
                }
            }
            std::lock_guard<std::mutex> l(mu); fputs(out.c_str(), stdout);
        });
        for (auto &t : ts) t.join();
        return 0;
    }
    std::vector<std::string> lines; std::string l;
    while (std::getline(std::cin, l)) lines.push_back(l);
    std::vector<std::string> res(lines.size()); std::atomic<size_t> nx_{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < 12; t++) ts.emplace_back([&] {
        for (size_t i; (i = nx_++) < lines.size();) {
            std::istringstream is(lines[i]); long long sd; int nx, nz; if (!(is >> sd >> nx >> nz)) continue;
            Seed s{uint64_t(sd)}; const Oct &o = s.get(0);
            int xc = int((2.5f - o.xo) * 512), zc = int((2.5f - o.zo) * 512);
            int a, b, c; float mn; int cum[8]; grid(s, xc + nx * 512, zc + nz * 512, a, b, c, mn, cum);
            char buf[200]; snprintf(buf, sizeof buf, "%lld %d %d %d %d %d %.4f %d %d %d %d %d %d %d %d\n", sd, nx, nz, a, b, c, mn, cum[0], cum[1], cum[2], cum[3], cum[4], cum[5], cum[6], cum[7]); res[i] = buf;
        }
    });
    for (auto &t : ts) t.join();
    for (auto &r : res) fputs(r.c_str(), stdout);
}
