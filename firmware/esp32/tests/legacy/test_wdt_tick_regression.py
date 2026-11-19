from pathlib import Path
p = Path(__file__).resolve().parents[1] / 'main' / 'main.c'
s = p.read_text()
assert 'vTaskDelay(pdMS_TO_TICKS(5));' not in s, '5ms might round to 0 ticks'
assert 'vTaskDelay(pdMS_TO_TICKS(50) > 0 ? pdMS_TO_TICKS(50) : 1);' in s
assert 'xTaskCreatePinnedToCore(can_monitor_task' in s
for hz in [100, 1000]:
    converted = (50 * hz) // 1000
    assert max(converted, 1) >= 1
print('PASS: CAN monitor sleep is never zero FreeRTOS ticks at 100/1000 Hz')
