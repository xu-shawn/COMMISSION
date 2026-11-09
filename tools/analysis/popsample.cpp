// Population of gradvecs_1 outputs on random seeds that pass the (reshaped) phase gate: for every lattice
// position with gate >= -5 and 8-term score > FMIN, its score and filter_2_01a count.
//   popsample THREADS SEEDS FMIN > sample      (stderr: gate-passing seeds)
#include "cpipe.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
using namespace cp;
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long want = atol(argv[2]); float fmin = atof(argv[3]);
    std::mutex mu; std::atomic<long> got{0};
    std::vector<std::thread> ts;
    for (int th = 0; th < threads; th++) ts.emplace_back([&, th] {
        std::mt19937_64 rng(4242 + th * 1000003);
        while (got < want) {
            uint64_t sd = rng(); Seed s{sd};
            float d[6];
            for (int side = 0; side < 2; side++)
                for (int o = 0; o < 3; o++) {
                    const auto &salt = standalone::octave_salt[-9 + o + 10];
                    Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
                    float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
                    d[o * 2 + side] = std::fabs(f - 0.5f);
                }
            float sc = 257.3f * (d[0]*d[0] + d[1]*d[1]) + 47.4f * (d[2]*d[2] + d[3]*d[3]) + 7.5f * (d[4]*d[4] + d[5]*d[5]);
            if (!(sc < 1.6572f)) continue;
            got++;
            const Oct &o = s.get(0);
            float conv[2][256][4];
            for (int t = 0; t < 256; t++) for (int dx = 1; dx < 5; dx++) for (int y = 0; y < 2; y++) {
                float c = 0; for (int dz = 0; dz < 6; dz++) c += host_kernel_0A[y][dx][dz][o.p[(t + dz) & 255] & 15]; conv[y][t][dx - 1] = c; }
            int xc = int((2.5f - o.xo) * 512), zc = int((2.5f - o.zo) * 512), ny = int(o.yo);
            int idx[2][262];
            for (int n = 0; n < 262; n++) for (int y = 0; y < 2; y++) idx[y][n] = o.p[(o.p[n & 255] + ny + y) & 255];
            std::vector<std::pair<float, int>> out;
            for (int nx = 0; nx < 256; nx++) for (int nz = 0; nz < 256; nz++) {
                auto C = [&](int y, int i, int col) { return conv[y][(idx[y][nx + i] + nz) & 255][col]; };
                float g = C(0, 2, 1) + C(0, 3, 2);
                if (!(g >= -5.0f)) continue;
                float f = (C(0, 2, 1) + C(0, 3, 2)) + C(0, 1, 0) + C(0, 4, 3) + C(1, 1, 0) + C(1, 2, 1) + C(1, 3, 2) + C(1, 4, 3);
                if (!(f > fmin)) continue;
                out.push_back({f, two_octave_count(s, 0, xc + nx * 512, zc + nz * 512)});
            }
            std::lock_guard l(mu);
            for (auto &p : out) printf("%.4f %d\n", p.first, p.second);
        }
    });
    for (auto &t : ts) t.join();
    fprintf(stderr, "gate-passing seeds %ld\n", (long)got);
}
