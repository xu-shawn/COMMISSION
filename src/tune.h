#pragma once
// Search tunables.
//
// Every default below reproduces the behaviour of the code this file was added to, so a plain `make`
// searches exactly as before except for the hashing order (TUNE_CHAIN_HASH, which changes the order in
// which seeds are visited but not which seeds pass).
//
//   make PRESET=record                      thresholds tuned for islands of 11 million blocks and more
//   make TUNE="-DTUNE_MAX_SCORE=0.046f"     override single values (wins over a preset)
//
// How these values were measured is written up in docs/search-economics.md.

#if defined(TUNE_PRESET_RECORD) && TUNE_PRESET_RECORD
// Island frequency falls off as exp(-350 d0^2 - 55 d1^2 - 4 d2^2) in the distance d of each octave's
// vertical offset from the middle of its lattice layer, for islands of record size.  A level set of that
// density keeps more islands per surviving seed than the weighted sum of |d| used before.
#  ifndef TUNE_SCORE_POW
#    define TUNE_SCORE_POW 2
#  endif
// Weights refitted on 7,457 islands of 9.5M and more (island probability exp(-257 d0^2 - 47 d1^2 - 7.5 d2^2) per
// stack, normalised by random seeds inside the gate); 1.6572 gives the pass rate of the previous 350/55/4 < 1.5.
// Measured: 6.7% +- 2.0% more islands per seed at the same speed. Large biomes fit 239/51/10, close enough.
#  ifndef TUNE_W0
#    define TUNE_W0 257.3f
#  endif
#  ifndef TUNE_W1
#    define TUNE_W1 47.4f
#  endif
#  ifndef TUNE_W2
#    define TUNE_W2 7.5f
#  endif
#  ifndef TUNE_MAX_SCORE
#    define TUNE_MAX_SCORE 1.6572f
#  endif
// fewer survivors per seed, so the fixed cost of the late kernels is spread over four times the seeds
#  ifndef TUNE_LOG2_BATCH
#    define TUNE_LOG2_BATCH 30
#  endif
// Size-aware thresholds (docs/record-tuning.md). gradvecs_1's z0 gate looks at the bottom layer of the
// central lattice nodes only and drops ~40% of large islands; -5 keeps most of them. Its final score, raised
// from -16 to -15, drops a tenth of 9-10M islands but only ~7% of those above 11M (their octave-0 cores are
// stronger), and cuts the positions handed to filter_2_01a by 2.5x.
#  ifndef TUNE_GV1_Z0_GATE
#    define TUNE_GV1_Z0_GATE -5.0f
#  endif
#  ifndef TUNE_GV1_FINAL
#    define TUNE_GV1_FINAL -15.0f
#  endif
// filter_2_01b replaced by the joint octave 0-1 test (both stacks), which keeps every known island above
// 10.5M and passes about a fortieth of what the B-only test did
#  ifndef TUNE_JOINT_B
#    define TUNE_JOINT_B 24
#  endif
// seed hashing overlapped with the later kernels, and the hashing's zero-state guard skipped (same results)
#  ifndef TUNE_RAW_FORK
#    define TUNE_RAW_FORK 1
#  endif
#  ifndef TUNE_OVERLAP
#    define TUNE_OVERLAP 1
#  endif
// the joint test reuses filter_2_01a's stack-A sums (same results up to float rounding; -40% on that stage)
#  ifndef TUNE_JOINT_REUSE_A
#    define TUNE_JOINT_REUSE_A 1
#  endif
#  ifndef TUNE_GV1_BPS
#    define TUNE_GV1_BPS 2
#  endif
#  ifndef TUNE_A_BPS
#    define TUNE_A_BPS 3
#  endif
// hand the CPU only candidates whose estimated area is at least 10.9 million blocks
#  ifndef TUNE_MIN_2B
#    define TUNE_MIN_2B 140
#  endif
#  ifndef TUNE_MIN_2C
#    define TUNE_MIN_2C 420
#  endif
#  ifndef TUNE_MIN_2D
#    define TUNE_MIN_2D 1700
#  endif
#endif

