#ifndef HOTSPOT_DATABASE_H
#define HOTSPOT_DATABASE_H

#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_SIZE 512
#define MAX_HOTSPOTS 12
#define SSID_MAX_LEN 32

struct HotspotEntry {
  char ssid[SSID_MAX_LEN];
  float x, y;
  float rssiAt1m;       // RSSI at 1 meter
  float pathLossExponent; // Path loss exponent
};

class HotspotDatabase {
public:
  void begin();  // Initialize EEPROM
  void save(const String& ssid, float x, float y, float rssiAt1m, float pathLossExponent);
  bool load(const String& ssid, float& x, float& y, float& rssiAt1m, float& pathLossExponent);
  void listAll();
  void clear();
};

#endif