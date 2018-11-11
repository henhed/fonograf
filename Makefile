CC ?= gcc

SRC_PATH = src
BUILD_PATH = build
LV2_PATH = $(BUILD_PATH)/extension
LV2_NAME = fonograf.so
LV2UI_NAME = fonograf_ui.so

SOURCES = $(SRC_PATH)/lv2/fonograf.c
OBJECTS = $(SOURCES:$(SRC_PATH)/%.c=$(BUILD_PATH)/%.o)
DEPS = $(OBJECTS:.o=.d)

UI_SOURCES = $(SRC_PATH)/lv2/ui.c \
	$(SRC_PATH)/gtk3/element.c \
	$(SRC_PATH)/gtk3/node.c \
	$(SRC_PATH)/gtk3/edge.c \
	$(SRC_PATH)/gtk3/graph.c
UI_OBJECTS = $(UI_SOURCES:$(SRC_PATH)/%.c=$(BUILD_PATH)/%.o)
UI_DEPS = $(UI_OBJECTS:.o=.d)

COMPILER_FLAGS = -std=c99 -fpic -Wall -Wextra -Werror
LIBS =
UI_LIBS =

.PHONY: release
release: export CCFLAGS := $(CCFLAGS) $(COMPILER_FLAGS)
release: dirs
	@$(MAKE) all

.PHONY: dirs
dirs:
	@echo "Creating directories"
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(dir $(UI_OBJECTS))
	@mkdir -p $(LV2_PATH)

.PHONY: clean
clean:
	@echo "Deleting directories"
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(LV2_PATH)

.PHONY: all
all: $(LV2_PATH)/$(LV2_NAME) $(LV2_PATH)/$(LV2UI_NAME)
	@echo "Assembling LV2 bundle: $(LV2_PATH)"
	@cp lv2/manifest.ttl.in $(LV2_PATH)/manifest.ttl
	@cp lv2/fonograf.ttl.in $(LV2_PATH)/fonograf.ttl
	@cp lv2/ui.css $(LV2_PATH)/ui.css

$(LV2_PATH)/$(LV2_NAME): $(OBJECTS)
	@echo "Linking extension: $@"
	$(CC) -shared $(OBJECTS) $(LIBS) -o $@

$(LV2_PATH)/$(LV2UI_NAME): $(UI_OBJECTS)
	@echo "Linking UI: $@"
	$(CC) -shared $(UI_OBJECTS) $(UI_LIBS) -o $@

-include $(DEPS)

$(BUILD_PATH)/lv2/fonograf.o: $(SRC_PATH)/lv2/fonograf.c
	@echo "Compiling $< -> $@"
	$(CC) $(CCFLAGS) -c $< -o $@

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo "Compiling $< -> $@"
	$(CC) $(CCFLAGS) $(shell pkg-config --cflags gtk+-3.0) -c $< -o $@
