#include "geodata.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COORDINATES_FILE_SIZE (9 * 1000 * 1000 * 1000ll)

// Vector functions
vec *vec_init(uint64_t item_size, uint64_t capacity) {
  vec *v = calloc(sizeof(vec), 1);
  v->size = 0;
  v->itemSize = item_size;
  v->capacity = capacity;
  v->p = calloc(capacity, item_size);
  return v;
}

void vec_append(vec *vec, void *item) {
  if (vec->size >= vec->capacity) {
    // Grow capacity
    vec->capacity *= 2;
    vec->p = realloc(vec->p, vec->capacity * vec->itemSize);
  }
  memcpy(vec->p + (vec->size * vec->itemSize), item, vec->itemSize);
  vec->size += 1;
}

void vec_free(vec *v) {
  if (v) {
    free(v->p);
    free(v);
  }
}

// File loading
static char *loadFile(const char *path) {
  FILE *p = fopen(path, "r");
  if (!p) {
    fprintf(stderr, "Error: Could not open file %s\n", path);
    return NULL;
  }

  // Get file size
  fseek(p, 0, SEEK_END);
  long fileSize = ftell(p);
  fseek(p, 0, SEEK_SET);

  char *buffer = (char *)malloc(fileSize + 1);
  size_t bytesRead = fread(buffer, 1, fileSize, p);
  buffer[bytesRead] = '\0';
  fclose(p);
  return buffer;
}

// CSV parsing helpers
static char *readColumn(int *start, char *fileData) {
  // Increased buffer size to handle large geoshape data (Indonesia has ~400KB!)
  char *b = malloc(500000);  // Increased to handle Indonesia's massive geoshape
  int idx = 0;
  int inQuotes = 0;

  while (fileData[*start] != '\0') {
    char c = fileData[*start];

    if (c == '"') {
      inQuotes = !inQuotes;  // Toggle quote state
    }

    // Stop at field delimiter (;) or newline, but only if not inside quotes
    if (!inQuotes && (c == ';' || c == '\n')) {
      break;
    }

    b[idx++] = fileData[(*start)++];
  }

  b[idx] = '\0';
  (*start)++;  // Skip the delimiter
  return b;
}

static int isNum(char c) {
  // Only digits start numbers reliably; '-' and '.' alone can cause issues
  return ('0' <= c && c <= '9') || c == '-';
}

static Polygon *parsePolygon(char *shape, char **endptr) {
  // Use heap allocation for large arrays to avoid stack overflow
  // Indonesia has extremely detailed coastlines requiring huge buffer
  double *arr = malloc(sizeof(double) * 50000000);
  int cnt = 0;

  while (*shape != '\0' && cnt < 50000000) {
    if (isNum(shape[0])) {
      char *localEndptr;
      double value = strtod(shape, &localEndptr);
      if (localEndptr == shape) {
        // strtod failed to parse - skip this character
        // This can happen with bare '-' or '.' not part of a number
        shape++;
      } else {
        arr[cnt++] = value;
        shape = localEndptr;
      }
    } else if (shape[0] == ']' && shape[1] == ']' && shape[2] == ']') {
      if (shape[3] != ']') {
        // Found ]]] but not ]]]] - this is the polygon end
        shape += 3;
        break;
      } else {
        // This is ]]]], skip one ] and continue
        shape++;
      }
    } else {
      // Skip non-numeric characters like brackets, commas, whitespace
      shape++;
    }
  }

  if (cnt >= 50000000) {
    printf("WARNING: Polygon truncated - hit 50M number limit!\n");
    // Scan forward to find the polygon boundary ]]] to continue parsing correctly
    while (*shape != '\0') {
      if (shape[0] == ']' && shape[1] == ']' && shape[2] == ']' && shape[3] != ']') {
        shape += 3;
        break;
      }
      shape++;
    }
  }

  *endptr = shape;

  Polygon *poly = malloc(sizeof(Polygon));
  poly->points = vec_init(sizeof(GeoPoint), cnt / 2 + 1);

  // Parse as [lon, lat] pairs
  for (int i = 0; i < cnt; i += 2) {
    GeoPoint p;
    p.lon = arr[i];      // First value is longitude
    p.lat = arr[i + 1];  // Second value is latitude
    vec_append(poly->points, &p);
  }

  free(arr);  // Free the temporary array
  return poly;
}

