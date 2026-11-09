// Stack-A field at an A candidate (biome units), on filter_2_01a's 16x16 grid, with octaves 0..K of stack A:
// counts of samples below -0.55, -0.70, -0.85 for K = 1, 2, 4, 8 (12 numbers) and the mean of the full field.
#pragma once
#include "cpipe.hpp"
inline void afull_stats(cp::Seed &s, int x, int z, int out[12], float &mean8) {
    for (int i = 0; i < 12; i++) out[i] = 0;
    double sum = 0;
    const float lv[3] = {-0.55f, -0.70f, -0.85f};
    for (int zi = 0; zi < 16; zi++)
        for (int xi = 0; xi < 16; xi++) {
            int px = x + xi * 128 - 960, pz = z + zi * 128 - 960;
            float v = 0; int stage = 0;
            for (int o = 0; o <= 8; o++) {
                v += cp::octave(s, 2 * o, px, pz);
                int K = (o == 1) ? 0 : (o == 2) ? 1 : (o == 4) ? 2 : (o == 8) ? 3 : -1;
                if (K >= 0) for (int l = 0; l < 3; l++) out[K * 3 + l] += v < lv[l];
            }
            sum += v;
        }
    mean8 = float(sum / 256);
}
