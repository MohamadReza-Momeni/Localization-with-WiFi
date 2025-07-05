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
};

class HotspotDatabase {
public:
  void begin();  // Initialize EEPROM
  void save(const String& ssid, float x, float y);
  bool load(const String& ssid, float& x, float& y);
  void listAll();
  void clear();
};

#endif
