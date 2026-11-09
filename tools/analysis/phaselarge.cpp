// Vertical-offset distances d0A d0B d1A d1B d2A d2B for LARGE-biome continentalness.
//   phaselarge seeds < "seed ..." lines        print "seed d.. rest-of-line"
//   phaselarge sample THREADS N                 random seeds inside both phase gates (old and reshaped)
#include "noise.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <array>
using standalone::Random;
static const uint64_t CL[2] = {0x9a3f51a113fce8dcULL, 0xee2dbd157e5dcdadULL};      // md5 "minecraft:continentalness_large"
static const uint64_t SALT[3][2] = {{0x0fd787bfbc403ec3ULL, 0x74a4a31ca21b48b8ULL},  // octave_-11
                                    {0x36d326eed40efeb2ULL, 0x5be9ce18223c636aULL},  // octave_-10
                                    {0x082fe255f8be6631ULL, 0x4e96119e22dedc81ULL}}; // octave_-9
static void ds(uint64_t sd, float d[6]) {
    Random world(sd); uint64_t a = world.next(), b = world.next();
    Random c(a ^ CL[0], b ^ CL[1]);
    uint64_t fa[2], fb[2];
    for (int side = 0; side < 2; side++) { fa[side] = c.next(); fb[side] = c.next(); }
    for (int side = 0; side < 2; side++)
        for (int o = 0; o < 3; o++) {
            Random r(fa[side] ^ SALT[o][0], fb[side] ^ SALT[o][1]); r.next();
            float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
            d[o * 2 + side] = std::fabs(f - 0.5f);
        }
}
static bool both(const float *d) {
    float q0 = d[0]*d[0] + d[1]*d[1], q1 = d[2]*d[2] + d[3]*d[3], q2 = d[4]*d[4] + d[5]*d[5];
    return 350.f*q0 + 55.f*q1 + 4.f*q2 < 1.5f && 257.3f*q0 + 47.4f*q1 + 7.5f*q2 < 1.6572f;
}
int main(int argc, char **argv) {
    std::string mode = argv[1];
    if (mode == "seeds") {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream is(line); long long sd; if (!(is >> sd)) continue;
            std::string rest; std::getline(is, rest);
            float d[6]; ds(uint64_t(sd), d);
            printf("%lld %.5f %.5f %.5f %.5f %.5f %.5f %d%s\n", sd, d[0], d[1], d[2], d[3], d[4], d[5], both(d) ? 1 : 0, rest.c_str());
        }
        return 0;
    }
    int threads = atoi(argv[2]); long n = atol(argv[3]); std::mutex mu; std::vector<std::thread> ts;
    for (int t = 0; t < threads; t++) ts.emplace_back([&, t] {
        std::mt19937_64 rng(777 + t); std::vector<std::array<float, 6>> out;
        for (long i = t; i < n; i += threads) { float d[6]; ds(rng(), d); if (both(d)) out.push_back({d[0], d[1], d[2], d[3], d[4], d[5]}); }
        std::lock_guard l(mu); for (auto &a : out) printf("%.5f %.5f %.5f %.5f %.5f %.5f\n", a[0], a[1], a[2], a[3], a[4], a[5]);
    });
    for (auto &t : ts) t.join();
}
