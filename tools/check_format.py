#!/usr/bin/env python3
from pathlib import Path
import sys

roots = [Path('include'), Path('tests'), Path('verify'), Path('bench')]
exts = {'.hpp', '.h', '.cpp', '.cc', '.cxx'}
errors = []
for root in roots:
    if not root.exists():
        continue
    for path in root.rglob('*'):
        if not path.is_file() or path.suffix not in exts:
            continue
        text = path.read_text()
        if not text.endswith('\n'):
            errors.append(f'{path}: missing final newline')
        for line_no, line in enumerate(text.splitlines(), 1):
            if line != line.rstrip():
                errors.append(f'{path}:{line_no}: trailing whitespace')
            if '\t' in line:
                errors.append(f'{path}:{line_no}: tab character')
            if len(line) > 120:
                errors.append(f'{path}:{line_no}: line length {len(line)} > 120')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
print('format policy: PASS')
