// Host check of TUNE_HASH_LEAN (src/gpu.cu, KernelFilterSeeds): the integer windows that decide staging must keep
// every seed the float gate passes. Replays the kernel's arithmetic along the hashing chain.
//   hashlean_check THREADS SEEDS      (also checks all 2^24 fractions of each window against the float test)
// Prints the pass rates of the staging tests and of the gate, and the number of violations (must be 0).
#include "../../src/Random.h"
#include "noise.hpp"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static constexpr float W0 = 257.3f, W1 = 47.4f, W2 = 7.5f, MAXS = 1.6572f;
static constexpr XrsrForkHash H_CONT{0x83886c9d0ae3a662, 0xafa638a61b42e8ad};
static constexpr XrsrForkHash H_OCT[3] = {{0x082fe255f8be6631, 0x4e96119e22dedc81}, {0x0ef68ec68504005e, 0x48b6bf93a2789640}, {0xf11268128982754f, 0x257a1d670430b0aa}};

// ---- copied from src/gpu.cu
static constexpr XrsrForkHash octave_yo_fork_hash(XrsrForkHash hash) { XrsrRandom rng{hash.lo, hash.hi}; rng.nextInternal(); return {rng.lo, rng.hi}; }
static XrsrRandomFork noise_yo_fork(XrsrRandomFork f) { uint64_t l = f.lo, h = f.hi; h ^= l; return {XrsrRandom::rol64(l, 49) ^ h ^ (h << 21), XrsrRandom::rol64(h, 28)}; }
static XrsrRandomFork fork_short(XrsrRandom &rng) {
  const uint64_t l = rng.lo, h = rng.hi; const uint64_t r1 = XrsrRandom::rol64(l + h, 17) + l; const uint64_t hx = h ^ l;
  const uint64_t l2 = XrsrRandom::rol64(l, 49) ^ hx ^ (hx << 21); const uint64_t h2 = XrsrRandom::rol64(hx, 28);
  const uint64_t r2 = XrsrRandom::rol64(l2 + h2, 17) + l2; rng.lo = l2; rng.hi = h2; return {r1, r2};
}
static void advance_one(XrsrRandom &rng) { const uint64_t l = rng.lo; const uint64_t hx = rng.hi ^ l; rng.lo = XrsrRandom::rol64(l, 49) ^ hx ^ (hx << 21); rng.hi = XrsrRandom::rol64(hx, 28); }
static XrsrRandomFork seed_fork_from_lh(uint64_t l, uint64_t h) {
  uint64_t r1 = XrsrRandom::rol64(l + h, 17) + l; h ^= l; uint64_t l2 = XrsrRandom::rol64(l, 49) ^ h ^ (h << 21); uint64_t h2 = XrsrRandom::rol64(h, 28);
  uint64_t r2 = XrsrRandom::rol64(l2 + h2, 17) + l2; return {r1, r2};
}
static float yo_mod1(int oct, const XrsrRandomFork &f) {
  const XrsrForkHash fh = octave_yo_fork_hash(H_OCT[oct]);
  uint64_t l = f.lo ^ fh.lo, h = f.hi ^ fh.hi; uint64_t r = XrsrRandom::rol64(l + h, 17) + l;
  return (uint32_t)((r >> 32) & 0xFFFFFF) * 5.9604645E-8f;
}
static float pen(float frac) { const float d = std::fabs(frac - 0.5f); return d * d; }
// ---- TUNE_HASH_LEAN
static constexpr uint32_t yo_window(double w, double max_score) {
  uint32_t lo = 0, hi = (UINT32_C(1) << 23) - 64;
  while (lo < hi) { const uint32_t mid = lo + (hi - lo + 1) / 2; const double d = mid * (1.0 / 16777216.0); if (w * d * d < max_score * 1.0001) { lo = mid; } else { hi = mid - 1; } }
  return (lo + 16 < (UINT32_C(1) << 23) - 64) ? lo + 16 : (UINT32_C(1) << 23) - 64;
}
static uint32_t yo_hi_approx(int oct, const XrsrRandomFork &f) {
  const XrsrForkHash fh = octave_yo_fork_hash(H_OCT[oct]);
  const uint64_t l = f.lo ^ fh.lo, h = f.hi ^ fh.hi; const uint64_t s = l + h;
  return (uint32_t)(s >> 15) + (uint32_t)(l >> 32);
}
static bool yo_in_window(uint32_t x, uint32_t k) { return ((x - ((UINT32_C(1) << 23) - k - 1u)) & 0xFFFFFFu) <= 2u * k + 1u; }

