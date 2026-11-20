// Host/static regression checks for the ESP-hosted education boundary.
// These do NOT substitute for an ESP-IDF compile or an actual ECU test.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=(name)=>readFileSync(new URL(name,import.meta.url),'utf8');
const http=source('../main/seth_http.c');
const main=source('../main/main.c');
const cmake=source('../main/CMakeLists.txt');
const site=source('../web/seth.html');
const assets=source('../tools/generate_web_assets.py');
const ino=source('../main/ini_cache.c');

test('Seth C prototype linked and generated from authored catalog',()=>{
  for(const file of ['seth_agent.c','seth_actions.c','seth_coordinator.c','seth_packet_json.c','seth_evidence.c','esp_education_core.c'])
    assert.ok(cmake.includes(file),file);
  assert.match(cmake,/generate_catalog.py/);
  assert.match(cmake,/lesson-catalog\.v1\.json/);
});
test('versioned phone and portable DS endpoints registered',()=>{
  for(const path of ['/seth','/api/seth/v1/view','/api/seth/v1/lessons','/api/seth/v1/action'])
    assert.ok(main.includes('uri="'+path+'"'),path);
  assert.match(main,/seth_http_init\(\)/);
  assert.match(assets,/'seth_page': 'seth.html'/);
});
test('source validation requires exact INI signature and fresh telemetry',()=>{
  assert.match(ino,/bool ini_cache_matches_signature/);
  assert.match(http,/ini_cache_matches_signature\(signature\)/);
  assert.match(http,/stats\.age_ms <= 1200u/);
  assert.match(http,/unverified_no_sensor_values_decoded/);
  assert.match(http,/calibration_authority", false/);
});
test('no ECU command, persistent engine archive or automatic write in guide service',()=>{
  assert.doesNotMatch(http,/\bts_request\s*\(/);
  assert.doesNotMatch(http,/\b(vcp_gateway_send|writeArgs|burnArgs|httpd_req_send)\s*\(/);
  assert.doesNotMatch(http,/\b(nvs_set|fwrite|remove|rename)\s*\(/);
  assert.match(http,/engine_archive_saved", false/);
  assert.match(http,/ecu_writes_available", false/);
});
test('no open-AP fact submission, symptom archive or unauthorized actions',()=>{
  assert.match(http,/allowed_education_action/);
  assert.doesNotMatch(http,/"set_fact"/);
  assert.doesNotMatch(http,/"diagnose"/);
  assert.doesNotMatch(http,/"stim_start"/);
  assert.doesNotMatch(http,/"write"/);
  assert.match(http,/seth_dispatch\(&s_agent/);
});
test('phone script is syntactically valid and renders server text as text',()=>{
  const script=site.match(/<script>\s*([\s\S]+?)\s*<\/script>/);
  assert.ok(script,'inline client script');
  new Function(script[1]);
  assert.match(site,/\.textContent=/);
  assert.match(site,/\/api\/seth\/v1\/view/);
  assert.match(site,/\/api\/seth\/v1\/lessons/);
  assert.match(site,/\/api\/seth\/v1\/action/);
  assert.doesNotMatch(site,/\.innerHTML\s*=/);
});
