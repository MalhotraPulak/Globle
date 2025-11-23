#!/bin/bash

# Web Build Script for Globle Game
# Compiles the C source to WebAssembly using Emscripten

set -e  # Exit on error

echo "🌍 Building Globle for Web..."
echo ""

# Configuration
RAYLIB_PATH="./raylib"
OUTPUT_DIR="./web_build"
OUTPUT_FILE="index.html"

# Check if emcc is available
if ! command -v emcc &> /dev/null; then
  echo "❌ Error: emcc (Emscripten) not found!"
  echo "Please install Emscripten:"
  echo "  brew install emscripten"
  exit 1
fi

# Check if raylib source exists
if [ ! -d "$RAYLIB_PATH/src" ]; then
  echo "❌ Error: Raylib source not found at $RAYLIB_PATH/src"
  exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check if raylib is compiled for web
if [ ! -f "$RAYLIB_PATH/src/libraylib.web.a" ]; then
  echo "📦 Compiling Raylib for web (PLATFORM_WEB)..."
  cd "$RAYLIB_PATH/src"
  make PLATFORM=PLATFORM_WEB -B
  cd ../..
  echo "✓ Raylib compiled for web"
  echo ""
fi

# Compile the game
echo "🔨 Compiling Globle game..."
emcc -o "$OUTPUT_DIR/$OUTPUT_FILE" \
  main.c geodata.c game.c \
  -Os -Wall \
  -I"$RAYLIB_PATH/src" \
  -L"$RAYLIB_PATH/src" \
  "$RAYLIB_PATH/src/libraylib.web.a" \
  -s USE_GLFW=3 \
  -s ASYNCIFY \
  -s ASYNCIFY_STACK_SIZE=81920 \
  -s INITIAL_MEMORY=256MB \
  -s TOTAL_STACK=64MB \
  -s FORCE_FILESYSTEM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  --preload-file coordinates \
  --preload-file earth2.jpg \
  --shell-file "$RAYLIB_PATH/src/minshell.html" \
  -DPLATFORM_WEB

echo ""
echo "✅ Build complete!"
echo "📁 Output: $OUTPUT_DIR/$OUTPUT_FILE"
echo ""
echo "🚀 To run the game:"
echo "   ./run_web.sh"
echo ""
echo "Or manually:"
echo "   cd $OUTPUT_DIR"
echo "   python3 -m http.server 8080"
echo "   Then open: http://localhost:8080/$OUTPUT_FILE"