// ---- filter_seeds ---------------------------------------------------------------------------------
// score = combine over octaves 0..2 of both stacks of  W_i * pen(|frac(yo) - 0.5|)  <  TUNE_MAX_SCORE
//   pen(d) = d (TUNE_SCORE_POW 1) or d*d (TUNE_SCORE_POW 2)
//   combine = sum (TUNE_SCORE_MAX 0) or maximum (TUNE_SCORE_MAX 1, a box)
#ifndef TUNE_MAX_SCORE
#define TUNE_MAX_SCORE 0.054f
#endif
#ifndef TUNE_W0
#define TUNE_W0 0.35f
#endif
#ifndef TUNE_W1
#define TUNE_W1 0.11f
#endif
#ifndef TUNE_W2
#define TUNE_W2 0.04f
#endif
#ifndef TUNE_SCORE_POW
#define TUNE_SCORE_POW 1
#endif
#ifndef TUNE_SCORE_MAX
#define TUNE_SCORE_MAX 0
#endif

// Hashing order.
//   0: consecutive seeds.
//   1: walk the chain t_k = t_0 + k*GOLDEN, seed_k = t_k ^ SILVER.  The generator state of a seed is
//      (mix64(t), mix64(t + GOLDEN)), so along the chain the second word of one seed is the first word of
//      the next, and each seed costs one mix64 instead of two.  GOLDEN is odd, so the chain visits every
//      64-bit seed exactly once; position 0 is the --start seed itself.
#ifndef TUNE_CHAIN_HASH
#define TUNE_CHAIN_HASH 1
#endif

// log2 of the seeds per GPU iteration, and of the survivor capacity per iteration.
// Survivors per iteration = pass rate * 2^TUNE_LOG2_BATCH must stay below the capacity.
#ifndef TUNE_LOG2_BATCH
#define TUNE_LOG2_BATCH 28
#endif
#ifndef TUNE_LOG2_SURVIVORS
#define TUNE_LOG2_SURVIVORS 19
#endif

// ---- filter_gradvecs_1: gate on the lower-layer centre columns, bound on the 8-term score ------------
#ifndef TUNE_GV1_Z0_GATE
#define TUNE_GV1_Z0_GATE -4.0f
#endif
#ifndef TUNE_GV1_FINAL
#define TUNE_GV1_FINAL -16.0047f
#endif

// ---- filter_gradvecs_2 -------------------------------------------------------------------------------
#ifndef TUNE_GV2_GATE
#define TUNE_GV2_GATE -13.5f
#endif
#ifndef TUNE_GV2_FINAL
#define TUNE_GV2_FINAL -20.0f
#endif

// Scan the images of stack B column by column (1) or image by image as before (0).  Same candidates.
#ifndef TUNE_GV2_BY_COLUMN
#define TUNE_GV2_BY_COLUMN 1
#endif

// ---- two-octave area tests: minimum samples (of 256) below -0.55 -------------------------------------
#ifndef TUNE_A_MIN
#define TUNE_A_MIN 27
#endif
#ifndef TUNE_B_MIN
#define TUNE_B_MIN 20
#endif

// ---- cascade: minimum samples below -1.05 of 256 / 1024 / 4096 / 16384 -------------------------------
// The last one times 6400 (102400 for large biomes) is the smallest estimated area passed to the CPU.
#ifndef TUNE_MIN_2A
#define TUNE_MIN_2A 24
#endif
#ifndef TUNE_MIN_2B
#define TUNE_MIN_2B 110
#endif
#ifndef TUNE_MIN_2C
#define TUNE_MIN_2C 340
#endif
#ifndef TUNE_MIN_2D
#define TUNE_MIN_2D 1540
#endif

// Write every candidate the GPU passes to the CPU to candidates-dev<N>.txt as "seed x z count", where
// count is the final stage's sample count (estimated area = count * 6400).  Off by default.
#ifndef TUNE_LOG_CANDIDATES
#define TUNE_LOG_CANDIDATES 0
#endif

// ---- additions (see docs/record-tuning.md) ----------------------------------------------------------------

// Skip the all-zero-state guard of XrsrRandom(lo, hi) in the seed hashing (probability 2^-128). Same results.
#ifndef TUNE_RAW_FORK
#define TUNE_RAW_FORK 0
#endif

// Hash batch i+1 on a low-priority stream while batch i runs through the later kernels on a high-priority one
// (seed hashing is bound by the integer ALU pipe, gradvecs_1 by shared-memory loads). Same results.
#ifndef TUNE_OVERLAP
#define TUNE_OVERLAP 0
#endif

