#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
SPHINX_HTML_DIR="${1:-doc}"

# ── Step 1: Generate daslang API docs ──
echo "Generating daslang API docs..."
python3 "$SCRIPT_DIR/source/das_api_gen.py" --source "$PROJECT_DIR/src" --output "$SCRIPT_DIR/source/stdlib"

# ── Step 2: Run Sphinx (hawkmoth parses C++ source directly) ──
echo "Building Sphinx ($SPHINX_HTML_DIR)..."
sphinx-build --keep-going -b html "$SCRIPT_DIR/source" "$BUILD_DIR/html-$SPHINX_HTML_DIR"

echo ""
echo "Documentation: $BUILD_DIR/html-$SPHINX_HTML_DIR/index.html"
