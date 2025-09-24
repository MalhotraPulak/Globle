#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u_int64_t COORDINATES_FILE_SIZE = 9 * 1000 * 1000 * 1000ll;

typedef struct {
  uint64_t size;
  uint64_t itemSize;
  uint64_t capacity;
  void *p;
} vec;

vec *vec_init(uint64_t item_size, uint64_t capacity) {
  vec *v = calloc(sizeof(vec), 1);
  v->size = 0;
  v->itemSize = item_size;
  v->capacity = capacity;
  v->p = calloc(capacity, item_size);
  return v;
}

void vec_append(vec *vec, void *item) {
  // add bounds check
  memcpy(vec->p + vec->size, item, vec->itemSize);
  vec->size += 1;
}

typedef struct {
  float x;
  float y;
} point;

typedef struct {
  vec *points;
} polygon;

typedef struct {
  char *geoPoint;
  char *geoShape;
  char *territoryCode;
  char *status;
  char *countryCode;
  char *englishName;
  char *continent;
  char *region;
  char *alpha2;
  uint64_t poly_count;
  vec *polygons;
} countryData;

char *loadFile(const char *path) {
  FILE *p = fopen(path, "r");
  char *buffer = (char *)malloc(COORDINATES_FILE_SIZE);
  fread(buffer, COORDINATES_FILE_SIZE, 1, p);
  return buffer;
}

char *readColumn(int *start, char *fileData) {
  char *b = malloc(2000);
  int idx = 0;
  while (fileData[*start] != ';' && fileData[*start] != '\0') {
    b[idx++] = fileData[(*start)++];
  }
  b[idx++] = '\0';
  (*start)++;
  return b;
}

int isNum(char c) {
  if (('0' <= c && c <= '9') || c == '-') {
    return 1;
  }
  return 0;
}

polygon *parsePolygon(char *shape, char **endptr) {
  double arr[1000];
  int cnt = 0;
  while (*shape != '\0') {
    if (isNum(shape[0])) {
      char *endptr;
      double value = strtod(shape, &endptr);
      arr[cnt++] = value;
      shape = endptr;
    } else if (shape[0] == ']' && shape[1] == ']' && shape[2] == ']') {
      shape += 3;
      // polygon ended here
      break;
    }
    shape++;
  }
  *endptr = shape;

  polygon *poly = malloc(sizeof(polygon));
  poly->points = vec_init(sizeof(point), cnt);
  for (int i = 0; i < cnt; i += 2) {
    point p;
    p.x = arr[i];
    p.y = arr[i + 1];
    vec_append(poly->points, &p);
  }
  return poly;
}

void printShapeIndent(char *shape, int target_depth, countryData *d) {
  int depth = 0;
  int i = 0;

  d->polygons = vec_init(sizeof(polygon), 100);
  while (*shape != '\0') {
    char *endptr;
    polygon *p = parsePolygon(shape, &endptr);
    shape = endptr;
    vec_append(d->polygons, p);
  }
}

void parseGeoShape(char *shape, countryData *d) {
  const char *prefix = "\"{\"\"coordinates\"\": ";
  int len = strlen(prefix);
  shape += len;
  if (shape[3] == '[') {
    printf("Multipolygon\n");
    printShapeIndent(shape, 4, d);
  } else {
    printf("Polygon\n");
    printShapeIndent(shape, 3, d);
  }
}

void parseCSV(char *fileData) {
  int idx = 0;
  int i = 0;
  for (int itr = 0; fileData[i] != '\0'; itr++) {
    countryData d = {.geoPoint = readColumn(&i, fileData),
                     .geoShape = readColumn(&i, fileData),
                     .territoryCode = readColumn(&i, fileData),
                     .status = readColumn(&i, fileData),
                     .countryCode = readColumn(&i, fileData),
                     .englishName = readColumn(&i, fileData),
                     .continent = readColumn(&i, fileData),
                     .region = readColumn(&i, fileData),
                     .alpha2 = readColumn(&i, fileData),
                     .poly_count = 0};
    printf("%d %s\n", itr, d.countryCode);
    parseGeoShape(d.geoShape, &d);
    printf("Poly count %llu \n", d.polygons->size);
  }
}

int main() {
  char *fileData = loadFile("./coordinates/ccc.csv");
  parseCSV(fileData);
  // printf("%s", fileData);
}
