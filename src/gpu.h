#pragma once

#include "common.h"

struct SeedIterator {
    std::atomic_uint64_t pos;
    const uint64_t origin;   // position 0 of the scan: the --origin seed, or the --start seed

    SeedIterator(uint64_t start, uint64_t origin) : pos(start), origin(origin) {

    }

    uint64_t next(uint64_t count) {
        return pos.fetch_add(count);
    }
};

struct GpuThread: Thread<GpuThread> {
    int device;
    SeedIterator &input;
    GpuOutputs &outputs;
    uint64_t max_iters;   // stop after this many batches (0: run until stopped)
    std::atomic_bool &running;

    GpuThread(int device, SeedIterator &input, GpuOutputs &outputs, uint64_t max_iters, std::atomic_bool &running);

    void run();
};