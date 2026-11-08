// CPU port of COMMISSION's GPU filter pipeline (search-tuning branch, record preset, small biomes, bounded).
// Same stages, same float formulation and thresholds, written for clarity rather than speed. Used to measure how
// many candidates each stage passes per seed and which stage drops a known island.
#pragma once
#include "noise.hpp"
#include "../../src/kernel_0A.h"
#include "../../src/kernel_0B.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace cp {
using standalone::Random;

struct Oct { uint8_t p[256]; float xo, yo, zo; };

inline void init_oct(Oct &o, Random r) {
    o.xo = float(r.next() >> 40) * 5.9604645E-8f * 256.0f;
    o.yo = float(r.next() >> 40) * 5.9604645E-8f * 256.0f;
    o.zo = float(r.next() >> 40) * 5.9604645E-8f * 256.0f;
    for (int i = 0; i < 256; i++) o.p[i] = uint8_t(i);
    for (int i = 0; i < 256; i++) { int j = int(r.bounded(256 - i)); std::swap(o.p[i], o.p[i + j]); }
}

// octave k of the interleaved order 0A, 0B, 1A, 1B, ..., 8A, 8B
struct Cfg { float in, val; };
inline Cfg cfg(int k) {
    const double amp[9] = {1, 1, 2, 2, 2, 1, 1, 1, 1};
    int o = k >> 1; bool b = k & 1;
    double in = 1.0 / 512 * double(1 << o);
    double val = double(1 << 8) / ((1 << 9) - 1.0) * (0.16666666666666666 / (0.1 * (1.0 + 1.0 / 9))) / double(1 << o) * amp[o];
    if (b) in *= 1.0181268882175227;
    return {float(in), float(val)};
}

// the continentalness octaves of a seed, built on demand
struct Seed {
    uint64_t seed; uint64_t fa[2], fb[2];
    Oct oct[18]; bool built[18] = {};
    explicit Seed(uint64_t s) : seed(s) {
        Random world(s); uint64_t a = world.next(), b = world.next();
        const auto &cf = standalone::configs[standalone::Continentalness]; Random c(a ^ cf.a, b ^ cf.b);
        for (int side = 0; side < 2; side++) { fa[side] = c.next(); fb[side] = c.next(); }
    }
    const Oct &get(int k) {
        if (!built[k]) {
            int o = k >> 1, side = k & 1; const auto &salt = standalone::octave_salt[-9 + o + 10];
            init_oct(oct[k], Random(fa[side] ^ salt[0], fb[side] ^ salt[1])); built[k] = true;
        }
        return oct[k];
    }
};

// the record preset's phase score: 350 d0^2 + 55 d1^2 + 4 d2^2 summed over both sides (float, as on the GPU)
inline float phase_score(const Seed &s) {
    float sc = 0; const float w[3] = {350.f, 55.f, 4.f};
    for (int side = 0; side < 2; side++)
        for (int o = 0; o < 3; o++) {
            const auto &salt = standalone::octave_salt[-9 + o + 10];
            Random r(s.fa[side] ^ salt[0], s.fb[side] ^ salt[1]); r.next();
            float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f;
            float d = std::fabs(f - 0.5f); sc += w[o] * d * d;
        }
    return sc;
}

