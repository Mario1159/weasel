BUILD_DIR=build

# C++20 modules require the Ninja generator (see CXX_MODULES_PLAN.md). Pass
# WEASEL_MODULES=1 to configure/build with modules enabled.
ifeq ($(WEASEL_MODULES),1)
CMAKE_GEN ?= -G Ninja -DWEASEL_ENABLE_MODULES=ON
else
CMAKE_GEN ?=
endif

.PHONY: all configure build clean

all: build

configure:
	cmake -B $(BUILD_DIR) $(CMAKE_GEN)

build: configure
	cmake --build $(BUILD_DIR) -j2

clean:
	rm -rf $(BUILD_DIR)

uml:
	clang-uml
	plantuml --format svg build/docs/diagrams/*.puml
