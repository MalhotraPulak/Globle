# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A C11 recreation of the Globle game - a geography guessing game where players identify a mystery country through proximity-based feedback. Built with Raylib for 3D rendering, featuring an interactive globe with country boundary overlays and a color-coded distance system.

## Build & Run Commands

**IMPORTANT**: Always prefer running the web version for testing and development. Use the web version commands below instead of the native build.

- **Build and run web version (PREFERRED)**:
  - `lsof -ti:8080 | xargs kill -9 2>/dev/null; bash ./build_web.sh && bash ./run_web.sh &`
  - Compiles to WebAssembly using Emscripten
  - Starts local web server on port 8080
  - Opens browser automatically at http://localhost:8080

- **Build and run native**: `./run.sh`
  - Compiles main.c, geodata.c, game.c with C11 standard
  - Links Raylib via pkg-config and math library (-lm)
  - Automatically launches the game after successful compilation

- **Build only**: `cc -std=c11 main.c geodata.c game.c $(pkg-config --libs --cflags raylib) -lm -o main`

- **Clean build artifacts**: `rm -f main a.out`

- **Prerequisites**:
  - For native: `raylib` installed and accessible via `pkg-config`
  - For web: `emscripten` installed (install via `brew install emscripten`)
  - Math library (standard on Unix systems)
  - Country data file at `./coordinates/ccc.csv`

## Code Architecture

The codebase is organized into three main modules: geodata (geographic data loading/parsing), game (game logic and state), and main (rendering and UI).

### Geographic Data Module (`geodata.h`, `geodata.c`)

Handles loading and parsing country boundary data from CSV files:

- **Data Structures**:
  - `GeoPoint`: Latitude/longitude coordinate pair
  - `vec`: Generic dynamic array with automatic growth
  - `Polygon`: Collection of GeoPoints forming a country boundary
  - `CountryData`: Complete country record with name, metadata, polygons, and calculated centroid
  - `CountryDatabase`: Collection of all countries loaded from CSV

- **Key Functions**:
  - `loadCountryDatabase()`: Parses semicolon-delimited CSV with embedded GeoJSON coordinates
  - `calculateCentroid()`: Averages all polygon points to find country center (used for distance calculations)
  - `getCountryByName()`: Case-insensitive country lookup
  - Handles both Polygon and MultiPolygon geometries from the CSV

### Game Logic Module (`game.h`, `game.c`)

Implements Globle game mechanics independent of rendering:

- **Data Structures**:
  - `Guess`: Single guess with country, distance (km), and assigned color
  - `GameState`: Tracks mystery country, all guesses, win status, search text, and closest guess

- **Core Algorithms**:
  - `calculateDistance()`: Haversine formula for great-circle distance between lat/lon points
  - `getColorForDistance()`: Maps distance to color gradient (white→blue→yellow→orange→red, green for correct)
    - Uses normalized distance (0-1 range, max 20,000km)
    - 5-segment gradient with smooth interpolation between color stops
  - `makeGuess()`: Validates guess, calculates distance, assigns color, detects win
  - `updateClosestGuess()`: Tracks which guess is nearest to mystery country

### Main Application (`main.c`)

Integrates all components into a playable game with 3D rendering and UI:

- **3D Rendering**:
  - `latLonToSphere()`: Projects lat/lon coordinates to 3D sphere surface using spherical coordinates
  - `drawCountryPolygon()`: Renders country boundaries as connected 3D line segments
  - Globe uses earth texture with rotation (auto-spin + manual arrow key control)
  - Guessed countries overlay on globe surface with distance-based colors
  - Closest guess rendered with brightened color

- **UI System**:
  - Search interface with text input and live autocomplete dropdown (ENTER to activate)
  - Type country names with real-time filtering (case-insensitive substring matching)
  - Navigate results with arrow keys, select with ENTER, cancel with ESC
  - Guess history panel shows recent 15 guesses with distances and colors
  - Win screen overlay when correct country is found

- **Input Flow**:
  1. Press ENTER → activates search mode
  2. Type country name → filters database in real-time
  3. Arrow keys navigate results, ENTER selects
  4. Game calculates distance, assigns color, updates globe
  5. Repeat until mystery country is guessed correctly

### Data Files

- **`coordinates/ccc.csv`**: Semicolon-delimited country database (9MB)
  - Format: geoPoint;geoShape;territoryCode;status;countryCode;englishName;continent;region;alpha2
  - geoShape contains embedded GeoJSON-like coordinate arrays
  - Loaded at startup by `loadCountryDatabase()`

- **Textures**:
  - `earth2.jpg`: Globe texture (3.5MB) - actively used
  - `earth.jpg`: Alternative texture (760KB) - unused

- **Other coordinate files** (not currently used):
  - `world-administrative-boundaries.geojson`: Pure GeoJSON format
  - `world-administrative-boundaries.json`: JSON format

### Third-Party Dependencies

- **`raylib/`**: Bundled Raylib source (subdirectory)
  - System-installed version used via pkg-config, not the bundled copy
  - Provides 3D rendering, input handling, and basic UI primitives

- **`stb/`**: STB single-file libraries (subdirectory)
  - Used by Raylib for image loading

## Development Notes

- **Working Directory**: Must run from repository root for asset paths (`earth2.jpg`, `coordinates/ccc.csv`) to resolve

- **Coding Style**:
  - 2-space indentation for main.c, geodata.c, game.c
  - K&R brace style (opening brace on same line)
  - Snake_case for functions and variables
  - Use Raylib's built-in constants (DEG2RAD, PI) instead of redefining

- **Performance Considerations**:
  - CSV parsing happens at startup - expect ~2-3 second load time for 250+ countries
  - Country polygons can have thousands of points - rendering is done via DrawLine3D (not optimized meshes)
  - Search filter runs on every keystroke - O(n) scan of all country names

- **Game Mechanics**:
  - Mystery country selected randomly at startup using current time as seed
  - Distance calculated using haversine formula (great-circle distance on sphere)
  - Color gradient based on normalized distance: 0-20,000km mapped to 0.0-1.0
  - Maximum 500 guesses allowed (defined by MAX_GUESSES in game.h)

## Key Files

- **Core source**: `main.c`, `geodata.c`, `game.c`
- **Headers**: `geodata.h`, `game.h`
- **Build script**: `run.sh`
- **Legacy**: `csvParser.c` (original parser, now superseded by geodata.c), `main.c.backup` (old globe demo)
