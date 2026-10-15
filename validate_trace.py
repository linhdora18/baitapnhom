#!/usr/bin/env python3
"""Structural validator for the supplied recorded trace.json (does not regenerate frames)."""
import json
from pathlib import Path

path = Path(__file__).with_name('trace.json')
data = json.loads(path.read_text(encoding='utf-8'))
assert isinstance(data, list) and len(data) == 22, 'Expected 22 trace scenarios'
assert all(item.get('pass') is True for item in data), 'One or more scenarios are not marked pass=true'
frame_count = 0
for scenario in data:
    assert isinstance(scenario.get('frames'), list), f"Missing frames: {scenario.get('name')}"
    for frame in scenario['frames']:
        frame_count += 1
        assert isinstance(frame.get('s'), list) and len(frame['s']) == 5, f"Expected five slot states in {scenario.get('name')}"

slot5 = next(item for item in data if item.get('name', '').startswith('5 slot độc lập'))
assert any(
    len(frame['s']) == 5 and frame['s'][4][0] == 1 and all(frame['s'][i][0] == 0 for i in range(4))
    for frame in slot5['frames']
), 'Slot-5 scenario does not show only the fifth actuator active'

print(f'OK: {len(data)} recorded scenarios, {frame_count} frames, five slots per frame.')
print('OK: all recorded scenarios have pass=true.')
print('OK: slot-5 scenario shows only internal actuator index 4 active.')
print('NOTE: this validates recorded trace data; it does not regenerate the trace from the C++ test binary.')
