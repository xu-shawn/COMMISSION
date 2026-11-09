// Stage scores of every lineage that can lead the pipeline to a known island, with all thresholds off, so that
// any threshold setting can be applied offline:  island found  <=>  phase <= P  and some lineage passes every stage.
//
//   lineage < islands      islands: "seed x z area" (blocks); output one line per lineage:
//   seed area phase  nx nz gate8 final8  amin  gv2gate gv2final  joint  c2a c2b c2c c2d  j64 j16  d0A d0B d1A d1B d2A d2B  P0 P1 Ob Ot (gv1 term groups)
//
// A lineage is an octave-0A lattice cell within 2 cells of the island (modulo the stack-A period), followed to
// its periodic image nearest the island. Cascade counts are computed only for lineages that pass loose bounds
// (final8 > -18, amin >= 14, gv2final > -23, joint >= 8); others print -1 there.
#include "afull.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
using namespace cp;

struct GV1Ctx { float conv[2][256][4]; int idx[2][262]; int xc, zc; };
static void gv1_ctx(Seed &s, GV1Ctx &c) {
    const Oct &o = s.get(0);
    for (int t = 0; t < 256; t++)
        for (int dx = 1; dx < 5; dx++)
            for (int y = 0; y < 2; y++) {
                float v = 0;
                for (int dz = 0; dz < 6; dz++) v += host_kernel_0A[y][dx][dz][o.p[(t + dz) & 255] & 15];
                c.conv[y][t][dx - 1] = v;
            }
    c.xc = int((2.5f - o.xo) * 512); c.zc = int((2.5f - o.zo) * 512);
    int ny = int(o.yo);
    for (int n = 0; n < 262; n++) for (int y = 0; y < 2; y++) c.idx[y][n] = o.p[(o.p[n & 255] + ny + y) & 255];
}
static void gv1_terms(const GV1Ctx &c, int nx, int nz, float tm[8]) {   // layer 0 x=1..4, layer 1 x=1..4
    for (int y = 0; y < 2; y++) for (int i = 1; i <= 4; i++) tm[y * 4 + i - 1] = c.conv[y][(c.idx[y][nx + i] + nz) & 255][i - 1];
}
static void gv1_score(const GV1Ctx &c, int nx, int nz, float &gate, float &fin, float g[4]) {
    auto C = [&](int y, int i, int col) { return c.conv[y][(c.idx[y][nx + i] + nz) & 255][col]; };
    gate = C(0, 2, 1) + C(0, 3, 2);
    fin = (C(0, 2, 1) + C(0, 3, 2)) + C(0, 1, 0) + C(0, 4, 3) + C(1, 1, 0) + C(1, 2, 1) + C(1, 3, 2) + C(1, 4, 3);
    g[0] = gate; g[1] = C(1, 2, 1) + C(1, 3, 2); g[2] = C(0, 1, 0) + C(0, 4, 3); g[3] = C(1, 1, 0) + C(1, 4, 3);
}
struct GV2Ctx { float conv[2][6][256]; };
static void gv2_ctx(Seed &s, GV2Ctx &c) {
    const Oct &o = s.get(1);
    for (int V = 0; V < 256; V++)
        for (int dx = 0; dx < 6; dx++)
            for (int y = 0; y < 2; y++) {
                float v = 0;
                for (int dz = 0; dz < 6; dz++) v += host_kernel_0B[y][dx][dz][o.p[(V + dz) & 255] & 15];
                c.conv[y][dx][V] = v;
            }
}
static void gv2_score(Seed &s, const GV2Ctx &c, int x, int z, float &gate, float &fin) {
    const Oct &o = s.get(1); const float fb = cfg(1).in; const int ny = int(o.yo);
    int cx = int(std::floor(float(x) * fb + o.xo - 2.0f)) & 255, row = int(std::floor(float(z) * fb + o.zo - 2.0f)) & 255;
    int h[2][6];
    for (int dx = 0; dx < 6; dx++) { int ix = o.p[(cx + dx) & 255]; for (int y = 0; y < 2; y++) h[y][dx] = o.p[(ix + ny + y) & 255]; }
    gate = c.conv[0][2][(h[0][2] + row) & 255] + c.conv[0][3][(h[0][3] + row) & 255] + c.conv[1][2][(h[1][2] + row) & 255] + c.conv[1][3][(h[1][3] + row) & 255];
    fin = 0; for (int i = 0; i < 6; i++) { fin += c.conv[0][i][(h[0][i] + row) & 255]; fin += c.conv[1][i][(h[1][i] + row) & 255]; }
}
// full count, and the counts on two 64-sample subsets: every other row and column, and rows 1, 5, 9, 13
static int joint_count(Seed &s, int x, int z, int &j64, int &j16) {
    int n = 0; j64 = 0; j16 = 0;
    for (int zi = 0; zi < 16; zi++)
        for (int xi = 0; xi < 16; xi++) {
            int px = x + xi * 128 - 960, pz = z + zi * 128 - 960;
            float v = octave(s, 0, px, pz) + octave(s, 1, px, pz) + octave(s, 2, px, pz) + octave(s, 3, px, pz);
            bool in = v < -1.05f;
            n += in;
            if (xi % 2 == 0 && zi % 2 == 0) j64 += in;
            if (zi % 4 == 1) j16 += in;   // four whole rows (64 samples)
        }
    return n;
}
// distance of each octave's vertical offset from the middle of its lattice layer: 0A 0B 1A 1B 2A 2B
static void phase_ds(const Seed &s, float d[6]) {
    for (int side = 0; side < 2; side++)
        for (int o = 0; o < 3; o++) {
            const auto &salt = standalone::octave_salt[-9 + o + 10];
            Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
            float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
            d[o * 2 + side] = std::fabs(f - 0.5f);
        }
}
static int casc(Seed &s, int noct, int range, int samples, bool move, Pos &p) {
    Pos q = p; if (!cascade(s, noct, range, samples, 1, move, q)) return 0; p = q; return q.count;
}

