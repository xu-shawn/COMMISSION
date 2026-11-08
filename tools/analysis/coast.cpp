// What ends a mushroom island? For each island ("seed x z area" lines, block coordinates) flood the island at
// 16-block resolution (C < -1.05, with shift, 4-connected, no bridging) and compare, octave group by group
// (octave 0, octave 1, octave 2, octaves 3-8; A and B summed), the mean contribution over the island with the
// mean over the cells just outside its coast. The group whose value rises most from inside to coast is what
// bounds the island. Also: the share of coast cells where octave 0 alone is above -0.6 (the octave-0 basin has
// ended there), and the island's extent in octave-0 lattice cells (area / 2048^2).
// Output: seed area in0 in1 in2 in3 coast0 coast1 coast2 coast3 frac_oct0_edge
#include "noise.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace standalone;

constexpr int R = 400, W = 2 * R + 1, G = 4;   // +-6400 blocks at 16 blocks

int main() {
    std::string line;
    printf("seed area in0 in1 in2 in3 coast0 coast1 coast2 coast3 frac_oct0_edge\n");
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); long long sd; int bx, bz; std::string area = "0";
        if (!(is >> sd >> bx >> bz)) continue;
        is >> area;
        Sampler s{uint64_t(sd)};
        const auto &C = s.get(Continentalness), &Sh = s.get(Shift);
        const int qx = bx >> 2, qz = bz >> 2;
        const size_t N = size_t(W) * W;
        std::vector<float> grp[4];
        for (auto &g : grp) g.assign(N, 0.f);
        std::vector<uint8_t> inisl(N, 0);
        for (int j = 0; j < W; j++) for (int i = 0; i < W; i++) {
            int x = qx + (i - R) * G, z = qz + (j - R) * G;
            double px = x + 4. * Sh.sample(x, 0, z), pz = z + 4. * Sh.sample(z, x, 0);
            double tot = 0;
            for (int side = 0; side < 2; side++) {
                double sc = side ? 337. / 331. : 1.;
                for (int o = 0; o < 9; o++) {
                    const auto &p = C.octave[side][o];
                    double v = p.weight * p.sample(px * sc * p.frequency, 0, pz * sc * p.frequency) * C.factor;
                    grp[o < 3 ? o : 3][size_t(j) * W + i] += float(v); tot += v;
                }
            }
            inisl[size_t(j) * W + i] = tot < -1.05;
        }
        // flood the island from the lowest in-island cell near the point
        int start = -1; float bv = 1e9f;
        for (int j = R - 60; j <= R + 60; j++) for (int i = R - 60; i <= R + 60; i++) {
            size_t k = size_t(j) * W + i; float v = grp[0][k] + grp[1][k] + grp[2][k] + grp[3][k];
            if (inisl[k] && v < bv) { bv = v; start = int(k); }
        }
        if (start < 0) { printf("%lld %s nan\n", sd, area.c_str()); continue; }
        std::vector<uint8_t> lab(N, 0); std::vector<int> st{start}; lab[start] = 1;
        double sin[4] = {}, sco[4] = {}; long nin = 0, nco = 0, nedge = 0;
        while (!st.empty()) {
            int c = st.back(); st.pop_back(); nin++;
            for (int g = 0; g < 4; g++) sin[g] += grp[g][c];
            int i = c % W, j = c / W;
            const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                int ii = i + di[k], jj = j + dj[k];
                if (ii < 0 || jj < 0 || ii >= W || jj >= W) continue;
                size_t d = size_t(jj) * W + ii;
                if (lab[d]) continue;
                if (inisl[d]) { lab[d] = 1; st.push_back(int(d)); }
                else { lab[d] = 2; nco++; for (int g = 0; g < 4; g++) sco[g] += grp[g][d]; nedge += grp[0][d] > -0.6f; }
            }
        }
        printf("%lld %s %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.3f\n", sd, area.c_str(), sin[0] / nin, sin[1] / nin, sin[2] / nin, sin[3] / nin,
               sco[0] / nco, sco[1] / nco, sco[2] / nco, sco[3] / nco, double(nedge) / nco);
        fflush(stdout);
    }
}
