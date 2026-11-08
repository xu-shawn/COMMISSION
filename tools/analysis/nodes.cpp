// Vertical-gradient classes of the 0A lattice nodes around each gradvecs_1 output that leads to a known island,
// against those of random lattice positions. Class per node: bottom layer y = -1 and top layer y = +1 ('F'),
// only one of the two ('h'), neither ('.'). Printed for kernel columns 1..4 (x) and rows 0..5 (z).
#include "cpipe.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
using namespace cp;
static const int GY[16] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1};
// class of node (column i, row k) of 0A: 2 favourable, 1 half, 0 neither
static int cls(const Oct &o, int i, int k) {
    int ny = int(o.yo);
    int a = o.p[(o.p[i & 255] + ny) & 255], b = o.p[(o.p[i & 255] + ny + 1) & 255];
    int gb = GY[o.p[(a + k) & 255] & 15], gt = GY[o.p[(b + k) & 255] & 15];
    return (gb == -1) + (gt == 1);
}
int main() {
    std::string line; long long sd; int bx, bz;
    int hist_c[5] = {};  // number of fully favourable central (2x2) nodes, islands
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); if (!(is >> sd >> bx >> bz)) continue;
        Seed s{uint64_t(sd)}; const Oct &o = s.get(0);
        std::vector<Pos> g1; gradvecs_1(s, g1);
        int qx = bx >> 2, qz = bz >> 2; const int P = 131072;
        auto dm = [&](int a, int b) { int t = ((a - b) % P + P) % P; return std::min(t, P - t); };
        // lattice position of each output: nx = (x - x_center) / 512
        int xc = int((2.5f - o.xo) * 512), zc = int((2.5f - o.zo) * 512);
        printf("%lld\n", sd);
        for (auto &p : g1) {
            double d = std::hypot(double(dm(p.x, qx)), double(dm(p.z, qz))) * 4;
            if (d > 3000) continue;
            int nx = (p.x - xc) / 512, nz = (p.z - zc) / 512;
            int nc = 0;
            for (int k = 0; k < 6; k++) {
                printf("   ");
                for (int i = 1; i < 5; i++) { int c = cls(o, nx + i, nz + k); printf("%c", ".hF"[c]); if (k >= 2 && k <= 3 && i >= 2 && i <= 3) nc += c == 2; }
                printf(k == 0 ? "    dist %.0f\n" : "\n", d);
            }
            hist_c[nc]++;
        }
    }
    printf("fully favourable central nodes (of 4) at island outputs:"); for (int c = 0; c < 5; c++) printf(" %d:%d", c, hist_c[c]); printf("\n");
    // random positions of random seeds
    long h[5] = {}, n = 0;
    for (uint64_t k = 0; k < 2000; k++) {
        Seed s{standalone::mix(k + 17)}; const Oct &o = s.get(0);
        for (int nx = 0; nx < 256; nx += 3) for (int nz = 0; nz < 256; nz += 3) {
            int nc = 0; for (int i = 2; i <= 3; i++) for (int kk = 2; kk <= 3; kk++) nc += cls(o, nx + i, nz + kk) == 2;
            h[nc]++; n++;
        }
    }
    printf("random positions:"); for (int c = 0; c < 5; c++) printf(" %d:%.5f", c, double(h[c]) / n); printf("\n");
}
