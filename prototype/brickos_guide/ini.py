"""Loss-preserving *subset* of TunerStudio/rusEFI INI inventory parser.

Deliberately does NOT claim to evaluate macros, conditional compilation, formulas,
complex dialog expressions, or to decode arbitrary ECU RAM. Unrecognized lines remain
in the inventory, rather than silently vanishing.
"""
from __future__ import annotations
import csv
from dataclasses import dataclass, field, asdict
import hashlib
import io
import re

DECL = {'scalar', 'bits', 'array', 'curve', 'table', 'string', 'value'}
SECTION = re.compile(r'^\s*\[([^]]+)\]\s*$')
ASSIGN = re.compile(r'^\s*([A-Za-z_]\w*)\s*=\s*(.*)$')

@dataclass(frozen=True)
class IniField:
    key: str
    section: str
    page: int | None
    name: str
    kind: str
    raw: str
    line: int
    decode_status: str
    args: tuple[str, ...] = ()

    def json(self):
        d = asdict(self)
        d['args'] = list(self.args)
        return d

@dataclass
class IniInventory:
    signature: str | None
    fingerprint: str
    fields: list[IniField] = field(default_factory=list)
    unknown: list[dict] = field(default_factory=list)
    directives: list[dict] = field(default_factory=list)

    def summary(self):
        return {'signature': self.signature, 'fingerprint': self.fingerprint,
                'field_count': len(self.fields), 'unknown_count': len(self.unknown),
                'directives_count': len(self.directives),
                'decoded_count': sum(f.decode_status == 'recognized_structure' for f in self.fields)}

def remove_comment(s: str) -> str:
    # INI semicolons in quoted labels are not comments.
    quote = False
    escaped = False
    for i, ch in enumerate(s):
        if ch == '"' and not escaped:
            quote = not quote
        if ch == ';' and not quote:
            return s[:i].rstrip()
        escaped = ch == '\\' and not escaped
    return s.strip()

def split_args(raw: str) -> tuple[str, ...]:
    try:
        return tuple(x.strip() for x in next(csv.reader(io.StringIO(raw), skipinitialspace=True)))
    except (csv.Error, StopIteration):
        return (raw,)

def parse_ini(text: str) -> IniInventory:
    digest = hashlib.sha256(text.encode('utf8')).hexdigest()
    inv = IniInventory(None, digest)
    section, page = '', None
    for lineno, line in enumerate(text.splitlines(), 1):
        s = remove_comment(line)
        if not s:
            continue
        sec = SECTION.match(s)
        if sec:
            section, page = sec.group(1), None
            continue
        if s.lstrip().startswith(('#if', '#else', '#endif', '#define', '#include', '#unset')):
            inv.directives.append({'line': lineno, 'section': section, 'raw': s,
                                   'status': 'not_evaluated'})
            continue
        m = ASSIGN.match(s)
        if not m:
            inv.unknown.append({'line': lineno, 'section': section, 'raw': s,
                                'reason': 'unparsed_syntax'})
            continue
        name, raw = m.groups()
        if name.lower() == 'signature' and section.lower() in ('tunerstudio', 'megatune'):
            inv.signature = raw.strip().strip('"')
        if section.lower() == 'constants' and name.lower() == 'page':
            try:
                page = int(raw.strip(), 0)
            except ValueError:
                inv.unknown.append({'line': lineno, 'section': section,
                                    'raw': s, 'reason': 'invalid_page'})
            continue
        args = split_args(raw)
        kind = args[0].lower() if args else ''
        is_data_field = section.lower() in ('constants', 'outputchannels')
        if is_data_field and kind in DECL:
            key = f'{section}:{page if section.lower()=="constants" else "och"}:{name}'
            inv.fields.append(IniField(key, section, page if section.lower()=='constants' else None,
                                       name, kind, raw, lineno, 'recognized_structure', args))
        elif is_data_field and kind not in ('',) and name.lower() not in {
                'endianness','pagesize','pageidentifier','blockingfactor','ochblocksize',
                'pageactivate','pagereadcommand','pagewritecommand','burncommand',
                'signature','nocommreaddelay','pagechunkwrite'}:
            inv.unknown.append({'line': lineno, 'section': section, 'name': name,
                                'raw': s, 'reason': 'unknown_definition_kind'})
    return inv
