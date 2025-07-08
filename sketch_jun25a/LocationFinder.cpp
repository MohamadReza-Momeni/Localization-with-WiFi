#include "WString.h"
#include "LocationFinder.h"
#include <math.h>  // for pow()

LocationFinder::LocationFinder(const std::vector<WiFiHotspot>& selectedHotspots)
  : hotspots(selectedHotspots) {}

float LocationFinder::rssiToDistance(const WiFiHotspot& hotspot) {
  // Use per-hotspot RSSI at 1m and path loss exponent
  return pow(10.0, (hotspot.rssiAt1m - hotspot.rssi) / (10.0 * hotspot.pathLossExponent));
}

bool LocationFinder::findLocation(float& x, float& y) {
  if (hotspots.size() < MIN_HOTSPOTS) {
    Serial.println("Not enough hotspots selected. At least " + String(MIN_HOTSPOTS) + " required.");
    return false;
  }

  // Threshold for rejecting weak signals
  const int32_t RSSI_THRESHOLD = -90; // Ignore hotspots with RSSI below -90 dBm

  float A = 0, B = 0, C = 0, D = 0, E = 0, F = 0;
  float totalWeight = 0;

  // Count valid hotspots after filtering
  size_t validHotspots = 0;
  for (size_t i = 0; i < hotspots.size(); i++) {
    if (hotspots[i].rssi > RSSI_THRESHOLD) {
      validHotspots++;
    }
  }

  if (validHotspots < MIN_HOTSPOTS) {
    Serial.println("Not enough valid hotspots with RSSI > " + String(RSSI_THRESHOLD) + " dBm.");
    return false;
  }

  for (size_t i = 0; i < hotspots.size(); i++) {
    if (hotspots[i].rssi <= RSSI_THRESHOLD) {
      Serial.println("Skipping hotspot " + hotspots[i].ssid + " (RSSI: " + String(hotspots[i].rssi) + " dBm) due to weak signal.");
      continue;
    }

    float xi = hotspots[i].x;
    float yi = hotspots[i].y;
    float di = rssiToDistance(hotspots[i]);

    // Weight is inversely proportional to distance
    float weight = 1.0 / (di * di); // Using inverse square of distance
    totalWeight += weight;

    Serial.print("Distance from ");
    Serial.print(hotspots[i].ssid);
    Serial.println(" is " + String(di) + " meters (Weight: " + String(weight) + ", RSSI@1m: " + String(hotspots[i].rssiAt1m) + ", PathLoss: " + String(hotspots[i].pathLossExponent) + ").");

    A += 2 * xi * weight;
    B += 2 * yi * weight;
    C += weight;
    D += (xi * xi + yi * yi - di * di) * weight;
    E += xi * weight;
    F += yi * weight;
  }

  if (totalWeight == 0) {
    Serial.println("No valid weights for calculation.");
    return false;
  }

  // Normalize by total weight
  A /= totalWeight;
  B /= totalWeight;
  C /= totalWeight;
  D /= totalWeight;
  E /= totalWeight;
  F /= totalWeight;

  // Check for numerical stability
  float denominator = C * C - 1;
  if (fabs(denominator) < 1e-6) { // Avoid division by near-zero
    Serial.println("Numerical instability detected in trilateration.");
    return false;
  }

  x = (A * D - E * C) / denominator;
  y = (B * D - F * C) / denominator;

  // Validate results
  if (isnan(x) || isnan(y) || isinf(x) || isinf(y)) {
    Serial.println("Invalid location calculated (NaN or Inf).");
    return false;
  }

  return true;
}