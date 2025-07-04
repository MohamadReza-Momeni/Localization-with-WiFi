#include "LocationFinder.h"
#include <math.h>  // for pow()

LocationFinder::LocationFinder(const std::vector<WiFiHotspot>& selectedHotspots)
  : hotspots(selectedHotspots) {}

float LocationFinder::rssiToDistance(int32_t rssi) {
  return pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
}

bool LocationFinder::findLocation(float& x, float& y) {
  if (hotspots.size() < MIN_HOTSPOTS) {
    Serial.println("Not Enough Hotspots where selected");
    return false;
  }

  float A = 0, B = 0, C = 0, D = 0, E = 0, F = 0;
  for (size_t i = 0; i < hotspots.size(); i++) {
    float xi = hotspots[i].x;
    float yi = hotspots[i].y;
    float di = rssiToDistance(hotspots[i].rssi);

    A += 2 * xi;
    B += 2 * yi;
    C += 1;
    D += xi * xi + yi * yi - di * di;
    E += xi;
    F += yi;
  }

  size_t n = hotspots.size();
  A /= n;
  B /= n;
  C /= n;
  D /= n;
  E /= n;
  F /= n;

  float denominator = C * C - 1;
  x = (A * D - E * C) / denominator;
  y = (B * D - F * C) / denominator;

  return true;
}
