BUILD_DIR=build

.PHONY: all configure build clean

all: build

configure:
	cmake -B $(BUILD_DIR)

build: configure
	cmake --build $(BUILD_DIR) -j2

clean:
	rm -rf $(BUILD_DIR)

uml:
	clang-uml
	plantuml --format svg build/docs/diagrams/*.puml
