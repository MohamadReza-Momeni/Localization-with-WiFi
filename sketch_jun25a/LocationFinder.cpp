#include "WString.h"
#include "LocationFinder.h"
#include <math.h>  // for pow()

LocationFinder::LocationFinder(const std::vector<WiFiHotspot>& selectedHotspots)
  : hotspots(selectedHotspots), initialized(false) {
  // Initialize Kalman filter state
  state[0] = 0.0; // x
  state[1] = 0.0; // y
  covariance[0][0] = 1000.0; // Large initial uncertainty for x
  covariance[0][1] = 0.0;
  covariance[1][0] = 0.0;
  covariance[1][1] = 1000.0; // Large initial uncertainty for y
}

float LocationFinder::rssiToDistance(const WiFiHotspot& hotspot) {
  // Use per-hotspot RSSI at 1m and path loss exponent
  return pow(10.0, (hotspot.rssiAt1m - hotspot.rssi) / (10.0 * hotspot.pathLossExponent));
}

void LocationFinder::kalmanFilter(float measurementX, float measurementY) {
  // Kalman filter parameters
  const float processNoise = 0.01; // Process noise (Q)
  const float measurementNoise = 1.0; // Measurement noise (R)
  const float dt = 0.5; // Time step (assuming 0.5s from scan interval in sketch)

  // Prediction step
  // State transition: assume static position (x, y remain same)
  // state = state (no motion model for simplicity)
  // Update covariance: P = P + Q
  covariance[0][0] += processNoise;
  covariance[1][1] += processNoise;

  // Update step
  // Measurement vector: [measurementX, measurementY]
  float z[2] = {measurementX, measurementY};

  // Kalman gain: K = P / (P + R)
  float kx = covariance[0][0] / (covariance[0][0] + measurementNoise);
  float ky = covariance[1][1] / (covariance[1][1] + measurementNoise);

  // Update state: state = state + K * (z - state)
  state[0] += kx * (z[0] - state[0]);
  state[1] += ky * (z[1] - state[1]);

  // Update covariance: P = (I - K) * P
  covariance[0][0] *= (1.0 - kx);
  covariance[1][1] *= (1.0 - ky);
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
  if (fabs(denominator) < 1e-6) {
    Serial.println("Numerical instability detected in trilateration.");
    return false;
  }

  float rawX = (A * D - E * C) / denominator;
  float rawY = (B * D - F * C) / denominator;

  // Validate raw results
  if (isnan(rawX) || isnan(rawY) || isinf(rawX) || isinf(rawY)) {
    Serial.println("Invalid location calculated (NaN or Inf).");
    return false;
  }

  // Apply Kalman filter
  if (!initialized) {
    state[0] = rawX;
    state[1] = rawY;
    initialized = true;
  } else {
    kalmanFilter(rawX, rawY);
  }

  x = state[0];
  y = state[1];

  return true;
}