static std::string process(const std::string &line) {
    std::istringstream is(line); long long sd; long bx, bz; double area = 0;
    if (!(is >> sd >> bx >> bz)) return "";
    is >> area;
    Seed s{uint64_t(sd)};
    const float ph = phase_score(s);
    float dd[6]; phase_ds(s, dd);
    GV1Ctx g1; gv1_ctx(s, g1); GV2Ctx g2; gv2_ctx(s, g2);
    const long P = 131072;   // stack-A period, biome units
    const long qx = bx >> 2, qz = bz >> 2;
    // lattice cell of the island's A position
    auto cell = [&](long q, int c0) { long r = ((q - c0) % P + P) % P; return int((r + 256) / 512) & 255; };
    int cnx = cell(qx, g1.xc), cnz = cell(qz, g1.zc);
    std::string out;
    char buf[512];
    for (int dx = -2; dx <= 2; dx++)
        for (int dz = -2; dz <= 2; dz++) {
            int nx = (cnx + dx) & 255, nz = (cnz + dz) & 255;
            float gate, fin, gg[4]; gv1_score(g1, nx, nz, gate, fin, gg);
            float tm[8]; gv1_terms(g1, nx, nz, tm);
            long ax = g1.xc + long(nx) * 512, az = g1.zc + long(nz) * 512;   // A candidate, biome units
            int amin = two_octave_count(s, 0, int(ax), int(az));
            int af[12] = {0}; float am = 0;
            if (fin > -17.f && amin >= 22) afull_stats(s, int(ax), int(az), af, am);
            long kx = std::lround(double(qx - ax) / P), kz = std::lround(double(qz - az) / P);
            long ix = ax + kx * P, iz = az + kz * P;                          // the image nearest the island
            float g2g, g2f; gv2_score(s, g2, int(ix), int(iz), g2g, g2f);
            int j = -1, c2a = -1, c2b = -1, c2c = -1, c2d = -1, j64 = -1, j16 = -1;
            if (fin > -18.f && amin >= 14 && g2f > -23.f) {
                j = joint_count(s, int(ix), int(iz), j64, j16);
                if (j >= 8) {
                    Pos r{int(ix), int(iz), 0};
                    c2a = casc(s, 12, 8192, 256, true, r);
                    c2b = c2a ? casc(s, 14, 8192, 1024, true, r) : 0;
                    c2c = c2b ? casc(s, 16, 10240, 4096, false, r) : 0;
                    c2d = c2c ? casc(s, 18, 10240, 16384, false, r) : 0;
                }
            }
            snprintf(buf, sizeof buf, "%lld %.0f %.4f %d %d %.4f %.4f %d %.4f %.4f %d %d %d %d %d %d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f  %.4f %d %d %d %d %d %d %d %d %d %d %d %d  %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
                     sd, area, ph, nx, nz, gate, fin, amin, g2g, g2f, j, c2a, c2b, c2c, c2d, j64, j16, dd[0], dd[1], dd[2], dd[3], dd[4], dd[5], gg[0], gg[1], gg[2], gg[3], am, af[0], af[1], af[2], af[3], af[4], af[5], af[6], af[7], af[8], af[9], af[10], af[11], tm[0], tm[1], tm[2], tm[3], tm[4], tm[5], tm[6], tm[7]);
            out += buf;
        }
    return out;
}

int main(int argc, char **argv) {
    int threads = argc > 1 ? atoi(argv[1]) : (int)std::thread::hardware_concurrency();
    std::vector<std::string> lines; std::string l;
    while (std::getline(std::cin, l)) lines.push_back(l);
    std::vector<std::string> res(lines.size());
    std::atomic<size_t> next{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; t++) ts.emplace_back([&] { for (size_t i; (i = next++) < lines.size();) res[i] = process(lines[i]); });
    for (auto &t : ts) t.join();
    for (auto &r : res) fputs(r.c_str(), stdout);
}
