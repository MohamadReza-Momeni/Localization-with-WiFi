#ifndef LOCATION_FINDER_H
#define LOCATION_FINDER_H

#include <Arduino.h>
#include <vector>

// Your WiFiHotspot struct
struct WiFiHotspot {
  String ssid;
  int32_t rssi;
  float x, y;
  float rssiAt1m;       // RSSI at 1 meter for this hotspot
  float pathLossExponent; // Path loss exponent for this hotspot
};

class LocationFinder {
private:
  const int MIN_HOTSPOTS = 2;
  std::vector<WiFiHotspot> hotspots;
  bool initialized; // Kalman filter initialization flag
  float state[2]; // Kalman filter state [x, y]
  float covariance[2][2]; // Kalman filter covariance matrix

  float rssiToDistance(const WiFiHotspot& hotspot);
  void kalmanFilter(float measurementX, float measurementY);

public:
  LocationFinder(const std::vector<WiFiHotspot>& selectedHotspots);
  bool findLocation(float& x, float& y);
};

#endif