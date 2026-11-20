import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

const registry = JSON.parse(fs.readFileSync(
  new URL('../../../design/diagnostics/scenario-registry.v1.json', import.meta.url),
  'utf8'
));

test('scenario registry explicitly remains a design-only, read-only proposal', () => {
  assert.equal(registry.schema, 'brickos.guide.scenario.registry.v1');
  assert.equal(registry.status, 'DESIGN_ONLY_NOT_LOADED_BY_FIRMWARE');
  assert.ok(registry.cases.length >= 50, 'broad scenario taxonomy not populated');
  assert.equal(registry.unknown_fallback.policy, 'read_only');
  assert.ok(registry.unknown_fallback.route.includes('clarifying_question'));
});

test('each scenario has a unique stable ID and verified semantic role boundary', () => {
  const ids = new Set();
  for (const item of registry.cases) {
    assert.match(item.id, /^[a-z][a-z0-9_]+$/);
    assert.ok(!ids.has(item.id), 'duplicate '+item.id);
    ids.add(item.id);
    assert.ok(registry.topics[item.category], 'unknown topic '+item.category);
    assert.ok(item.semantic_roles.length > 0);
    assert.ok(item.semantic_roles.every(v=>/^[a-z][a-zA-Z0-9]+$/.test(v)), item.id);
    assert.ok(item.symptoms.length > 0);
    assert.ok(item.hypotheses.length >= 2, item.id);
    assert.ok(item.next_discriminating_question.endsWith('?'), item.id);
    assert.equal(item.expected_reference,'active_ini_strategy_or_verified_comparable_baseline');
    assert.equal(item.policy,'read_only');
    assert.equal(item.may_claim_cause,false);
    assert.equal(item.missing_data_action,'state_unknown_and_ask_for_evidence');
    assert.ok(!('raw_offset' in item) && !('can_write' in item) && !('flash' in item));
  }
});

test('all principal symptom families are covered without implying exhaustive engine diagnosis', () => {
  for (const topic of [
    'connection','sensors','starting','trigger','idle','warmup',
    'fuel','transient','ignition','advanced','diagnostics','validation'
  ]) assert.ok(registry.cases.some(x=>x.category===topic),topic);
  for (const id of ['crank_cough_sputter','idle_hunts','fuel_trim_masking',
     'cruise_lambda_uncertain','spark_optimality_unknown','eculog_partial']) {
     assert.ok(registry.cases.some(x=>x.id===id),id);
  }
  assert.equal(registry.unknown_fallback.id,'unclassified_symptom');
  assert.match(registry.format_notes,/NOT raw INI names/);
});