inline float smooth(float v) { return v * v * v * (v * (v * 6.0f - 15.0f) + 10.0f); }
inline float grad(uint8_t h, float x, float y, float z) {
    static const float gx[16] = {1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0};
    static const float gy[16] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1};
    static const float gz[16] = {0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1};
    h &= 15; return std::fma(x, gx[h], std::fma(y, gy[h], z * gz[h]));
}
inline float lerp(float t, float a, float b) { return std::fma(t, b - a, a); }
inline float noise(const Oct &n, float x, float y, float z) {
    x += n.xo; y += n.yo; z += n.zo;
    int ix = int(std::floor(x)), iy = int(std::floor(y)), iz = int(std::floor(z));
    float fx = x - float(ix), fy = y - float(iy), fz = z - float(iz);
    const uint8_t *p = n.p;
    int p0 = p[ix & 255], p1 = p[(ix + 1) & 255];
    int p00 = p[(p0 + iy) & 255], p01 = p[(p0 + iy + 1) & 255], p10 = p[(p1 + iy) & 255], p11 = p[(p1 + iy + 1) & 255];
    float n000 = grad(p[(p00 + iz) & 255], fx, fy, fz), n100 = grad(p[(p10 + iz) & 255], fx - 1, fy, fz);
    float n010 = grad(p[(p01 + iz) & 255], fx, fy - 1, fz), n110 = grad(p[(p11 + iz) & 255], fx - 1, fy - 1, fz);
    float n001 = grad(p[(p00 + iz + 1) & 255], fx, fy, fz - 1), n101 = grad(p[(p10 + iz + 1) & 255], fx - 1, fy, fz - 1);
    float n011 = grad(p[(p01 + iz + 1) & 255], fx, fy - 1, fz - 1), n111 = grad(p[(p11 + iz + 1) & 255], fx - 1, fy - 1, fz - 1);
    float sx = smooth(fx), sy = smooth(fy), sz = smooth(fz);
    float lo = lerp(sy, lerp(sx, n000, n100), lerp(sx, n010, n110));
    float hi = lerp(sy, lerp(sx, n001, n101), lerp(sx, n011, n111));
    return lerp(sz, lo, hi);
}
inline float octave(Seed &s, int k, int x, int z) {
    Cfg c = cfg(k); return noise(s.get(k), float(x) * c.in, 0.0f * c.in, float(z) * c.in) * c.val;
}

struct Pos { int x, z; int count; };

// ---- filter_gradvecs_1: lattice positions of octave 0A that score well under the 0A matched filter ----
struct GV1 { float z0_gate = -4.0f, final_ = -16.0047f; };
inline void gradvecs_1(Seed &s, std::vector<Pos> &out, const GV1 &g = {}) {
    const Oct &o = s.get(0);
    float conv[2][256][4];
    for (int t = 0; t < 256; t++)
        for (int dx = 1; dx < 5; dx++)
            for (int y = 0; y < 2; y++) {
                float c = 0;
                for (int dz = 0; dz < 6; dz++) c += host_kernel_0A[y][dx][dz][o.p[(t + dz) & 255] & 15];
                conv[y][t][dx - 1] = c;
            }
    const int cell = 512;
    int x_center = int((2.5f - o.xo) * cell), z_center = int((2.5f - o.zo) * cell), ny = int(o.yo);
    int idx[2][262];
    for (int n = 0; n < 262; n++) for (int y = 0; y < 2; y++) idx[y][n] = o.p[(o.p[n & 255] + ny + y) & 255];
    for (int nx = 0; nx < 256; nx++)
        for (int nz = 0; nz < 256; nz++) {
            float gate = conv[0][(idx[0][nx + 2] + nz) & 255][1] + conv[0][(idx[0][nx + 3] + nz) & 255][2];
            if (!(gate >= g.z0_gate)) continue;
            float sc = (conv[0][(idx[0][nx + 2] + nz) & 255][1] + conv[0][(idx[0][nx + 3] + nz) & 255][2])
                     + conv[0][(idx[0][nx + 1] + nz) & 255][0] + conv[0][(idx[0][nx + 4] + nz) & 255][3]
                     + conv[1][(idx[1][nx + 1] + nz) & 255][0] + conv[1][(idx[1][nx + 2] + nz) & 255][1]
                     + conv[1][(idx[1][nx + 3] + nz) & 255][2] + conv[1][(idx[1][nx + 4] + nz) & 255][3];
            if (sc > g.final_) out.push_back({x_center + nx * cell, z_center + nz * cell, 0});
        }
}

// ---- side-only two-octave area tests (filter_2_01a: 0A+1A, filter_2_01b: 0B+1B), 256 samples below -0.55 ----
inline int two_octave_count(Seed &s, int side, int x, int z) {
    int n = 0;
    for (int zi = 0; zi < 16; zi++)
        for (int xi = 0; xi < 16; xi++) {
            int px = x + xi * 128 - 960, pz = z + zi * 128 - 960;
            float v = octave(s, side, px, pz) + octave(s, 2 + side, px, pz);
            n += v < -0.55f;
        }
    return n;
}

