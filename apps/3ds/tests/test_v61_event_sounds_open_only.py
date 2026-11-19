#!/usr/bin/env python3
"""Guard against contextual samples being fired merely by cursor/touch hover."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'source/main.c').read_text()

def section(start,end):
    return main.split(start,1)[1].split(end,1)[0]

# Browse and preview are silent, whether controlled by touch or key.
touch=section('if(ui_page==PAGE_TUNE_TREE){\n    if(tp.py>=30', 'if(ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL){')
assert 'tune_lower_selected=idx' in touch
assert 'bts_context_named(' not in touch
assert 'tune_enter_preview()' in touch
buttons=section('if(ui_page==PAGE_TUNE_TREE){\n   if(k&KEY_SELECT)', 'if(in_map_view()&&map_3d_view')
assert 'tune_lower_selected++' in buttons
assert 'bts_context_named(' not in buttons
assert 'tune_enter_preview()' in buttons
# Opening a real item still fires the sound, with cooldown in the event player.
open_entries=section('static void tune_enter_preview(void){','static void enter_selected(void){')
assert 'bts_context_named(m->label,m->target)' in open_entries
open_targets=section('static void open_target(const char*target,const char*label){','static void edit_history_capture(')
assert 'bts_context_named(label,target)' in open_targets
assert 'if(bts_context_last_at[cue] && now-bts_context_last_at[cue]<2500ULL)' in main
# No sounds from single-step numeric/curve/map adjustments.
adjust=section('static void adjust_cell(double dir){','static void numeric_edit_prompt(void){')
assert 'bts_context_named(' not in adjust
# Actual option/setting choice events and the Undo cue remain.
assert 'bts_sound_for_change(f->name,choices[selected].label,old_choice,(double)chosen)' in main
assert 'bts_audio_play(BTS_SOUND_UNDO,event_audio_enabled)' in main
assert 'bts_audio_start_burn_loop' in main
print('PASS: no sounds from highlighting; actual opens and value-choice events remain')
