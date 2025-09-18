#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include <stdio.h>

u_int64_t COORDINATES_FILE_SIZE = 9 * 1000 * 1000 * 1000ll;

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

void parseCSV(char *fileData) {
  int idx = 0;
  int i = 0;
  for (int itr = 0; fileData[i] != '\0'; itr++) {
    countryData d = {
        .geoPoint = readColumn(&i, fileData),
        .geoShape = readColumn(&i, fileData),
        .territoryCode = readColumn(&i, fileData),
        .status = readColumn(&i, fileData),
        .countryCode = readColumn(&i, fileData),
        .englishName = readColumn(&i, fileData),
        .continent = readColumn(&i, fileData),
        .region = readColumn(&i, fileData),
        .alpha2 = readColumn(&i, fileData),
    };
    printf("%d %s\n", itr, d.countryCode);
    printf("%d %s\n", itr, d.geoShape);
  }
}

int main() {
  char *fileData = loadFile("./coordinates/ccc.csv");
  parseCSV(fileData);
  // printf("%s", fileData);
}
