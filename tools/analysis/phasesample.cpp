// Vertical-offset distances (d0A d0B d1A d1B d2A d2B) of random seeds whose quadratic phase score
// 350 d0^2 + 55 d1^2 + 4 d2^2 (summed over both stacks) is below LIMIT, and the number of seeds drawn.
//   phasesample THREADS N LIMIT > sample
#include "cpipe.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
using namespace cp;
int main(int argc, char **argv) {
    int threads = atoi(argv[1]); long n = atol(argv[2]); float limit = atof(argv[3]);
    std::mutex mu; std::atomic<long> drawn{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; t++) ts.emplace_back([&, t] {
        std::mt19937_64 rng(1234567 + t * 7919);
        std::vector<std::array<float, 10>> out;
        for (long i = t; i < n; i += threads) {
            Seed s{rng()};
            float d[10];
            for (int side = 0; side < 2; side++)
                for (int o = 0; o < 5; o++) {
                    const auto &salt = standalone::octave_salt[-9 + o + 10];
                    Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
                    float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
                    d[o * 2 + side] = std::fabs(f - 0.5f);
                }
            float sc = 350.f * (d[0]*d[0] + d[1]*d[1]) + 55.f * (d[2]*d[2] + d[3]*d[3]) + 4.f * (d[4]*d[4] + d[5]*d[5]);
            if (sc < limit) out.push_back({d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9]});
        }
        std::lock_guard l(mu);
        for (auto &a : out) { for (int k = 0; k < 10; k++) printf(k ? " %.5f" : "%.5f", a[k]); printf("\n"); }
    });
    for (auto &t : ts) t.join();
    fprintf(stderr, "drawn %ld\n", n);
}
