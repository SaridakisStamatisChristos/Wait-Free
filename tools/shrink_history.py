#!/usr/bin/env python3
"""Delta-debug a failing history while preserving JSON schema.

Usage: shrink_history.py failing.json checker-command [checker-args...]
The checker command must return non-zero while the failure is preserved.
"""
import copy, json, subprocess, sys, tempfile
from pathlib import Path

if len(sys.argv) < 3: raise SystemExit(__doc__)
source=Path(sys.argv[1]); checker=sys.argv[2:]
data=json.loads(source.read_text()); ops=data['operations']

def fails(candidate):
    payload=copy.deepcopy(data); payload['operations']=candidate
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
        json.dump(payload,f); name=f.name
    return subprocess.run(checker+[name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode != 0

n=2
while len(ops)>=2:
    size=max(1,len(ops)//n); reduced=False
    for start in range(0,len(ops),size):
        candidate=ops[:start]+ops[start+size:]
        if candidate and fails(candidate): ops=candidate; n=max(2,n-1); reduced=True; break
    if not reduced:
        if n>=len(ops): break
        n=min(len(ops),n*2)
data['operations']=ops
out=source.with_name(source.stem+'.min.json'); out.write_text(json.dumps(data,indent=2)+'\n'); print(out)
