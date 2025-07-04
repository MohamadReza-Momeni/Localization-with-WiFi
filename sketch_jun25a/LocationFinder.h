#ifndef LOCATION_FINDER_H
#define LOCATION_FINDER_H

#include <Arduino.h>
#include <vector>

// Your WiFiHotspot struct
struct WiFiHotspot {
  String ssid;
  int32_t rssi;
  float x, y;
};

class LocationFinder {
private:
  const float PATH_LOSS_EXPONENT = 2.5;
  const float RSSI_AT_1M = -45.0;
  const int MIN_HOTSPOTS = 2;
  std::vector<WiFiHotspot> hotspots;

  float rssiToDistance(int32_t rssi);

public:
  LocationFinder(const std::vector<WiFiHotspot>& selectedHotspots);
  bool findLocation(float& x, float& y);
};

#endif
