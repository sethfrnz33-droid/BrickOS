from pathlib import Path
m=(Path(__file__).resolve().parents[1]/'main/main.c').read_text()
i=(Path(__file__).resolve().parents[1]/'main/ini_cache.c').read_text()
assert 'esp_wifi_set_mode(WIFI_MODE_AP)' in m
assert 'esp_wifi_set_mode(WIFI_MODE_APSTA)' in i
assert 'g_boot_counter' in m and 'wifi_last_disconnect_reason' in m
assert 'brick_usb_late_start' in m and 'vTaskDelay(pdMS_TO_TICKS(8000)' in m
assert 'BRICK_LED_HEARTBEAT GPIO_NUM_17' in m
assert 'BRICK_LED_WIFI_CLIENT GPIO_NUM_18' in m
print('PASS: AP-only boot, temporary STA, boot counter, USB delay, LED config')
