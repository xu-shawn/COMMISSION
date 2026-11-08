# Analysis tools

CPU programs used for `docs/record-tuning.md`. All read the continentalness noise through `noise.hpp`, an independent,
bit-exact sampler. Build any of them with

```sh
g++ -O3 -march=native -std=c++20 -pthread NAME.cpp -o NAME
```

| Program | What it does |
|---|---|
| `cpipe.hpp`, `cpipe.cpp` | CPU port of the GPU pipeline of the previous record preset (same stages, float formulation and thresholds; stage counts match the GPU). `cpipe probe < "seed x z"` shows which stage drops a known island; `cpipe rate THREADS N [key=value]` gives candidates per phase-gate survivor for each stage. |
| `anatomy.cpp` | Floods an island and reports, per octave and stack, the y-offset, the mean contribution over the island and the favourable lattice nodes inside it. |
| `levels.cpp` | Keeps an island's octaves 0..L-1 and re-draws the rest at random: how much of the island each octave level carries. |
| `coast.cpp` | Which octave group rises from an island's interior to its coast. |
| `basin.cpp` | Areas of the low-octave skeleton (octave 0, octaves 0-1, both stacks) below several levels around an island. |
| `gvfeat.cpp` | gradvecs_1-level features (score, core and window node counts) of the octave-0A and 0B windows at an island. |
| `joint.cpp` | The joint octave-0/1 statistics used by `TUNE_JOINT_B`, at the gradvecs_2 outputs of islands (`joint islands`) or of random seeds (`joint random N`). |
| `phasecorr.cpp`, `gaterecall.cpp` | Correlation of the octaves' y-offsets through the RNG; recall and pass rate of phase gates under the island density of `docs/search-economics.md`. |
| `nodes.cpp` | Vertical-gradient classes of the central octave-0A nodes at islands. |

Island lists are "seed x z [area]" lines in block coordinates (`verify_largest` output can be cut to that).
