#!/usr/bin/env python3
"""Require exactly one producer/consumer instantiation per common queue case before scoring."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def audit(path, symbols=None, expected_total=280):
    text = path.read_text()
    names = re.findall(r'^(_Z[^\s:]*_M_runEv):[^\n]*$', text, re.M)
    demangled = subprocess.run(['c++filt'], input='\n'.join(names), text=True,
                               capture_output=True, check=True).stdout.splitlines()
    rows = []
    seen = set()
    for name, dem in zip(names, demangled):
        case = re.search(r'run_implementation<.*?payload<(\d+)ul>, (\d+)ul>', dem)
        owner = re.search(r'::\{lambda\(\)#([12])\}', dem)
        impl = re.search(r'payload<\d+ul> const&\)#(\d+)', dem)
        if not impl or int(impl[1]) != 3:
            continue
        if not case or not owner:
            raise ValueError('Unresolved shared-code function')
        payload, capacity = map(int, case.groups())
        key = capacity, payload, int(owner[1])
        if key in seen or int(impl[1]) != 3:
            raise ValueError('Duplicate or wrong shared-code callsite')
        seen.add(key)
        address = None
        if symbols is not None:
            matches = [m[1] for line in symbols.read_text().splitlines()
                       if (m := re.match(r"^\s*([0-9a-fA-F]+)\s+[A-Za-z]\s+(.+)$", line)) and m[2] == dem]
            if len(matches) != 1:
                raise ValueError("Missing/duplicate linked shared thread function")
            address = int(matches[0], 16)
        rows.append(dict(elf_address=address,capacity=capacity, payload_bytes=payload, owner=int(owner[1]), symbol=name))
    expected = {(c, p, o) for c in (2, 64, 256, 1024, 65536) for p in (8, 16, 64, 256) for o in (1, 2)}
    if seen != expected or len(names) != expected_total:
        raise ValueError(f'Common-code geometry incomplete: {len(seen)} shared / {len(names)} total functions')
    return dict(shared_queue_cases=20, shared_thread_functions=40, total_thread_functions=len(names),
                queue_allocator='runtime_buffer_allocator', callsite=3, functions=rows,
                linked_symbols_verified=symbols is not None,
                boundary='Both runtime labels enter this single source callsite; audit ELF symbols separately.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assembly', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--symbols', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.assembly, args.symbols)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('Shared code verified:', result['shared_thread_functions'], 'functions for both runtime labels')


if __name__ == '__main__':
    main()
