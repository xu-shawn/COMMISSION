// A candidates (gv1 gate >= -5, score > -15, filter_2_01a count >= 27) on random seeds passing the reshaped
// phase gate, with afull_stats.   afullpop THREADS N_CANDIDATES > sample
#include "afull.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
using namespace cp;
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long want = atol(argv[2]); float fmin = argc > 3 ? atof(argv[3]) : -15.0f; int amin_ = argc > 4 ? atoi(argv[4]) : 27;
    std::mutex mu; std::atomic<long> got{0}, seeds{0};
    std::vector<std::thread> ts;
    for (int th = 0; th < threads; th++) ts.emplace_back([&, th] {
        std::mt19937_64 rng(31337 + th * 7777);
        while (got < want) {
            Seed s{rng()};
            float d[6];
            for (int side = 0; side < 2; side++) for (int o = 0; o < 3; o++) {
                const auto &salt = standalone::octave_salt[-9 + o + 10];
                Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
                float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
                d[o * 2 + side] = std::fabs(f - 0.5f);
            }
            if (!(257.3f*(d[0]*d[0]+d[1]*d[1]) + 47.4f*(d[2]*d[2]+d[3]*d[3]) + 7.5f*(d[4]*d[4]+d[5]*d[5]) < 1.6572f)) continue;
            seeds++;
            std::vector<Pos> g1; GV1 g; g.z0_gate = -5.0f; g.final_ = fmin; gradvecs_1(s, g1, g);
            for (auto &p : g1) {
                int a = two_octave_count(s, 0, p.x, p.z);
                if (a < amin_) continue;
                int st[12]; float m; afull_stats(s, p.x, p.z, st, m);
                // 8-term score of this position, recomputed
                std::lock_guard l(mu);
                printf("%.4f", m); for (int i = 0; i < 12; i++) printf(" %d", st[i]); printf(" %d\n", a);
                got++;
            }
        }
    });
    for (auto &t : ts) t.join();
    fprintf(stderr, "gate-passing seeds %ld, A candidates %ld (score > %g, count >= %d)\n", (long)seeds, (long)got, fmin, amin_);
}
