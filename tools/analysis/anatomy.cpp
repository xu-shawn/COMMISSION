// Anatomy of a mushroom island: what each continentalness octave contributes.
//   anatomy < "seed x z ..." lines (block coordinates of a point in the island)
// Floods the island (C < -1.05, 4-connected, quart resolution, no bridging) from the given point and prints,
// per octave o = 0..8 and side A/B: the y-phase d = |frac(yo) - 0.5|, the mean contribution over the island,
// the contribution at the island centroid, and for octaves 0..2 the share of lattice nodes inside the island
// whose vertical gradients are favourable (bottom y = -1, top y = +1).
#include "noise.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace standalone;

int main() {
    std::string line;
    printf("seed,area,meanC");
    for (int o = 0; o < 9; o++) for (int s = 0; s < 2; s++) printf(",d%d%c", o, "AB"[s]);
    for (int o = 0; o < 9; o++) for (int s = 0; s < 2; s++) printf(",m%d%c", o, "AB"[s]);
    for (int o = 0; o < 9; o++) for (int s = 0; s < 2; s++) printf(",c%d%c", o, "AB"[s]);
    for (int o = 0; o < 3; o++) for (int s = 0; s < 2; s++) printf(",n%d%c,f%d%c", o, "AB"[s], o, "AB"[s]);
    printf("\n");
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); long long sd; int bx, bz;
        if (!(is >> sd >> bx >> bz)) continue;
        uint64_t seed = uint64_t(sd);
        Sampler s(seed);
        const auto &C = s.get(Continentalness), &Sh = s.get(Shift);
        auto pos = [&](int qx, int qz, double &px, double &pz) {
            px = qx + 4. * Sh.sample(qx, 0, qz); pz = qz + 4. * Sh.sample(qz, qx, 0);
        };
        const int R = 1400;                           // quarts: +-5600 blocks around the start point
        const int W = 2 * R + 1;
        std::vector<uint8_t> st(size_t(W) * W, 0);    // 0 unknown, 1 out, 2 in
        int q0x = bx >> 2, q0z = bz >> 2;
        auto in = [&](int i, int j) {
            uint8_t &v = st[size_t(j) * W + i];
            if (!v) { double px, pz; pos(q0x + i - R, q0z + j - R, px, pz); v = int64_t(10000.f * float(C.sample(px, 0, pz))) < -10500 ? 2 : 1; }
            return v == 2;
        };
        // flood every component that meets a 64-block grid within +-3000 blocks; keep the largest
        std::vector<int> stack, cells, best; std::vector<int> label(size_t(W) * W, 0); int nl = 0, bestl = 0;
        for (int j = R - 750; j <= R + 750; j += 16) for (int i = R - 750; i <= R + 750; i += 16) {
            if (label[size_t(j) * W + i] || !in(i, j)) continue;
            label[size_t(j) * W + i] = ++nl; stack.push_back(j * W + i); cells.clear();
            while (!stack.empty()) {
                int c = stack.back(); stack.pop_back(); cells.push_back(c);
                int ci = c % W, cj = c / W;
                const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int ii = ci + di[k], jj = cj + dj[k];
                    if (ii < 0 || jj < 0 || ii >= W || jj >= W) continue;
                    size_t d = size_t(jj) * W + ii;
                    if (!label[d] && in(ii, jj)) { label[d] = nl; stack.push_back(int(d)); }
                }
            }
            if (cells.size() > best.size()) { best.swap(cells); bestl = nl; }
        }
        if (best.empty()) { printf("%lld,0\n", sd); continue; }
        cells = best;
        std::vector<uint8_t> seen(size_t(W) * W, 0);
        for (int c : cells) seen[c] = 1;
        // per-octave mean contribution over the island (every 4th cell each way is plenty)
        double m[9][2] = {}, meanC = 0; long n = 0; double cx = 0, cz = 0;
        for (int c : cells) {
            int i = c % W, j = c / W;
            cx += i; cz += j;
            if ((i & 3) || (j & 3)) continue;
            double px, pz; pos(q0x + i - R, q0z + j - R, px, pz);
            double tot = 0;
            for (int side = 0; side < 2; side++) {
                double sc = side ? 337. / 331. : 1.;
                for (int o = 0; o < 9; o++) {
                    const auto &p = C.octave[side][o];
                    double v = p.weight * p.sample(px * sc * p.frequency, 0, pz * sc * p.frequency) * C.factor;
                    m[o][side] += v; tot += v;
                }
            }
            meanC += tot; n++;
        }
        cx /= cells.size(); cz /= cells.size();
        double area = double(cells.size()) * 16;
        printf("%lld,%.0f,%.4f", sd, area, meanC / n);
        for (int o = 0; o < 9; o++) for (int side = 0; side < 2; side++) printf(",%.4f", std::abs(C.octave[side][o].fy - 0.5));
        for (int o = 0; o < 9; o++) for (int side = 0; side < 2; side++) printf(",%.4f", m[o][side] / n);
        double px, pz; pos(q0x + int(cx) - R, q0z + int(cz) - R, px, pz);
        for (int o = 0; o < 9; o++) for (int side = 0; side < 2; side++) {
            double sc = side ? 337. / 331. : 1.; const auto &p = C.octave[side][o];
            printf(",%.4f", p.weight * p.sample(px * sc * p.frequency, 0, pz * sc * p.frequency) * C.factor);
        }
        // lattice nodes inside the island: node (I, K) of octave o sits where px*sc*freq + ox = I
        for (int o = 0; o < 3; o++) for (int side = 0; side < 2; side++) {
            const auto &p = C.octave[side][o]; double sc = side ? 337. / 331. : 1.;
            double f = sc * p.frequency;                       // lattice units per quart
            double lo_x = (q0x - R) * f + p.ox, hi_x = (q0x + R) * f + p.ox;
            double lo_z = (q0z - R) * f + p.oz, hi_z = (q0z + R) * f + p.oz;
            int nin = 0, fav = 0;
            for (int I = int(std::ceil(lo_x)); I <= int(std::floor(hi_x)); I++)
                for (int K = int(std::ceil(lo_z)); K <= int(std::floor(hi_z)); K++) {
                    int i = int(std::lround((I - p.ox) / f)) - (q0x - R), j = int(std::lround((K - p.oz) / f)) - (q0z - R);
                    if (i < 0 || j < 0 || i >= W || j >= W || !seen[size_t(j) * W + i]) continue;
                    nin++;
                    int a = p.permutation[I & 255];
                    int gb = p.permutation[(p.permutation[(a + p.iy) & 255] + K) & 255] & 15;
                    int gt = p.permutation[(p.permutation[(a + p.iy + 1) & 255] + K) & 255] & 15;
                    static const int gy[16] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1};
                    if (gy[gb] == -1 && gy[gt] == 1) fav++;
                }
            printf(",%d,%.2f", nin, nin ? double(fav) / nin : 0.);
        }
        printf("\n"); fflush(stdout);
    }
}
