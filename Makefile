LLVM21=/usr/lib/llvm21
BUILD_DIR=build

export PATH := $(LLVM21)/bin:$(PATH)

CMAKE_FLAGS= \
	-DCMAKE_C_COMPILER=$(LLVM21)/bin/clang \
	-DCMAKE_CXX_COMPILER=$(LLVM21)/bin/clang++ \
	-DCMAKE_LINKER=$(LLVM21)/bin/ld.lld \
	-DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" \
	-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DCMAKE_BUILD_TYPE=Debug

.PHONY: all configure build clean

all: build

configure:
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR) -j6

clean:
	rm -rf $(BUILD_DIR)

uml:
	clang-uml
	plantuml --format svg build/docs/diagrams/*.puml
