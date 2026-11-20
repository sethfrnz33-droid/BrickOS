CC ?= cc
PYTHON ?= python3
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
BUILD := build
CATALOG := $(BUILD)/seth_catalog.c
SOURCES := seth_agent.c seth_packet_json.c ../esp_education_core.c $(CATALOG)

.PHONY: all test clean
all: $(BUILD)/seth_test $(BUILD)/seth_demo

$(BUILD):
	mkdir -p $@

$(CATALOG): generate_catalog.py ../../design/education/lesson-catalog.v1.json | $(BUILD)
	$(PYTHON) generate_catalog.py --output $@

$(BUILD)/seth_test: $(SOURCES) test_seth_agent.c seth_agent.h seth_packet_json.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -I.. $(SOURCES) test_seth_agent.c -o $@

$(BUILD)/seth_demo: $(SOURCES) demo.c seth_agent.h seth_packet_json.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -I.. $(SOURCES) demo.c -o $@

test: $(BUILD)/seth_test
	./$(BUILD)/seth_test

clean:
	rm -rf $(BUILD)
