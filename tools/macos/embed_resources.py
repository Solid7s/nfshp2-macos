#!/usr/bin/env python3
"""Embed the port-owned resources from speed.rc for non-Windows hosts."""
from pathlib import Path
import re
import sys

runtime = Path(__file__).resolve().parents[2] / 'recomp' / 'runtime'
out = Path(sys.argv[1])
parts = ['#pragma once\n#include <cstddef>\n#include <cstdint>\n']
entries = []
for name, rel in re.findall(r'^(\w+) RCDATA "([^"]+)"', (runtime / 'speed.rc').read_text(), re.M):
    data = (runtime / rel).read_bytes()
    parts.append(f'static const uint8_t resource_{name}[] = {{\n')
    for at in range(0, len(data), 24):
        parts.append(','.join(str(x) for x in data[at:at + 24]) + ',\n')
    parts.append('};\n')
    entries.append(f'{{L"{name}", resource_{name}, sizeof(resource_{name})}}')
parts.append('static const struct { const wchar_t *name; const uint8_t *data; size_t size; } hp2_resources[] = {\n')
parts.append(',\n'.join(entries) + '\n};\n')
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(''.join(parts))
