"""Explicit, versioned field-to-expert mapping; no guessed optimization of INI symbols."""
from __future__ import annotations
from dataclasses import dataclass
from .ini import IniField, IniInventory

@dataclass(frozen=True)
class SemanticRule:
    name: str
    expert: str
    description: str
    capability: str = 'explain'
    requires: tuple[str,...] = ()

# These are illustrative mappings. Real mapping requires per-firmware verification.
RULES = {
    'engineCylinders': SemanticRule('engineCylinders','engine_setup','Number of configured engine cylinders'),
    'idleRpmPid': SemanticRule('idleRpmPid','idle','Idle-speed feedback controller settings'),
    'veTable': SemanticRule('veTable','fuel','Primary configured volumetric-efficiency calibration'),
    'stftEnabled': SemanticRule('stftEnabled','fuel_trims','Closed-loop short-term trim enable state'),
    'rpm': SemanticRule('rpm','foundation','Reported engine speed','analyze'),
    'lambdaCurrentlyGood': SemanticRule('lambdaCurrentlyGood','foundation','Reported wideband validity','analyze'),
    'Gego': SemanticRule('Gego','fuel_trims','Legacy fuel correction requiring firmware-specific scaling','explain'),
}

def describe_field(f: IniField) -> dict:
    rule = RULES.get(f.name)
    return {'key': f.key, 'name': f.name,
            'section': f.section, 'page': f.page, 'source_line': f.line,
            'definition_kind': f.kind,
            'owner': rule.expert if rule else None,
            'description': rule.description if rule else None,
            'capability': rule.capability if rule else 'structure_only',
            'rule_verified_for_ecu': False,
            'status': 'illustrative_rule_not_firmware_verified' if rule else 'unknown_semantics',
            'write_allowed': False}

def field_coverage(inv: IniInventory):
    entries=[describe_field(f) for f in inv.fields]
    unknown=[e for e in entries if e['owner'] is None]
    return {'total_fields':len(entries),'example_rules_matched':len(entries)-len(unknown),
            'unknown_fields':len(unknown),'unparsed_definitions':len(inv.unknown),
            'unprocessed_directives':len(inv.directives),
            'complete_semantic_coverage':False,
            'entries':entries}