// Grid caps, in blocks per SM, for gradvecs_1 and filter_2_01a (0 = the fixed grids used before). With
// TUNE_OVERLAP they leave room on each SM for the hashing blocks.
#ifndef TUNE_GV1_BPS
#define TUNE_GV1_BPS 0
#endif
#ifndef TUNE_A_BPS
#define TUNE_A_BPS 0
#endif

// filter_2_01b replaced by a joint test on octaves 0-1 of both stacks: the minimum number of the 256 samples
// where 0A + 0B + 1A + 1B < TUNE_JOINT_LEVEL (0 = the B-only test).
#ifndef TUNE_JOINT_B
#define TUNE_JOINT_B 0
#endif
#ifndef TUNE_JOINT_LEVEL
#define TUNE_JOINT_LEVEL -1.05
#endif

// gradvecs_1 scanned with candidates on the lanes and columns in the loop (kernel_v2): same candidates.
//   1: candidates in ascending row order (2.5 wavefronts per B load: 32 consecutive rows of the list are not
//      distinct mod 32)
//   2: B stored 8-fold and candidates placed by row % 4, so every B load is one wavefront; partly filled chunks
//      (2.36 per seed for 2.10) and a compare without the add; +6 KB of shared memory (24.4 KB a block).
//      Measured: 1% faster than 1 alone on Pascal and slower than `kernel` with TUNE_GV1_LEAN on both
//      cards (its hit loop runs 60% more passes, because hits per candidate are far more uneven than per column)
#ifndef TUNE_GV1_V2
#define TUNE_GV1_V2 0
#endif

// timing experiments (wrong results): kernel_v2: 1 = setup only, 2 = scan without queue/score; kernel: 11, 12 the same;
// kernel: 13 = as 12 with one lag for every lane, so the scan's B loads have no bank conflicts (12 against 13
// shows how much of the scan the conflicts cost)
#ifndef TUNE_GV1_EXP
#define TUNE_GV1_EXP 0
#endif

// The joint test reuses filter_2_01a's octave 0A + 1A sums instead of recomputing them at every image.
#ifndef TUNE_JOINT_REUSE_A
#define TUNE_JOINT_REUSE_A 0
#endif

// mix64 multipliers of the seed hashing passed as kernel arguments (Pascal: avoids MOV32I per seed). Same results.
#ifndef TUNE_HASH_KARG
#define TUNE_HASH_KARG 0
#endif

// gradvecs_1 (kernel): packed lattice indices and an early exit after the bottom-layer terms when scoring
// gate survivors. Same candidates.
#ifndef TUNE_GV1_DRAIN2
#define TUNE_GV1_DRAIN2 0
#endif

// Joint test pre-filter (needs TUNE_JOINT_REUSE_A): minimum count over 64 samples (4 rows) before the full test.
#ifndef TUNE_JOINT_PRE
#define TUNE_JOINT_PRE 0
#endif

// Full-A screen (needs TUNE_JOINT_REUSE_A): minimum count of the 256 samples where octaves 0A + 1A + 2A are below
// -0.55, applied to filter_2_01a's outputs before gradvecs_2 (0 = off). Pair with a looser TUNE_A_MIN (25).
#ifndef TUNE_A2_MIN
#define TUNE_A2_MIN 0
#endif

// gradvecs_1 with a block-wide list of gate survivors, scored after the scan by all threads (kernel_v3). Same
// candidates as `kernel`.
#ifndef TUNE_GV1_V3
#define TUNE_GV1_V3 0
#endif

// gradvecs_1 gate as a join of the A-row and B-row lists with per-lag column lists (kernel_v4). Same candidates.
#ifndef TUNE_GV1_V4
#define TUNE_GV1_V4 0
#endif

// seed hashing: seeds per thread and unroll of the per-seed loop
#ifndef TUNE_SEEDS_PER_THREAD
#define TUNE_SEEDS_PER_THREAD 32
#endif
#ifndef TUNE_HASH_UNROLL
#define TUNE_HASH_UNROLL 2
#endif

// gradvecs_1 gate on the bottom (0) or top (1) layer's central pair (kernel only)
#ifndef TUNE_GV1_GATE_LAYER
#define TUNE_GV1_GATE_LAYER 0
#endif

