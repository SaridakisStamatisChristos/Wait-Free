#!/usr/bin/env python3
"""Counter parsing falsification only; synthetic counts are not PMU evidence."""
from profile_common_placement import EVENTS, capability, parse_counts

valid = '\n'.join(f'12345;;{event};1000000;100.00;;' for event in EVENTS)
ready, counts = capability(0, valid)
assert ready and set(counts) == set(EVENTS)
for bad in (valid.replace('12345', '<not supported>'), valid.replace('100.00', '89.99'),
            valid.replace('12345', 'nan'), valid.replace('12345', '-1'),
            valid.replace('100.00', '100.01'), valid.replace('1000000', '0')):
    assert not capability(0, bad)[0]
assert not capability(255, valid)[0]
assert parse_counts('<not counted>;;cycles:u;1000000;100.00;;') == {}
try:
    parse_counts(valid + '\n' + valid)
except ValueError:
    pass
else:
    raise AssertionError('duplicate counters accepted')
print('PMU capability/parser falsification passed; synthetic values are not profiling evidence.')