int main(int argc, char **argv) {
  const int threads = argc > 1 ? atoi(argv[1]) : 8; const uint64_t N = argc > 2 ? strtoull(argv[2], 0, 10) : 100000000ull;
  constexpr uint32_t win0 = yo_window(W0, MAXS), win1 = yo_window(W1, MAXS);
  printf("windows: octave 0  +-%u (%.5f of a layer), octave 1  +-%u (%.5f)\n", win0, win0 / 16777216.0, win1, win1 / 16777216.0);
  // every fraction: float test (plain and fused forms) passes  =>  window accepts it at x = f and x = f - 1
  long bad = 0, in0 = 0, fl0 = 0;
  for (int oct = 0; oct < 2; oct++) {
    const float w = oct ? W1 : W0; const uint32_t win = oct ? win1 : win0;
    for (uint32_t f = 0; f < (1u << 24); f++) {
      const float frac = (float)f * 5.9604645E-8f;
      const float d = std::fabs(frac - 0.5f); const float d2 = std::fabs(std::fma((float)f, 5.9604645E-8f, -0.5f));
      const bool pass = (w * (d * d) < MAXS) || (w * (d2 * d2) < MAXS) || ((double)w * d * d < (double)MAXS);
      const bool acc = yo_in_window(f, win) && yo_in_window((f - 1) & 0xFFFFFF, win);
      if (pass && !acc) bad++;
      if (oct == 0) { in0 += yo_in_window(f, win); fl0 += pass; }
    }
  }
  printf("exhaustive: %ld fractions pass the float test but not the window (must be 0); octave 0 window %ld, float %ld\n", bad, in0, fl0);
  std::atomic<uint64_t> a_live{0}, a_lean1{0}, a_lean2{0}, a_gate{0}, a_viol{0}, a_ref{0}, a_refmis{0}, a_carry{0};
  std::vector<std::thread> ts;
  const uint64_t t0 = 0x1234567890abcdefull;
  for (int th = 0; th < threads; th++) ts.emplace_back([&, th] {
    uint64_t live = 0, lean1 = 0, lean2 = 0, gate = 0, viol = 0, ref = 0, refmis = 0, carry = 0;
    for (uint64_t k = th; k < N; k += threads) {
      const uint64_t t = t0 + k * XrsrRandom::XRSR_GOLDEN_RATIO;          // chain value; seed = t ^ SILVER
      const uint64_t l = XrsrRandom::mix64(t), h = XrsrRandom::mix64(t + XrsrRandom::XRSR_GOLDEN_RATIO);
      const XrsrRandomFork sf = seed_fork_from_lh(l, h);
      XrsrRandom nr; nr.lo = sf.lo ^ H_CONT.lo; nr.hi = sf.hi ^ H_CONT.hi;
      const XrsrRandomFork a_yo = noise_yo_fork(fork_short(nr));
      const bool old_live = W0 * pen(yo_mod1(0, a_yo)) < MAXS;
      const uint32_t x0 = yo_hi_approx(0, a_yo), x1 = yo_hi_approx(1, a_yo);
      const bool l1 = yo_in_window(x0, win0), l2 = l1 && yo_in_window(x1, win1);
      { const XrsrForkHash fh = octave_yo_fork_hash(H_OCT[0]); uint64_t ll = a_yo.lo ^ fh.lo, hh = a_yo.hi ^ fh.hi; uint64_t r = XrsrRandom::rol64(ll + hh, 17) + ll; carry += ((uint32_t)(r >> 32) != x0); if ((uint32_t)(r >> 32) - x0 > 1u) viol++; }
      live += old_live; lean1 += l1; lean2 += l2;
      if (!old_live && !l1) continue;
      // the staged tail, as OMISSION_DRAIN
      XrsrRandom n2 = nr; advance_one(n2); const XrsrRandomFork b_yo = noise_yo_fork(n2.fork());
      float sc = W0 * pen(yo_mod1(0, a_yo)); bool pass = false;
      sc = sc + W0 * pen(yo_mod1(0, b_yo));
      if (sc < MAXS) { sc = sc + W1 * pen(yo_mod1(1, a_yo)); if (sc < MAXS) { sc = sc + W1 * pen(yo_mod1(1, b_yo)); if (sc < MAXS) {
        sc = sc + W2 * pen(yo_mod1(2, a_yo)); if (sc < MAXS) { sc = sc + W2 * pen(yo_mod1(2, b_yo)); if (sc < MAXS) pass = true; } } } }
      if (pass) {
        gate++;
        if (!old_live || !l1 || !l2) viol++;
        // independent path (noise.hpp): the same six fractions from the game's seeding
        const uint64_t seed = t ^ XrsrRandom::XRSR_SILVER_RATIO;
        standalone::Random world(seed); uint64_t a = world.next(), b = world.next();
        const auto &cf = standalone::configs[standalone::Continentalness]; standalone::Random c(a ^ cf.a, b ^ cf.b);
        float s2 = 0; uint64_t fa[2], fb[2]; for (int side = 0; side < 2; side++) { fa[side] = c.next(); fb[side] = c.next(); }
        float tm[2][3];
        for (int side = 0; side < 2; side++) for (int o = 0; o < 3; o++) {
          const auto &salt = standalone::octave_salt[-9 + o + 10]; standalone::Random r(fa[side] ^ salt[0], fb[side] ^ salt[1]); r.next();
          float f = float(uint32_t((r.next() >> 32) & 0xFFFFFF)) * 5.9604645E-8f; tm[side][o] = pen(f); }
        s2 = W0 * tm[0][0]; s2 = s2 + W0 * tm[1][0]; s2 = s2 + W1 * tm[0][1]; s2 = s2 + W1 * tm[1][1]; s2 = s2 + W2 * tm[0][2]; s2 = s2 + W2 * tm[1][2];
        ref++; if (!(s2 < MAXS) || s2 != sc) refmis++;
      }
    }
    a_live += live; a_lean1 += lean1; a_lean2 += lean2; a_gate += gate; a_viol += viol; a_ref += ref; a_refmis += refmis; a_carry += carry;
  });
  for (auto &t : ts) t.join();
  printf("%llu chain seeds: float 0A test %.5f, window 0A %.5f, windows 0A+1A %.5f, gate 1 in %.0f (%llu)\n", (unsigned long long)N,
         double(a_live) / N, double(a_lean1) / N, double(a_lean2) / N, double(N) / a_gate, (unsigned long long)a_gate.load());
  printf("carry left out changes the high word for %.4f of seeds\n", double(a_carry) / N);
  printf("violations (gate passes, staging test does not): %llu;  mismatches against noise.hpp seeding: %llu of %llu\n",
         (unsigned long long)a_viol.load(), (unsigned long long)a_refmis.load(), (unsigned long long)a_ref.load());
  return a_viol != 0 || bad != 0;
}
