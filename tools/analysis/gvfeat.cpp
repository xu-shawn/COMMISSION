// gradvecs_1-level features of the octave-0A lattice position that best matches each island (block coordinates
// "seed x z area"): the 8-term score, its central (2x2 nodes, both layers) and ring parts, the number of fully
// favourable nodes among the 2x2 core and among the 4x6 window, per island. Same for octave 0B at the island's
// own world position (B lattice, nearest window). Output: seed area scA cenA ringA coreA winA scB cenB ringB coreB winB
#include "cpipe.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
using namespace cp;
static const int GY[16] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1};
struct F { float sc, cen, ring; int core, win; };
// best window of octave k (0 = 0A, 1 = 0B) whose centre (lattice nx+2.5, nz+2.5) lies within 1.2 cells of the
// island point (lattice coordinates lx, lz)
static F best(const Oct &o, int k, double lx, double lz) {
    const float (*K)[6][6][16] = k ? host_kernel_0B : host_kernel_0A;
    int ny = int(o.yo); F b{-1e9f, 0, 0, 0, 0};
    for (int nx = int(std::floor(lx - 2.5)) - 1; nx <= int(std::floor(lx - 2.5)) + 2; nx++)
        for (int nz = int(std::floor(lz - 2.5)) - 1; nz <= int(std::floor(lz - 2.5)) + 2; nz++) {
            if (std::hypot(nx + 2.5 - lx, nz + 2.5 - lz) > 1.2) continue;
            float cen = 0, ring = 0; int core = 0, win = 0;
            for (int dx = 1; dx < 5; dx++) for (int dz = 0; dz < 6; dz++) {
                int fav = 0;
                for (int y = 0; y < 2; y++) {
                    int a = o.p[(o.p[(nx + dx) & 255] + ny + y) & 255];
                    int g = o.p[(a + nz + dz) & 255] & 15;
                    float v = K[y][dx][dz][g];
                    if ((dx == 2 || dx == 3) && (dz == 2 || dz == 3)) cen += v; else ring += v;
                    fav += (y == 0) ? GY[g] == -1 : GY[g] == 1;
                }
                if (fav == 2) { win++; if ((dx == 2 || dx == 3) && (dz == 2 || dz == 3)) core++; }
            }
            if (cen + ring > b.sc) b = {cen + ring, cen, ring, core, win};
        }
    return b;
}
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is(line); long long sd; long x, z; std::string area;
        if (!(is >> sd >> x >> z >> area)) continue;
        Seed s{uint64_t(sd)};
        const Oct &a = s.get(0), &b = s.get(1);
        double q_x = x / 4.0, q_z = z / 4.0;
        Cfg ca = cfg(0), cb = cfg(1);
        F fa = best(a, 0, q_x * ca.in + a.xo, q_z * ca.in + a.zo);
        F fb = best(b, 1, q_x * cb.in + b.xo, q_z * cb.in + b.zo);
        printf("%lld %s %.2f %.2f %.2f %d %d %.2f %.2f %.2f %d %d\n", sd, area.c_str(), fa.sc, fa.cen, fa.ring, fa.core, fa.win, fb.sc, fb.cen, fb.ring, fb.core, fb.win);
    }
}
