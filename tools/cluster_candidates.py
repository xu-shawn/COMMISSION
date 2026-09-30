#!/usr/bin/env python3
"""Reduce a candidates file to one line per island.

The GPU reports an island once for every lineage that reaches it (on average about 1.1 to 2.5 times), so
candidates of one seed that lie within a cascade window of each other are merged, keeping the largest count.

usage: cluster_candidates.py candidates-dev0.txt [minimum count] > islands.txt
"""
import sys

best = {}
for line in open(sys.argv[1]):
    t = line.split()
    if len(t) != 4:
        continue
    seed, x, z, count = map(int, t)
    cell = (round(x / 8000), round(z / 8000))
    for dx in (-1, 0, 1):
        for dz in (-1, 0, 1):
            key = (seed, cell[0] + dx, cell[1] + dz)
            if key in best:
                if count > best[key][3]:
                    best[key] = (seed, x, z, count)
                break
        else:
            continue
        break
    else:
        best[(seed, *cell)] = (seed, x, z, count)

floor = int(sys.argv[2]) if len(sys.argv) > 2 else 0
for row in sorted((v for v in best.values() if v[3] >= floor), key=lambda v: -v[3]):
    print(*row)