// Phase-adaptive gradvecs_1 final cut (kernel only): cut + slope * (phase score - centre). At equal output the
// fitted models give about +0.7% islands (slope 0.75, centre 1.137 for the refitted gate), but storing the phase
// scores behind the survivors made the run about 2.7% slower on Pascal. TUNE_GV1_PHASE_ADAPT 1 = on.
#ifndef TUNE_GV1_PHASE_ADAPT
#define TUNE_GV1_PHASE_ADAPT 0
#endif
#ifndef TUNE_GV1_PHASE_SLOPE
#define TUNE_GV1_PHASE_SLOPE 0.75f
#endif
#ifndef TUNE_GV1_PHASE_CENTRE
#define TUNE_GV1_PHASE_CENTRE 1.137f
#endif

// ---- kernel-level additions ---------------------------------------------------------------------------------
//
// Measured on identical seeds (2.75e11, small biomes, unbounded, overlap on), against this tree with the options off:
//
//   Pascal (sm_61): TUNE_HASH_LEAN=2 TUNE_GV1_LEAN=4 TUNE_JOINT_SKIP_A=1 TUNE_A_NODES=1 TUNE_JOINT_NODES=1
//                        with TUNE_GV1_BPS=3 TUNE_A_BPS=4                         1.155x  (1.163x with TUNE_A_EARLY=13)
//   Turing (sm_75): TUNE_HASH_LEAN=4 TUNE_GV1_LEAN=8 TUNE_JOINT_SKIP_A=1 TUNE_A_NODES=1 TUNE_JOINT_NODES=1
//                        (kernel, not kernel_v2; default grid caps)               1.42x   (1.44x with TUNE_A_EARLY=13)
//
// Stage counts are identical to the unmodified kernels except filter_2_01a's with TUNE_A_NODES (2 more of
// 2,314,525: sums differ in the last bits) and, with TUNE_A_EARLY=13, 1.5% fewer filter_2_01a outputs.
// What each option contributes depends on the other and on the card, because the seed hashing shares the SMs with
// the later kernels: see the notes at each option.

// Seed hashing, leaner tail. Same survivors (the staged tail still applies the whole gate in float).
//   1: the test that decides staging is an integer window on the high word of octave 0A's offset, computed
//      without the carry out of the low word (no float); seeds are staged as their index in the batch, 9 words
//      per slot instead of 10
//   2: also octave 1A's window before staging: 5.5% of the seeds are staged instead of 16.1%, so the staged tail
//      (about 200 instructions per 32 staged seeds) runs 2.9 times less often. Hashing alone 7.6% faster on a
//      Pascal; the best setting there
//   3: as 2, staging the generator before the A fork (5 words a slot; the staged tail redoes the A fork)
//   4: as 2, but a passing seed only sets a bit; every 32 seeds the bits are compacted into a queue of seed
//      indices and the staged tail rebuilds the seed from its index (1 word a slot, no per-seed ballot or
//      stores). Hashing alone 8.8% faster on Pascal, 7.5% on Turing. Its blocks need 2 KB of
//      shared memory, so many fit beside the later kernels: on Turing that hides nearly all of the hashing
//      behind gradvecs_1 (the best setting there); on Pascal it takes too many issue slots from gradvecs_1 and
//      2 is better
#ifndef TUNE_HASH_LEAN
#define TUNE_HASH_LEAN 0
#endif
// bytes of unused shared memory added to a hashing block, to set how many fit on an SM (see TUNE_HASH_TPB)
#ifndef TUNE_HASH_PAD
#define TUNE_HASH_PAD 0
#endif

// Seed hashing: threads per block (256: the staging area of a block is 20,480 B, 18,432 B with TUNE_HASH_LEAN 1-2,
// 10,240 B with 3, 2,048 B with 4). Together with TUNE_HASH_PAD and the grid caps of the later kernels this sets
// how many hashing warps run beside them. On Pascal eight hashing warps beside three gradvecs_1 blocks
// (the default with TUNE_HASH_LEAN 2) is the optimum: 4 and 12-40 were measured worse or equal.
#ifndef TUNE_HASH_TPB
#define TUNE_HASH_TPB 256
#endif

