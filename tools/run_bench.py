#!/usr/bin/env python3
import argparse, json, os, pathlib, platform, subprocess, time

p=argparse.ArgumentParser()
p.add_argument('--build', default='build/release')
p.add_argument('--repetitions', type=int, default=20)
p.add_argument('--output', default='evidence/benchmarks/raw.jsonl')
a=p.parse_args()
pathlib.Path(a.output).parent.mkdir(parents=True, exist_ok=True)
programs=['bench_throughput','bench_latency','bench_bursts','bench_payload']
meta={'timestamp_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'platform':platform.platform(), 'python':platform.python_version()}
with open(a.output,'w') as f:
    f.write(json.dumps({'meta':meta})+'\n')
    for rep in range(a.repetitions):
        for prog in programs:
            exe=pathlib.Path(a.build)/prog
            cp=subprocess.run([str(exe)], check=True, text=True, capture_output=True)
            for line in cp.stdout.splitlines():
                if line.strip():
                    obj=json.loads(line); obj['repetition']=rep; f.write(json.dumps(obj, sort_keys=True)+'\n')
print(a.output)
