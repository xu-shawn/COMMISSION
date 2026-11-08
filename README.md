# COMMISSION

## Definition
CUDA Optimized Massive Mushroom Island Standalone Search Including Only Necessities

CUDA app that uses various clever tricks to produce mushroom islands of adequate size.

## System Requirements

- Nvidia GPU (or AMD if you're lucky and you can transpile to HIP (currently unsupported)).
  - Tested Maxwell (sm_52) through Blackwell (sm_120)
  - 3GB+ VRAM (2976 MiB)
  - CUDA 12+ (tested, could work for older perhaps but that's untested.)

## Record search build (`PRESET=record`)

This branch adds a tuned build for hunting islands of 11 million blocks and more. It finds about 1.5× the
record-size islands per GPU-hour of the first version of this preset, and about 3× those of the unmodified
program. Every threshold is a macro in `src/tune.h`; a plain `make` still searches exactly as before, except for
a buffer race fixed in all builds.

```sh
make PRESET=record                 # bounded small biomes   -> main-sb-record
make PRESET=record UNBOUND=1       # unbounded small biomes -> main-sbu-record
./main-sb-record --device 0 --size 11000000 --start <your own start seed>
```

- The preset only hands the CPU candidates estimated at 10.9M blocks and more, so use `--size 11000000`
  (or at least 10,700,000). For smaller islands use the plain build.
- Give every machine its own `--start`. Two runs with the same start scan the same seeds in the same order.
- Bounded, RTX 4080 Super: 64 G seeds/s. Seeds per second are no longer comparable with other builds, since
  the thresholds decide how many islands a seed yields.
- An island above the current bounded record (12,618,448) takes about 22 days of one RTX 4080 Super, or
  14 of an RTX 4090, give or take a factor of 2. That is about $120 on-demand.
- What makes an island large, the size-dependent thresholds and the measurements are in
  `docs/record-tuning.md`; the first round of tuning is in `docs/search-economics.md`. `tools/` has the
  verification program and `tools/analysis/` the analysis programs, including a CPU port of the pipeline.
- The preset was validated for small biomes only. The branch also fixes an overflow that made the
  large-biomes CPU pre-test accept every candidate.
- Prebuilt Linux binaries were built with `NVCC_FLAGS="--cudart static"` on CUDA 12.4 and need driver 550
  or newer: `sm_86` for the 30 series, `sm_89` for the 40 and 50 series, `sm_75` for the 20 series.

## Usage
- Clone the repo 
  `git clone https://github.com/MinecraftAtHome/COMMISSION.git`
- Enter the folder
  `cd COMMISSION`
- Make the program
  `make`
- Run the program
  `./main --device 0,1,... --size 11000000`
### Runtime arguments
```
./main [--device <device>,<device>,...] [--threads <threads>] [--client <server_address>] [--server <listen_address>] [--output <output_file>] [--start <start_seed>] [--size <min_size>]
```

- client and server options aren't often used, they were created to network together multiple computers running this app.

### Info about device numbers
- These are just numbers given by the driver to identify your GPUs. You'll probably need to do this via trial and error if you have multiple gpus and you only want some of them running this.
- You could also omit the `--devices` option altogether. Then it'll just use `device 0`.
### Makefile info
- As of writing, the Makefile uses nvidia-smi to determine which gpu you have.
 - This is only necessary because Blackwell (sm\_120, or 50 series) performs worse when you compile native for it. So we compile targeting sm\_89 if you're on Lovelace (40 series) or newer, rather than compile for sm_120 for Blackwell.
 - If your GPU isn't 40 series or newer, it compiles with `-arch=native` by default. You can override this with `ARCH=<insert architecture>` i.e. `ARCH=sm_80` for 30 series. There hasn't been a reason to override native aside from for 50 series, though. Or if you're running GPUs from multiple generations on one computer.
- You can (generally) run the makefile without supplying any options, as all of the options have defaults.
- By default, your settings will be:
  - Small (normal) Biomes
  - Bound to world border (aka bounded)
  - Print interval at 4096
#### Options
 - ARCH
   - Change GPU target for PTX compilation
     - `ARCH=native` is default for GPUs 30 series or older
     - `ARCH=sm_89` is default for GPUs 40 series or newer
 - PRINT_INTERVAL
   - Change how often the program prints benchmarking info
     - Default is 256, meaning every 256 iterations of the full GPU pipeline, it will print the table with stats.
   - Example usage
     `make -B PRINT_INTERVAL=256` (default)
     `make -B PRINT_INTERVAL=4096`
 - UNBOUND
   - Change whether the app will check for mushroom islands that fall outside of the world border (30'000'000 in each direction)
   - Example usage
     `make -B UNBOUND=1`
   - Default is 0, which keeps it within the world border.
 - LARGE_BIOMES
   - Changes whether you're using normal (small) biomes, or large biomes.
   - Example usage
     `make -B LARGE_BIOMES=1` for large biomes
     `make -B LARGE_BIOMES=0` for small biomes (default)
