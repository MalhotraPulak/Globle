#!/bin/bash

# Run Globle Web Build
# Starts a local HTTP server and opens the game in the browser

set -e  # Exit on error

# Configuration
OUTPUT_DIR="./web_build"
OUTPUT_FILE="index.html"
PORT=8080

# Check if build exists
if [ ! -f "$OUTPUT_DIR/$OUTPUT_FILE" ]; then
  echo "❌ Error: Web build not found at $OUTPUT_DIR/$OUTPUT_FILE"
  echo "Please build first:"
  echo "  ./build_web.sh"
  exit 1
fi

echo "🌍 Starting Globle Web Server..."
echo "📡 Server: http://localhost:$PORT"
echo "🎮 Game: http://localhost:$PORT/$OUTPUT_FILE"
echo ""
echo "Press Ctrl+C to stop the server"
echo ""

# Try to open in default browser
sleep 1
if command -v open &> /dev/null; then
  # macOS
  open "http://localhost:$PORT/$OUTPUT_FILE"
elif command -v xdg-open &> /dev/null; then
  # Linux
  xdg-open "http://localhost:$PORT/$OUTPUT_FILE"
fi

# Start Python HTTP server
cd "$OUTPUT_DIR"
python3 -m http.server $PORT
