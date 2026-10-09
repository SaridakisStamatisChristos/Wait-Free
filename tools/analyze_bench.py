#!/usr/bin/env python3
import argparse, json, statistics
from collections import defaultdict

p=argparse.ArgumentParser(); p.add_argument('input'); a=p.parse_args()
groups=defaultdict(list)
for line in open(a.input):
    obj=json.loads(line)
    if 'benchmark' in obj and 'transfers_per_second' in obj:
        groups[obj['benchmark']].append(obj['transfers_per_second'])
for name, values in groups.items():
    values=sorted(values)
    q1=values[len(values)//4]; q3=values[(3*len(values))//4]
    med=statistics.median(values); mean=statistics.mean(values); sd=statistics.pstdev(values)
    print(f'{name}: n={len(values)} median={med:.3f} IQR=[{q1:.3f},{q3:.3f}] CV={(sd/mean if mean else 0):.4f}')
