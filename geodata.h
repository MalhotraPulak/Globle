#ifndef GEODATA_H
#define GEODATA_H

#include <stdint.h>

// Point in 2D space (latitude, longitude)
typedef struct {
  float lat;  // Latitude
  float lon;  // Longitude
} GeoPoint;

// Generic dynamic array
typedef struct {
  uint64_t size;
  uint64_t itemSize;
  uint64_t capacity;
  void *p;
} vec;

// Polygon made of geographic points
typedef struct {
  vec *points;  // vec of GeoPoint
} Polygon;

// Country data with metadata and geographic boundaries
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
  vec *polygons;       // vec of Polygon*
  GeoPoint centroid;   // Center point of country
} CountryData;

// Global country database
typedef struct {
  CountryData *countries;
  uint64_t count;
} CountryDatabase;

// Vector functions
vec *vec_init(uint64_t item_size, uint64_t capacity);
void vec_append(vec *vec, void *item);
void vec_free(vec *v);

// Country data functions
CountryDatabase *loadCountryDatabase(const char *csv_path);
void freeCountryDatabase(CountryDatabase *db);
CountryData *getCountryByName(CountryDatabase *db, const char *name);
void calculateCentroid(CountryData *country);

#endif // GEODATA_H
