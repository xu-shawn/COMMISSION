// Skeleton basins of mushroom islands: the area around an island where only the lowest continentalness octaves
// (both noise stacks) are already below a level. Reads "seed x z [area]" lines (block coordinates inside or near
// the island) and prints, per island, the 4-connected area (in millions of blocks) containing the point where
//   s0  = octave 0 (A + B)          is below  -0.60 -0.70 -0.80
//   s01 = octaves 0 and 1 (A + B)   is below  -0.95 -1.05 -1.15
// sampled every 64 blocks within +-8000 blocks, no shift. Output: seed area s0_60 s0_70 s0_80 s01_95 s01_105 s01_115
#include "noise.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace standalone;

constexpr int R = 125, W = 2 * R + 1, G = 16;   // grid half-width (cells) and step (quarts)

static double basin(const std::vector<float> &f, float level, int ci, int cj) {
    // start from the lowest cell within 1000 blocks of the given point
    int best = -1; float bv = 1e9f;
    for (int j = cj - 16; j <= cj + 16; j++) for (int i = ci - 16; i <= ci + 16; i++) {
        float v = f[size_t(j) * W + i]; if (v < bv) { bv = v; best = j * W + i; }
    }
    if (!(bv < level)) return 0;
    std::vector<uint8_t> seen(f.size(), 0); std::vector<int> st{best}; seen[best] = 1; long n = 0;
    while (!st.empty()) {
        int c = st.back(); st.pop_back(); n++;
        int i = c % W, j = c / W;
        const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int ii = i + di[k], jj = j + dj[k];
            if (ii < 0 || jj < 0 || ii >= W || jj >= W) continue;
            size_t d = size_t(jj) * W + ii;
            if (!seen[d] && f[d] < level) { seen[d] = 1; st.push_back(int(d)); }
        }
    }
    return n * double(G * 4) * (G * 4) / 1e6;
}

int main() {
    std::string line;
    printf("seed area s0_60 s0_70 s0_80 s01_95 s01_105 s01_115\n");
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); long long sd; int bx, bz; std::string area = "0";
        if (!(is >> sd >> bx >> bz)) continue;
        is >> area;
        Sampler s{uint64_t(sd)};
        const auto &C = s.get(Continentalness);
        std::vector<float> f0(size_t(W) * W), f01(size_t(W) * W);
        const int qx = bx >> 2, qz = bz >> 2;
        for (int j = 0; j < W; j++) for (int i = 0; i < W; i++) {
            double px = qx + (i - R) * G, pz = qz + (j - R) * G, v0 = 0, v1 = 0;
            for (int side = 0; side < 2; side++) {
                double sc = side ? 337. / 331. : 1.;
                for (int o = 0; o < 2; o++) {
                    const auto &p = C.octave[side][o];
                    double v = p.weight * p.sample(px * sc * p.frequency, 0, pz * sc * p.frequency) * C.factor;
                    if (o == 0) v0 += v; v1 += v;
                }
            }
            f0[size_t(j) * W + i] = float(v0); f01[size_t(j) * W + i] = float(v1);
        }
        printf("%lld %s %.3f %.3f %.3f %.3f %.3f %.3f\n", sd, area.c_str(), basin(f0, -0.6f, R, R), basin(f0, -0.7f, R, R), basin(f0, -0.8f, R, R),
               basin(f01, -0.95f, R, R), basin(f01, -1.05f, R, R), basin(f01, -1.15f, R, R));
        fflush(stdout);
    }
}