// ---- filter_gradvecs_2: every image of an A position in the bounded world, scored under the 0B filter ----
struct GV2 { float gate = -13.5f, final_ = -20.0f; int width = 115; };
inline void gradvecs_2(Seed &s, const Pos &in, std::vector<Pos> &out, const GV2 &g = {}) {
    const Oct &o = s.get(1);
    static thread_local float conv[2][6][256];
    for (int V = 0; V < 256; V++)
        for (int dx = 0; dx < 6; dx++)
            for (int y = 0; y < 2; y++) {
                float c = 0;
                for (int dz = 0; dz < 6; dz++) c += host_kernel_0B[y][dx][dz][o.p[(V + dz) & 255] & 15];
                conv[y][dx][V] = c;
            }
    const int half = g.width / 2, period = 512 * 256;
    const float fb = cfg(1).in; const int ny = int(o.yo);
    std::vector<int> cxs(g.width), czs(g.width);
    for (int t = 0; t < g.width; t++) {
        int d = (t - half) * period;
        cxs[t] = int(std::floor(float(in.x + d) * fb + o.xo - 2.0f)) & 255;
        czs[t] = int(std::floor(float(in.z + d) * fb + o.zo - 2.0f)) & 255;
    }
    for (int tx = 0; tx < g.width; tx++) {
        int h[2][6];
        for (int dx = 0; dx < 6; dx++) { int ix = o.p[(cxs[tx] + dx) & 255]; for (int y = 0; y < 2; y++) h[y][dx] = o.p[(ix + ny + y) & 255]; }
        for (int tz = 0; tz < g.width; tz++) {
            int row = czs[tz];
            float gate = conv[0][2][(h[0][2] + row) & 255] + conv[0][3][(h[0][3] + row) & 255]
                       + conv[1][2][(h[1][2] + row) & 255] + conv[1][3][(h[1][3] + row) & 255];
            if (!(gate >= g.gate)) continue;
            float sc = 0;
            for (int i = 0; i < 6; i++) { sc += conv[0][i][(h[0][i] + row) & 255]; sc += conv[1][i][(h[1][i] + row) & 255]; }
            if (sc > g.final_) out.push_back({in.x + (tx - half) * period, in.z + (tz - half) * period, 0});
        }
    }
}

// ---- cascade: N octaves, S samples on a square grid over RANGE blocks, count below -1.05, optional recentre ----
inline bool cascade(Seed &s, int noct, int range, int samples, int min_count, bool move, Pos &p) {
    int sq = 1; while (sq * sq < samples) sq <<= 1;
    int step = range / 4 / sq, off = -(step * (sq - 1) / 2);
    int n = 0; long sx = 0, sz = 0;
    for (int zi = 0; zi < sq; zi++)
        for (int xi = 0; xi < sq; xi++) {
            int x = p.x + xi * step + off, z = p.z + zi * step + off;
            float v = 0;
            for (int k = 0; k < noct; k++) v += octave(s, k, x, z);
            if (v < -1.05f) { n++; sx += x - p.x; sz += z - p.z; }
        }
    if (n < min_count) return false;
    if (move) { p.x += int(sx / n); p.z += int(sz / n); }
    p.count = n;
    return true;
}

struct Thresholds { float phase = 1.5f; GV1 gv1; GV2 gv2; int a_min = 27, b_min = 20, m2a = 24, m2b = 140, m2c = 420, m2d = 1700; };
struct Counts { long seeds = 0, gv1 = 0, a = 0, gv2 = 0, b = 0, c2a = 0, c2b = 0, c2c = 0, c2d = 0; };

// the whole GPU pipeline for one seed that passed the phase gate; final candidates in block coordinates
inline void run(Seed &s, const Thresholds &T, Counts &c, std::vector<Pos> &final_out) {
    c.seeds++;
    std::vector<Pos> g1; gradvecs_1(s, g1, T.gv1); c.gv1 += g1.size();
    for (auto &p : g1) {
        if (two_octave_count(s, 0, p.x, p.z) < T.a_min) continue;
        c.a++;
        std::vector<Pos> g2; gradvecs_2(s, p, g2, T.gv2); c.gv2 += g2.size();
        for (auto &q : g2) {
            if (two_octave_count(s, 1, q.x, q.z) < T.b_min) continue;
            c.b++;
            Pos r = q;
            if (!cascade(s, 12, 8192, 256, T.m2a, true, r)) continue; c.c2a++;
            if (!cascade(s, 14, 8192, 1024, T.m2b, true, r)) continue; c.c2b++;
            if (!cascade(s, 16, 10240, 4096, T.m2c, false, r)) continue; c.c2c++;
            if (!cascade(s, 18, 10240, 16384, T.m2d, false, r)) continue; c.c2d++;
            final_out.push_back({r.x * 4, r.z * 4, r.count});
        }
    }
}
} // namespace cp
