CC ?= cc
PYTHON ?= python3
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
BUILD := build
CATALOG := $(BUILD)/seth_catalog.c
SOURCES := seth_agent.c seth_packet_json.c seth_coordinator.c seth_actions.c seth_checkpoint.c seth_learning_path.c ../esp_education_core.c $(CATALOG)

.PHONY: all test clean
all: $(BUILD)/seth_test $(BUILD)/seth_extended_test $(BUILD)/seth_demo

$(BUILD):
	mkdir -p $@

$(CATALOG): generate_catalog.py ../../design/education/lesson-catalog.v1.json | $(BUILD)
	$(PYTHON) generate_catalog.py --output $@

$(BUILD)/seth_test: $(SOURCES) test_seth_agent.c seth_agent.h seth_packet_json.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -I.. $(SOURCES) test_seth_agent.c -o $@

$(BUILD)/seth_demo: $(SOURCES) demo.c seth_agent.h seth_packet_json.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -I.. $(SOURCES) demo.c -o $@

$(BUILD)/seth_extended_test: $(SOURCES) test_seth_extended.c seth_agent.h seth_coordinator.h seth_actions.h seth_checkpoint.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -I.. $(SOURCES) test_seth_extended.c -o $@

test: $(BUILD)/seth_test $(BUILD)/seth_extended_test
	./$(BUILD)/seth_test
	./$(BUILD)/seth_extended_test

clean:
	rm -rf $(BUILD)