// gradvecs_1 (kernel): leaner gate scan. Same candidates.
//   K = 2, 4, 8 or 16: the gate's B column is stored K-fold so that the lanes of a warp read it from different
//   banks (2.11 wavefronts per load with K = 4, 2.00 with K = 8, against 3.15), each candidate row is one 8-byte
//   broadcast (byte offset of its row, lowest passing B), and the compare needs no add: 6.3 instructions a
//   check for 10.9. Also: the block maximum of B is reduced before the first barrier (one barrier a seed less), the
//   hit queue holds list positions in 16 bits, and the score tables are plain per-column arrays.
//   Shared memory per block: 19,848 B with K = 4, 23,944 B with K = 8 (19,304 B without).
//   gradvecs_1 alone on Pascal: 6.8 s -> 5.7 s per 2.75e11 seeds with K = 4. Use 4 on Pascal (8 is slower
//   there with three blocks per SM) and 8 on Turing.
#ifndef TUNE_GV1_LEAN
#define TUNE_GV1_LEAN 0
#endif

// filter_2_01a: lanes sample their half row from the middle of the window outwards, and a candidate with fewer
// than TUNE_A_EARLY of the 128 central samples below the level is dropped before the outer 128 (0 = off, the
// original order; negative = the new order without the exit, which reproduces the stage counts exactly).
// A filter change: 13 drops 63% of the inputs at half time and 1.5% of the passes; of 23,154 verified islands of
// 9M and more it keeps 99.84%, of 829 above 10.5M 99.6% (tools/analysis/afirst.cpp). Worth 0.5-1.5% in speed.
#ifndef TUNE_A_EARLY
#define TUNE_A_EARLY 0
#endif

// Gradient table layout, for every noise-sampling kernel. Same arithmetic, same results. Not a gain: 1 makes
// filter_2_01a 12% faster alone and the joint test 12% slower, and neither helps with the hashing beside them.
//   1: one 16-byte entry per index (x, y, z, pad): a corner's three components are one 128-bit shared load
//      instead of three loads (but entries h and h + 8 share banks)
//   2: (x, y) pairs and a z array: one 64-bit and one 32-bit load a corner, both free of bank conflicts
#ifndef TUNE_GRAD4
#define TUNE_GRAD4 0
#endif

// Joint test with TUNE_JOINT_REUSE_A: fetch only the two stack-B octaves it samples (it used to copy all four
// noise structs to shared memory for every input). Same results; the stage is 7% faster.
#ifndef TUNE_JOINT_SKIP_A
#define TUNE_JOINT_SKIP_A 0
#endif

// TUNE_OVERLAP: how many batches may be hashed ahead of the batch the later kernels are working on (1 = as before).
// No gain measured with 2: the hashing stream still gets one batch per iteration, in a burst at its start.
#ifndef TUNE_HASH_AHEAD
#define TUNE_HASH_AHEAD 1
#endif

// filter_2_01a: build the y-blended gradients of the window's lattice nodes once per candidate (36 nodes of
// octave 0A, 100 of 1A) and form each lane's cell faces from two nodes, instead of hashing and blending four
// corners per face in every lane. Same sums up to the last bits (the y blend is taken before the z term is
// added), so the pass counts can differ by a few in a million. 1,896 B of shared memory per warp: a block needs
// 19.7 KB, so cap the grid at four blocks per SM on Pascal (TUNE_A_BPS=4). The stage alone: 2.1 s -> 1.2 s per
// 2.75e11 seeds on Pascal.
#ifndef TUNE_A_NODES
#define TUNE_A_NODES 0
#endif

// The joint test (with TUNE_JOINT_REUSE_A) with its two stack-B octaves sampled from a node table, as
// TUNE_A_NODES does for filter_2_01a. Same sums up to the last bits. The stage alone: 0.70 s -> 0.42 s.
#ifndef TUNE_JOINT_NODES
#define TUNE_JOINT_NODES 0
#endif

// gradvecs_1 (kernel with TUNE_GV1_LEAN): read the candidate rows two per 16-byte broadcast load. Same candidates.
#ifndef TUNE_GV1_PAIR
#define TUNE_GV1_PAIR 0
#endif
// grid caps (blocks per SM, 0 = the old fixed grids) for gradvecs_2 and the joint test: both kernels grid-stride, so
// a smaller grid gives the same results and leaves room on every SM for the overlapped seed hashing
#ifndef TUNE_GV2_BPS
#define TUNE_GV2_BPS 0
#endif
#ifndef TUNE_JOINT_BPS
#define TUNE_JOINT_BPS 0
#endif
// gradvecs_1 (kernel with TUNE_GV1_LEAN = K): deal the columns to the warps by lag class (lag mod 32/K) so most
// warps hold K columns of each class and their B loads take one wavefront. Same candidates.
#ifndef TUNE_GV1_SORT
#define TUNE_GV1_SORT 0
#endif
