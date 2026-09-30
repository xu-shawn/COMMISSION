#!/usr/bin/env python3
"""Fit the tail of the verified island sizes and extrapolate the effort for a target size.

usage: size_fit.py verified.txt <seeds scanned> <complete above, M blocks> [G seeds/s] [$ per hour]

verified.txt is the output of verify_largest; an optional ninth column gives the fraction of islands of
that kind that were verified (stratified sampling), default 1.  "Complete above" is the size above which
no island can have been removed by the run's cascade cut: the cut's estimated area is a safe choice,
because the verified area never exceeds the estimate by more than a few percent.
"""
import math
import sys

rows = [l.split() for l in open(sys.argv[1]) if len(l.split()) >= 8]
seeds = float(sys.argv[2]); u = float(sys.argv[3])
rate = float(sys.argv[4]) * 1e9 if len(sys.argv) > 4 else None
price = float(sys.argv[5]) if len(sys.argv) > 5 else None

area = [int(r[6]) / 1e6 for r in rows]
weight = [1 / float(r[8]) if len(r) > 8 else 1.0 for r in rows]
tail = [(a, w) for a, w in zip(area, weight) if a >= u]
n = sum(w for _, w in tail)
beta = sum(w * (a - u) for a, w in tail) / n                     # exponential: N(>=A) = N_u exp(-(A-u)/beta)
n_eff = n * n / sum(w * w for _, w in tail)
n_u = n / seeds * 1e12
print(f"{len(tail)} verified islands >= {u}M standing for {n:.0f}; {n_u:.1f} per 1e12 seeds")
print(f"e-fold {beta:.3f} +- {beta / math.sqrt(n_eff):.3f} M  (frequency halves every {beta * math.log(2):.3f} M)")
for target in (9, 10, 11, 11.147, 12, 12.618, 13):
    f = n_u * math.exp(-(target - u) / beta)
    line = f"  >= {target:6.3f}M: {f:10.3e} per 1e12 seeds"
    if rate:
        hours = 1e12 / f / rate / 3600
        line += f"   one per {hours:9.2f} GPU-hours"
        if price:
            line += f"   ${hours * price:9.2f}"
    print(line)
