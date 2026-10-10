#!/usr/bin/env python3
"""Require shared packed/split worker functions and preserve their linked identities."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def audit(assembly, symbols, implementations=5):
    names = re.findall(r'^(_Z[^\s:]*_M_runEv):[^\n]*$', assembly.read_text(), re.M)
    decoded = subprocess.run(['c++filt'], input='\n'.join(names), text=True,
                             capture_output=True, check=True).stdout.splitlines()
    linked = {}
    for line in symbols.read_text().splitlines():
        if m := re.match(r'^\s*([0-9a-fA-F]+)\s+[A-Za-z]\s+(.+)$', line):
            linked.setdefault(m[2], []).append(int(m[1], 16))
    rows = []
    seen = set()
    for name, dem in zip(names, decoded):
        mode = 'shared' if 'run_pair_shared<' in dem else 'original' if 'run_pair_original<' in dem else None
        case = re.search(r'run_implementation<.*?payload<(\d+)ul>, (\d+)ul>', dem)
        owner = re.search(r'::\{lambda\(\)#([12])\}', dem)
        impl = re.search(r'payload<\d+ul> const&\)#(\d+)', dem)
        if mode is None or not case or not owner or not impl:
            raise ValueError('Unresolved worker function')
        payload, capacity = map(int, case.groups())
        key = mode, capacity, payload, int(impl[1]), int(owner[1])
        if key in seen or len(linked.get(dem, [])) != 1:
            raise ValueError('Duplicate worker or missing/duplicate linked symbol')
        seen.add(key)
        rows.append(dict(mode=mode,capacity=capacity,payload=payload,implementation=int(impl[1]),
                         owner=int(owner[1]),symbol=name,elf_address=linked[dem][0]))
    expected = {(m,c,p,i,o) for m in ('original','shared') for c in (2,64,256,1024,65536)
                for p in (8,16,64,256) for i in range(1,implementations+1) for o in (1,2)}
    if seen != expected or len(names) != len(expected):
        raise ValueError(f'Incomplete worker matrix {len(seen)}/{len(expected)}')
    return dict(shared_worker_functions=len(expected)//2, total_worker_functions=len(expected),
                linked_symbols_verified=True,functions=rows,
                interpretation='Packed and split use one run_pair_shared callsite; only consumed counter address differs.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assembly', type=Path)
    parser.add_argument('symbols', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--implementations', type=int, default=5)
    args = parser.parse_args()
    result = audit(args.assembly,args.symbols,args.implementations)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('Shared worker code verified:', result['shared_worker_functions'])
