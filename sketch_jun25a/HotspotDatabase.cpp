#include "HotspotDatabase.h"
#include <math.h>

void HotspotDatabase::begin() {
  EEPROM.begin(EEPROM_SIZE);
}

void HotspotDatabase::save(const String& ssid, float x, float y, float rssiAt1m, float pathLossExponent) {
  if (ssid.length() >= SSID_MAX_LEN) {
    Serial.println("SSID too long, skipping save.");
    return;
  }

  // Check if SSID already exists
  for (int i = 0; i < MAX_HOTSPOTS; i++) {
    int addr = i * sizeof(HotspotEntry);
    HotspotEntry entry;
    EEPROM.get(addr, entry);
    if (String(entry.ssid) == ssid) {
      // Update existing entry
      strncpy(entry.ssid, ssid.c_str(), SSID_MAX_LEN);
      entry.x = x;
      entry.y = y;
      entry.rssiAt1m = rssiAt1m;
      entry.pathLossExponent = pathLossExponent;
      EEPROM.put(addr, entry);
      EEPROM.commit();
      Serial.println("Updated existing hotspot in EEPROM.");
      return;
    }
  }

  // Find an empty slot
  for (int i = 0; i < MAX_HOTSPOTS; i++) {
    int addr = i * sizeof(HotspotEntry);
    HotspotEntry entry;
    EEPROM.get(addr, entry);
    if (String(entry.ssid).length() == 0) {
      strncpy(entry.ssid, ssid.c_str(), SSID_MAX_LEN);
      entry.x = x;
      entry.y = y;
      entry.rssiAt1m = rssiAt1m;
      entry.pathLossExponent = pathLossExponent;
      EEPROM.put(addr, entry);
      EEPROM.commit();
      Serial.println("Saved new hotspot to EEPROM.");
      return;
    }
  }

  Serial.println("EEPROM is full. Could not save hotspot.");
}

bool HotspotDatabase::load(const String& ssid, float& x, float& y, float& rssiAt1m, float& pathLossExponent) {
  for (int i = 0; i < MAX_HOTSPOTS; i++) {
    int addr = i * sizeof(HotspotEntry);
    HotspotEntry entry;
    EEPROM.get(addr, entry);
    if (String(entry.ssid) == ssid) {
      x = entry.x;
      y = entry.y;
      rssiAt1m = entry.rssiAt1m;
      pathLossExponent = entry.pathLossExponent;
      return true;
    }
  }
  return false;
}

void HotspotDatabase::listAll() {
  Serial.println("EEPROM Hotspots:");
  for (int i = 0; i < MAX_HOTSPOTS; i++) {
    int addr = i * sizeof(HotspotEntry);
    HotspotEntry entry;
    EEPROM.get(addr, entry);
    // Check if SSID is valid and coordinates are not nan
    bool validSSID = true;
    for (size_t j = 0; j < SSID_MAX_LEN && entry.ssid[j] != '\0'; j++) {
      if (!isPrintable(entry.ssid[j])) {
        validSSID = false;
        break;
      }
    }
    if (validSSID && String(entry.ssid).length() > 0 && !isnan(entry.x) && !isnan(entry.y)) {
      Serial.print("SSID: ");
      Serial.print(entry.ssid);
      Serial.print(", x: ");
      Serial.print(entry.x);
      Serial.print(", y: ");
      Serial.print(entry.y);
      Serial.print(", RSSI@1m: ");
      Serial.print(entry.rssiAt1m);
      Serial.print(", PathLoss: ");
      Serial.println(entry.pathLossExponent);
    }
  }
}

void HotspotDatabase::clear() {
  for (int i = 0; i < MAX_HOTSPOTS; i++) {
    int addr = i * sizeof(HotspotEntry);
    HotspotEntry entry;
    memset(&entry, 0, sizeof(HotspotEntry)); // Clear entry to zeros
    EEPROM.put(addr, entry);
  }
  EEPROM.commit();
  Serial.println("All hotspots cleared from EEPROM.");
}