static void parseGeoShape(char *shape, CountryData *d) {
  const char *prefix = "\"{\"\"coordinates\"\": ";
  int len = strlen(prefix);
  shape += len;

  d->polygons = vec_init(sizeof(Polygon *), 100);

  while (*shape != '\0') {
    // Look for the start of a polygon: [[[ (but not [[[[)
    while (*shape != '\0') {
      if (*shape == '"') {
        // Reached end of geoshape data
        return;
      }

      if (*shape == '[' && shape[1] == '[' && shape[2] == '[') {
        if (shape[3] == '[') {
          // This is [[[[, skip all 4 brackets
          shape += 4;
          continue;
        } else {
          // This is [[[, found a polygon start
          break;
        }
      }

      shape++;
    }

    if (*shape == '\0') break;

    char *endptr;
    Polygon *p = parsePolygon(shape, &endptr);
    if (p->points->size > 0) {
      vec_append(d->polygons, &p);
    } else {
      vec_free(p->points);
      free(p);
    }
    shape = endptr;

    // Check if we've reached the end of the shape data
    if (*shape == '"' || *shape == '\0') {
      break;
    }
  }
}

// Parse centroid from the Geo Point column (format: "lat, lon")
void calculateCentroid(CountryData *country) {
  // Default to 0,0 if parsing fails
  country->centroid.lat = 0.0;
  country->centroid.lon = 0.0;

  if (!country->geoPoint || strlen(country->geoPoint) == 0) {
    return;
  }

  // Parse "lat, lon" format
  char *comma = strchr(country->geoPoint, ',');
  if (comma) {
    char *endptr;
    country->centroid.lat = strtod(country->geoPoint, &endptr);
    country->centroid.lon = strtod(comma + 1, &endptr);
  }
}

// Load country database from CSV
CountryDatabase *loadCountryDatabase(const char *csv_path) {
  char *fileData = loadFile(csv_path);
  if (!fileData) {
    return NULL;
  }

  CountryDatabase *db = malloc(sizeof(CountryDatabase));
  db->countries = malloc(sizeof(CountryData) * 300); // Pre-allocate for ~250 countries
  db->count = 0;

  int i = 0;
  int rowNum = 0;

  while (fileData[i] != '\0') {
    CountryData d = {
      .geoPoint = readColumn(&i, fileData),
      .geoShape = readColumn(&i, fileData),
      .territoryCode = readColumn(&i, fileData),
      .status = readColumn(&i, fileData),
      .countryCode = readColumn(&i, fileData),
      .englishName = readColumn(&i, fileData),
      .continent = readColumn(&i, fileData),
      .region = readColumn(&i, fileData),
      .alpha2 = readColumn(&i, fileData),
      .poly_count = 0
    };

    // Skip French Name column (last column in CSV)
    char *frenchName = readColumn(&i, fileData);
    free(frenchName);

    rowNum++;

    // Skip header row and empty entries
    if (rowNum == 1 || strlen(d.englishName) == 0 ||
        strcmp(d.englishName, "English Name") == 0 ||
        strcmp(d.englishName, "\"\"") == 0) {
      free(d.geoPoint);
      free(d.geoShape);
      free(d.territoryCode);
      free(d.status);
      free(d.countryCode);
      free(d.englishName);
      free(d.continent);
      free(d.region);
      free(d.alpha2);
      continue;
    }

    // Parse geographic data
    parseGeoShape(d.geoShape, &d);
    calculateCentroid(&d);

    // Validate centroid
    if (d.centroid.lat == 0.0 && d.centroid.lon == 0.0) {
      printf("Warning: Invalid centroid for %s\n", d.englishName);
    }

    db->countries[db->count++] = d;

    if (db->count % 50 == 0) {
      printf("Loaded %llu countries...\n", db->count);
    }
  }

  free(fileData);
  printf("Total countries loaded: %llu\n", db->count);
  return db;
}

// Find country by name (case-insensitive)
CountryData *getCountryByName(CountryDatabase *db, const char *name) {
  for (uint64_t i = 0; i < db->count; i++) {
    if (strcasecmp(db->countries[i].englishName, name) == 0) {
      return &db->countries[i];
    }
  }
  return NULL;
}

// Free country database
void freeCountryDatabase(CountryDatabase *db) {
  if (!db) return;

  for (uint64_t i = 0; i < db->count; i++) {
    CountryData *c = &db->countries[i];
    free(c->geoPoint);
    free(c->geoShape);
    free(c->territoryCode);
    free(c->status);
    free(c->countryCode);
    free(c->englishName);
    free(c->continent);
    free(c->region);
    free(c->alpha2);

    if (c->polygons) {
      for (uint64_t j = 0; j < c->polygons->size; j++) {
        Polygon **polyPtr = (Polygon **)c->polygons->p + j;
        Polygon *poly = *polyPtr;
        vec_free(poly->points);
        free(poly);
      }
      vec_free(c->polygons);
    }
  }

  free(db->countries);
  free(db);
}
