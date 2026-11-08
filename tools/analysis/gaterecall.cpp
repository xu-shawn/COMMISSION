// Recall of phase gates under the real (correlated) joint distribution of octave y-phases, taking the island
// density per seed from docs/search-economics.md: exp(-379 d0^2 - 106 d1^2 - 5.3 d2^2) summed over both sides.
#include "noise.hpp"
#include <cstdio>
#include <string>
#include <vector>
using namespace standalone;
static void phases(uint64_t seed, double d[2][3]) {
    Random world(seed); uint64_t a = world.next(), b = world.next();
    const auto &cf = configs[Continentalness]; Random c(a ^ cf.a, b ^ cf.b);
    for (int side = 0; side < 2; side++) {
        uint64_t fa = c.next(), fb = c.next();
        for (int o = 0; o < 3; o++) {
            const auto &salt = octave_salt[cf.first + o + 10];
            Random r(fa ^ salt[0], fb ^ salt[1]); r.next();
            double y = r.uniform() * 256.; d[side][o] = std::abs(y - std::floor(y) - .5);
        }
    }
}
int main(int argc, char **argv) {
    uint64_t N = std::stoull(argv[1]);
    const double tw[] = {1.25, 1.5, 2, 2.5, 3, 4, 5, 6, 8};
    double wsum = 0, wpass[9] = {}; long npass[9] = {};
    for (uint64_t k = 0; k < N; k++) {
        double d[2][3]; phases(mix(k * 0x9e3779b97f4a7c15ULL + 777), d);
        double dens = 0, sc = 0;
        for (int s = 0; s < 2; s++) { dens += 379 * d[s][0] * d[s][0] + 106 * d[s][1] * d[s][1] + 5.3 * d[s][2] * d[s][2];
                                      sc += 350 * d[s][0] * d[s][0] + 55 * d[s][1] * d[s][1] + 4 * d[s][2] * d[s][2]; }
        double w = std::exp(-dens); wsum += w;
        for (int t = 0; t < 9; t++) if (sc < tw[t]) { wpass[t] += w; npass[t]++; }
    }
    for (int t = 0; t < 9; t++) printf("score < %.2f: pass 1 in %6.0f  recall %.3f\n", tw[t], double(N) / npass[t], wpass[t] / wsum);
}
