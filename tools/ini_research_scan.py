#!/usr/bin/env python3
"""Read-only research scanner for generated rusEFI / TunerStudio INIs.

Does not evaluate expressions, execute controller commands, or connect to an ECU.
Preserves parser warnings for manual review rather than shifting offsets.
"""
import argparse
import hashlib
import json
import math
import re
from collections import Counter, defaultdict
from pathlib import Path

DECL = re.compile(r'^([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(scalar|bits|array|string)\s*,\s*([^,]+)\s*,\s*(.*)$')
SEC = re.compile(r'^\[([A-Za-z][A-Za-z_0-9]*)\]$')
SHAPE = re.compile(r'^\[([0-9]+(?:x[0-9]+)*)\]$')
TYPE_BYTES = {'U08': 1, 'S08': 1, 'U16': 2, 'S16': 2, 'U32': 4, 'S32': 4, 'F32': 4, 'F64': 8}


def scan(path):
    raw = Path(path).read_bytes()
    # ASCII directives remain byte-compatible. Byte offsets are not text positions.
    lines = raw.decode('cp1252', errors='replace').splitlines()
    section, page = '', None
    constants, outputs, sections, tables, submenus, buttons, anomalies = [], [], [], [], [], [], []
    counts, config = Counter(), {}
    for n, original in enumerate(lines, start=1):
        line = original.strip()
        sm = SEC.fullmatch(line)
        if sm:
            section = sm.group(1)
            sections.append({'name': section, 'line': n})
            continue
        if not line or line[0] in ';!#':
            continue
        counts[section] += 1
        if section == 'Constants':
            pm = re.match(r'^page\s*=\s*(\d+)\b', line)
            if pm:
                page = int(pm[1])
                continue
        if section in ('Constants', 'OutputChannels'):
            dm = DECL.match(line)
            if dm:
                name, kind, dtype, tail = dm.groups()
                parts = [p.strip() for p in tail.split(',')]
                field = {'name': name, 'kind': kind, 'dtype': dtype.strip(), 'line': n}
                if section == 'Constants':
                    field['page_ordinal'] = page
                if not parts or not re.fullmatch(r'\d+', parts[0]):
                    anomalies.append({'line': n, 'field': name, 'reason': 'invalid/missing offset token',
                                      'prefix': line[:110]})
                    continue
                field['offset'] = int(parts[0])
                width = TYPE_BYTES.get(field['dtype'])
                if kind in ('scalar', 'bits'):
                    length = width
                elif kind == 'array':
                    match = SHAPE.fullmatch(parts[1]) if len(parts) > 1 else None
                    field['shape'] = list(map(int, match[1].split('x'))) if match else None
                    length = width * math.prod(field['shape']) if width and field['shape'] else None
                else:
                    length = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None
                field['byte_length'] = length
                if kind == 'bits' and len(parts) > 1:
                    match = re.fullmatch(r'\[(\d+):(\d+)\]', parts[1])
                    field['bits'] = [int(match[1]), int(match[2])] if match else None
                field['flags'] = [flag for flag in ('noMsqSave', 'controllerPriority') if flag in line]
                (constants if section == 'Constants' else outputs).append(field)
                if length is None:
                    anomalies.append({'line': n, 'field': name, 'reason': 'unsupported type/shape'})
                continue
        if section == 'Constants':
            m = re.match(r'^([\w]+)\s*=\s*(.+)$', line)
            if m and m[1] in {'nPages', 'pageSize', 'pageIdentifier', 'burnCommand',
                              'endianness', 'blockingFactor'}:
                config[m[1]] = m[2].split(';', 1)[0].strip()
        elif section == 'OutputChannels':
            m = re.match(r'^ochBlockSize\s*=\s*(\d+)', line)
            if m:
                config['ochBlockSize'] = int(m[1])
        elif section == 'TunerStudio':
            m = re.match(r'^(signature|defaultRuntimeRecordPerSec)\s*=\s*(.+)$', line)
            if m:
                config[m[1]] = m[2].split(';', 1)[0].strip()
        if section == 'TableEditor' and line.startswith('table ='):
            tables.append({'line': n, 'entry': line[:250]})
        if section == 'Menu' and line.startswith('subMenu ='):
            submenus.append({'line': n, 'entry': line[:230]})
        if section == 'UserDefined' and line.startswith('commandButton ='):
            buttons.append({'line': n, 'entry': line[:230]})

    sizes = [int(p.strip()) for p in config.get('pageSize', '').split(',') if p.strip().isdigit()]
    if len(sizes) != int(config.get('nPages', len(sizes))):
        anomalies.append({'reason': 'nPages/pageSize mismatch'})
    aliases = defaultdict(list)
    for field in constants:
        page_id, length = field.get('page_ordinal'), field.get('byte_length')
        if not isinstance(page_id, int) or not isinstance(length, int) or page_id < 1 or page_id > len(sizes):
            anomalies.append({'line': field['line'], 'field': field['name'], 'reason': 'page/size missing'})
            continue
        if field['offset'] + length > sizes[page_id - 1]:
            anomalies.append({'line': field['line'], 'field': field['name'], 'reason': 'field overflows page'})
        if field['kind'] == 'array':
            aliases[(page_id, field['offset'], length)].append(field['name'])

    return {
        'schema': 'brickos.ini.research.inspect.v1',
        'source_name': Path(path).name, 'sha256': hashlib.sha256(raw).hexdigest(),
        'bytes': len(raw), 'line_count': len(lines), 'live_ecu_signature_verified': False,
        'text_encoding_policy': 'cp1252 compatibility for non-ASCII; offsets byte-based',
        'config': config, 'page_sizes_bytes': sizes, 'sections': sections, 'entry_counts': dict(counts),
        'counts': {'calibration_fields_parsed': len(constants), 'output_fields': len(outputs),
                   'table_editors': len(tables), 'menu_submenus': len(submenus),
                   'command_buttons': len(buttons), 'unparsed_calibration_declarations':
                   sum(1 for a in anomalies if a.get('reason') == 'invalid/missing offset token')},
        'calibration_fields': constants, 'output_fields': outputs,
        'table_editors': tables, 'submenus': submenus, 'command_buttons': buttons,
        'same_address_array_groups': [{'page_ordinal': k[0], 'offset': k[1], 'bytes': k[2], 'names': names}
                                      for k, names in aliases.items() if len(names) > 1],
        'anomalies': anomalies, 'read_only': True,
        'note': 'Declared INI metadata only, not proof of matched ECU, active settings or physical hardware.'
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ini', help='Generated INI to scan')
    parser.add_argument('--output', help='JSON result path (stdout if omitted)')
    args = parser.parse_args()
    report = scan(args.ini)
    body = json.dumps(report, indent=2, ensure_ascii=False) + '\n'
    if args.output:
        Path(args.output).write_text(body, encoding='utf-8')
        print(f'{len(report["anomalies"])} anomalies recorded in {args.output}')
    else:
        print(body, end='')


if __name__ == '__main__':
    main()
