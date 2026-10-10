#!/usr/bin/env python3
"""Falsify missing/duplicate code-sharing evidence; fixtures are not compiler artifacts."""
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch
from audit_common_code import audit

with TemporaryDirectory() as temp:
    root = Path(temp)
    names, demangled, symbols = [], [], []
    for c in (2, 64, 256, 1024, 65536):
        for p in (8, 16, 64, 256):
            for impl in range(1, 8):
                for owner in (1, 2):
                    n = len(names)
                    names.append(f'_Zfixture{n}_M_runEv')
                    dem = (f'run_implementation<payload<{p}ul>, {c}ul>'
                           f' {{lambda(payload<{p}ul> const&)#{impl}}}::{{lambda()#{owner}}}')
                    demangled.append(dem)
                    symbols.append(f'{4096 + n * 16:016x} T {dem}')
    assembly, linked = root / 'fixture.s', root / 'symbols.txt'
    assembly.write_text('\n'.join(n + ':' for n in names))
    linked.write_text('\n'.join(symbols))
    def reject():
        try:
            audit(assembly, linked)
        except ValueError:
            return
        raise AssertionError('corrupt shared-code evidence accepted')
    with patch('audit_common_code.subprocess.run', return_value=SimpleNamespace(stdout='\n'.join(demangled))):
        result = audit(assembly, linked)
        assert result['shared_thread_functions'] == 40 and result['linked_symbols_verified']
        assembly.write_text('\n'.join(n + ':' for n in names[:-1]))
        reject()
        assembly.write_text('\n'.join(n + ':' for n in names) + '\n' + names[4] + ':')
        reject()
        assembly.write_text('\n'.join(n + ':' for n in names))
        linked.write_text('\n'.join(symbols[:4] + symbols[5:]))
        reject()
        linked.write_text('\n'.join(symbols))
    changed = demangled.copy()
    changed[4] = changed[4].replace('const&)#3', 'const&)#4')
    with patch('audit_common_code.subprocess.run', return_value=SimpleNamespace(stdout='\n'.join(changed))):
        reject()
print('Shared-code audit falsification passed; synthetic symbols are not assembly evidence.')
