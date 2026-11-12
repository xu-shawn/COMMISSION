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

A tuned build for hunting record-size islands (11 million blocks and more). Every setting is a macro in
`src/tune.h`; a plain `make` still searches exactly as before, except for a buffer race fixed in all builds.

```sh
make PRESET=record                    # bounded small biomes    -> main-sb-record
make PRESET=record UNBOUND=1          # unbounded small biomes  -> main-sbu-record
make PRESET=record LARGE_BIOMES=1     # bounded large biomes    -> main-lb-record
make PRESET=record ARCH=sm_61         # build for a GPU generation other than this machine's
./main-sb-record --device 0 --size 11000000 --start <your own start seed>
```

What the preset changes:

- **Filters fitted to where large islands come from.** The seed filter is a quadratic score whose weights were
  fitted to how island probability falls with each octave's vertical offset; the later thresholds were set on
  tens of thousands of verified islands. These filters lose islands of every size at the same rate, so they were
  tuned on ordinary large islands and then checked on the largest ones.
- **Faster kernels, chosen for the GPU generation it is built for** (the makefile reads `ARCH`, or the first GPU's
  compute capability from `nvidia-smi`): a leaner seed-hashing tail, a gradvecs_1 scan with fewer shared-memory
  bank conflicts (and on Pascal a join of the A and B row lists), per-warp lattice-node tables in filter_2_01a and
  the joint test, a row scan of the stack-B images, and grid caps that leave room for the overlapped hashing.
  All of them are exact: on identical seeds every stage passes the same seeds and positions as without them.
  Measured over the whole search: 1.21x on Pascal (GTX 10 series) and 1.47x on Turing for unbounded small biomes,
  1.18x and 1.49x for bounded large biomes. Ampere, Ada and Blackwell get the Turing settings, which are exact
  there too but have not been timed there.
- **Unbounded builds screen A candidates on all of stack A's large octaves** before pairing each with its 109,561
  stack-B images: 5.6% fewer islands per seed at every size, 10% faster, about 3.6% more islands per GPU-hour.
- Large biomes keep a lower cascade cut, because the small-biome cut drops large-biome islands near the bounded
  large-biome record.

The `TUNE_*` notes in `src/tune.h` record each measurement. `docs/search-economics.md` and `docs/record-tuning.md`
describe the earlier rounds of tuning; `tools/` has the verification programs and `tools/analysis/` the analysis
programs, including a CPU port of the pipeline and a host emulator of the kernels (`tools/simt/`).

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
