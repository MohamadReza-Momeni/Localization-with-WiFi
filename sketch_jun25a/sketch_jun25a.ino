#include <WiFi.h>

#include "LocationFinder.h"
#include "HotspotDatabase.h"

// Array to store selected hotspots
std::vector<WiFiHotspot> selectedHotspots;
HotspotDatabase hotspotDB;

// Program modes
enum ProgramMode {
  IDLE,
  FIND_LOCATION,
  SELECT_HOTSPOTS,
  BASE_STATION,
  CHANGE_LOCATION,
  VIEW_DATABASE
};

enum LocalizationTechnique {
  WIFI,
  WiFi_BaseStation,
  WiFi_LoRa,
  WiFi_LoRa_BaseStation
};

ProgramMode currentMode = IDLE;
ProgramMode previousMode = IDLE;
LocalizationTechnique localizationTechnique = WIFI;

// Function prototypes
void displayMenu();
void scanAndDisplayNetworks();
void selectHotspots();
bool compareByRSSI(WiFiHotspot a, WiFiHotspot b);
void findLocation();
void useBaseStation();
void changeHotspotLocation();
void viewDatabase();
String printLocalizationTechnique(LocalizationTechnique technique);
void updateSelectedHotspotsRSSI();

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("WiFi Localization System Started");

  // Initialize EEPROM
  hotspotDB.begin();

  // Set WiFi to station mode and disconnect from any network
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  displayMenu();
}

void loop() {
  if (currentMode == IDLE) {
    if (previousMode != IDLE) {
      previousMode = IDLE;
      displayMenu();
    }

    // Wait for user input with timeout
    unsigned long startTime = millis();
    while (!Serial.available() && millis() - startTime < 10000) {
      delay(100);
    }

    if (!Serial.available()) {
      return;
    }

    String input = Serial.readStringUntil('\n');
    input.trim();
    int choice = input.toInt();

    switch (choice) {
      case 1:
        currentMode = FIND_LOCATION;
        previousMode = IDLE;
        findLocation();
        break;
      case 2:
        currentMode = SELECT_HOTSPOTS;
        previousMode = IDLE;
        selectHotspots();
        break;
      case 3:
        currentMode = BASE_STATION;
        previousMode = IDLE;
        useBaseStation();
        break;
      case 4:
        currentMode = CHANGE_LOCATION;
        previousMode = IDLE;
        changeHotspotLocation();
        break;
      case 5:
        currentMode = VIEW_DATABASE;
        previousMode = IDLE;
        viewDatabase();
        break;
      default:
        Serial.println("Invalid option, please choose 1-5.");
        break;
    }
  } else if (currentMode == SELECT_HOTSPOTS) {
    selectHotspots();
  } else if (currentMode == FIND_LOCATION){
    findLocation();
  }

  updateSelectedHotspotsRSSI();
}

void displayMenu() {
  Serial.println("\n=== WiFi Localization System Menu ===");
  Serial.println("Current Technique: " + printLocalizationTechnique(localizationTechnique));

  // Display selected hotspots
  Serial.println("\nSelected Hotspots:");
  if (!selectedHotspots.empty()) {
    for (size_t i = 0; i < selectedHotspots.size(); i++) {
      Serial.println(String(i + 1) + ": " + selectedHotspots[i].ssid + " (RSSI: " + selectedHotspots[i].rssi + " dBm, Pos: (" + selectedHotspots[i].x + ", " + selectedHotspots[i].y + "), RSSI@1m: " + selectedHotspots[i].rssiAt1m + ", PathLoss: " + selectedHotspots[i].pathLossExponent + ")");
    }
  } else {
    Serial.println("No hotspots selected yet.");
  }

  Serial.println("\nOptions:");
  Serial.println("1. Find Location");
  Serial.println("2. Select WiFi Hotspots");
  Serial.println("3. Use Base Station ESP32");
  Serial.println("4. Change Hotspot Location");
  Serial.println("5. View Database");
  Serial.println("Enter your choice (1-5):");
}

void scanAndDisplayNetworks() {
  Serial.println("\nScanning for WiFi networks...");

  // Scan for networks
  int16_t n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("No networks found");
    return;
  }

  // Create vector to store found networks
  std::vector<WiFiHotspot> networks;

  // Store found networks
  for (int i = 0; i < n; i++) {
    WiFiHotspot hotspot;
    hotspot.ssid = WiFi.SSID(i);
    hotspot.rssi = WiFi.RSSI(i);
    hotspot.x = 0.0;
    hotspot.y = 0.0;
    hotspot.rssiAt1m = -45.0; // 
    hotspot.pathLossExponent = 2.5; // Default value
    networks.push_back(hotspot);
  }

  // Sort networks by RSSI (descending order)
  std::sort(networks.begin(), networks.end(), compareByRSSI);

  // Display sorted networks
  Serial.println("Networks found (sorted by RSSI):");
  for (size_t i = 0; i < networks.size(); i++) {
    Serial.println(String(i + 1) + ": " + networks[i].ssid + " (RSSI: " + networks[i].rssi + " dBm)");
  }

  // Free scan results
  WiFi.scanDelete();
}

// Function to compare hotspots by RSSI
bool compareByRSSI(WiFiHotspot a, WiFiHotspot b) {
  return a.rssi > b.rssi;  // Sort in descending order
}

void selectHotspots() {
  static unsigned long lastScanTime = 0;
  const unsigned long scanInterval = 5000;  // 5 seconds

  // Perform scan and display if it's time
  if (millis() - lastScanTime >= scanInterval) {
    scanAndDisplayNetworks();
    lastScanTime = millis();
  }

  Serial.println("\nEnter the numbers of hotspots to select (e.g., '1 3 5'), '0' to skip, or 'q' to return to menu:");

  // Wait for user input with timeout
  unsigned long startTime = millis();
  while (!Serial.available() && millis() - startTime < scanInterval) {
    delay(100);
  }

  if (!Serial.available()) {
    return;  // Continue scanning every 5 seconds
  }

  String input = Serial.readStringUntil('\n');
  input.trim();

  if (input == "q") {
    Serial.println("Returning to menu.");
    currentMode = IDLE;
    previousMode = SELECT_HOTSPOTS;
    return;
  }

  if (input == "0") {
    Serial.println("Selection skipped.");
    return;  // Continue scanning
  }

  // Parse input for hotspot numbers
  std::vector<int> selections;
  int start = 0;
  int space = input.indexOf(' ');

  while (space != -1) {
    int num = input.substring(start, space).toInt();
    if (num > 0) selections.push_back(num);
    start = space + 1;
    space = input.indexOf(' ', start);
  }
  int num = input.substring(start).toInt();
  if (num > 0) selections.push_back(num);

  // Add selected hotspots to the list
  int n = WiFi.scanNetworks(false, false);  // Re-scan to get fresh data
  std::vector<WiFiHotspot> networks;

  for (int i = 0; i < n; i++) {
    WiFiHotspot hotspot;
    hotspot.ssid = WiFi.SSID(i);
    hotspot.rssi = WiFi.RSSI(i);
    hotspot.x = 0.0;
    hotspot.y = 0.0;
    hotspot.rssiAt1m = -45.0; // Default value
    hotspot.pathLossExponent = 2.5; // Default value
    networks.push_back(hotspot);
  }
  std::sort(networks.begin(), networks.end(), compareByRSSI);

  for (int sel : selections) {
    if (sel > 0 && sel <= (int)networks.size()) {
      bool alreadySelected = false;
      for (const auto& hotspot : selectedHotspots) {
        if (hotspot.ssid == networks[sel - 1].ssid) {
          alreadySelected = true;
          break;
        }
      }
      if (!alreadySelected) {
        WiFiHotspot newHotspot = networks[sel - 1];
        // Check if hotspot exists in EEPROM
        float x, y, rssiAt1m, pathLossExponent;
        if (hotspotDB.load(newHotspot.ssid, x, y, rssiAt1m, pathLossExponent)) {
          newHotspot.x = x;
          newHotspot.y = y;
          newHotspot.rssiAt1m = rssiAt1m;
          newHotspot.pathLossExponent = pathLossExponent;
          Serial.println("Loaded position from database: (" + String(x) + ", " + String(y) + "), RSSI@1m: " + String(rssiAt1m) + ", PathLoss: " + String(pathLossExponent));

        } else {
          // Prompt for position and parameters
          Serial.println("Enter x coordinate for " + newHotspot.ssid + ":");
          while (!Serial.available()) {
            delay(100);
          }
          newHotspot.x = Serial.readStringUntil('\n').toFloat();

          Serial.println("Enter y coordinate for " + newHotspot.ssid + ":");
          while (!Serial.available()) {
            delay(100);
          }
          newHotspot.y = Serial.readStringUntil('\n').toFloat();

          Serial.println("Enter RSSI at 1 meter for " + newHotspot.ssid + " (e.g., -45.0):");
          while (!Serial.available()) {
            delay(100);
          }
          newHotspot.rssiAt1m = Serial.readStringUntil('\n').toFloat();

          Serial.println("Enter path loss exponent for " + newHotspot.ssid + " (e.g., 2.5):");
          while (!Serial.available()) {
            delay(100);
          }
          newHotspot.pathLossExponent = Serial.readStringUntil('\n').toFloat();

          // Save to EEPROM
          hotspotDB.save(newHotspot.ssid, newHotspot.x, newHotspot.y, newHotspot.rssiAt1m, newHotspot.pathLossExponent);
          Serial.println("Saved position to database: (" + String(newHotspot.x) + ", " + String(newHotspot.y) + "), RSSI@1m: " + String(newHotspot.rssiAt1m) + ", PathLoss: " + String(newHotspot.pathLossExponent));
        }
        selectedHotspots.push_back(newHotspot);
        Serial.println("Selected: " + newHotspot.ssid + " (RSSI: " + String(newHotspot.rssi) + " dBm, Pos: (" + String(newHotspot.x) + ", " + String(newHotspot.y) + "), RSSI@1m: " + String(newHotspot.rssiAt1m) + ", PathLoss: " + String(newHotspot.pathLossExponent) + ")");

      } else {
        Serial.println("Hotspot " + networks[sel - 1].ssid + " already selected.");
      }
    } else {
      Serial.println("Invalid selection: " + String(sel));
    }
  }

  WiFi.scanDelete();
}

void findLocation() {
  static unsigned long lastScanTime = 0;
  const unsigned long scanInterval = 500;  // 0.5 seconds

  // Perform scan and localization if it's time
  if (millis() - lastScanTime >= scanInterval) {
    if (selectedHotspots.size() < 3) {
      Serial.println("\nError: At least 3 hotspots are required for localization.");
      Serial.println("Please select more hotspots using option 2.");
      lastScanTime = millis();
      currentMode = IDLE;
      previousMode = FIND_LOCATION;
      return;
    }

    // Perform localization
    Serial.println("\nFinding location using trilateration...");
    LocationFinder locator(selectedHotspots);
    float x, y;
    if (locator.findLocation(x, y)) {
      Serial.println("Estimated ESP32 location: (" + String(x) + ", " + String(y) + ")");
    } else {
      Serial.println("Failed to estimate location. Ensure hotspots have valid positions and RSSI values.");
    }
    lastScanTime = millis();
  }

  // Check for user input to exit
  unsigned long startTime = millis();
  while (!Serial.available() && millis() - startTime < scanInterval) {
    delay(100);
  }

  if (!Serial.available()) {
    return;
  }

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "q") {
      Serial.println("Returning to menu.");
      currentMode = IDLE;
      previousMode = FIND_LOCATION;
    }
  }
}

void useBaseStation() {
  Serial.println("\nUse Base Station ESP32 selected.");
  // Placeholder for base station logic
  currentMode = IDLE;
  previousMode = BASE_STATION;
}

void changeHotspotLocation() {
  if (selectedHotspots.empty()) {
    Serial.println("\nNo hotspots selected. Please select hotspots using option 2.");
    currentMode = IDLE;
    previousMode = CHANGE_LOCATION;
    return;
  }

  Serial.println("\nSelected Hotspots:");
  for (size_t i = 0; i < selectedHotspots.size(); i++) {
    Serial.println(String(i + 1) + ": " + selectedHotspots[i].ssid + " (Current Pos: (" + String(selectedHotspots[i].x) + ", " + String(selectedHotspots[i].y) + "), RSSI@1m: " + String(selectedHotspots[i].rssiAt1m) + ", PathLoss: " + String(selectedHotspots[i].pathLossExponent) + ")");
  }

  Serial.println("\nEnter the number of the hotspot to change location.");
  Serial.println("Enter 'q' to return to menu.");
  Serial.println("Enter 'c' to clear database.");
  while (!Serial.available()) {
    delay(100);
  }
  String input = Serial.readStringUntil('\n');
  input.trim();

  if (input == "q") {
    Serial.println("Returning to menu.");
    currentMode = IDLE;
    previousMode = CHANGE_LOCATION;
    return;
  } else if (input == "c") {
    hotspotDB.clear();
  } else if (input.toInt() > 0 && input.toInt() <= (int)selectedHotspots.size()) {
    WiFiHotspot& hotspot = selectedHotspots[input.toInt() - 1];
    Serial.print("Enter new x coordinate for ");
    Serial.print(hotspot.ssid);
    Serial.println(":");
    while (!Serial.available()) {
      delay(100);
    }
    hotspot.x = Serial.readStringUntil('\n').toFloat();

    Serial.print("Enter new y coordinate for ");
    Serial.print(hotspot.ssid);
    Serial.println(":");
    while (!Serial.available()) {
      delay(100);
    }
    hotspot.y = Serial.readStringUntil('\n').toFloat();

    Serial.print("Enter new RSSI at 1 meter for ");
    Serial.print(hotspot.ssid);
    Serial.println(" (e.g., -45.0):");
    while (!Serial.available()) {
      delay(100);
    }
    hotspot.rssiAt1m = Serial.readStringUntil('\n').toFloat();

    Serial.print("Enter new path loss exponent for ");
    Serial.print(hotspot.ssid);
    Serial.println(" (e.g., 2.5):");
    while (!Serial.available()) {
      delay(100);
    }
    hotspot.pathLossExponent = Serial.readStringUntil('\n').toFloat();

    // Update EEPROM
    hotspotDB.save(hotspot.ssid, hotspot.x, hotspot.y, hotspot.rssiAt1m, hotspot.pathLossExponent);
    Serial.println("Updated position in database: (" + String(hotspot.x) + ", " + String(hotspot.y) + "), RSSI@1m: " + String(hotspot.rssiAt1m) + ", PathLoss: " + String(hotspot.pathLossExponent));

  } else {
    Serial.println("Invalid selection.");
  }

  currentMode = IDLE;
  previousMode = CHANGE_LOCATION;
}

void viewDatabase() {
  hotspotDB.listAll();
  currentMode = IDLE;
  previousMode = VIEW_DATABASE;
}

String printLocalizationTechnique(LocalizationTechnique technique) {
  switch (technique) {
    case WIFI: return "WiFi";
    case WiFi_BaseStation: return "WiFi + BaseStation";
    case WiFi_LoRa: return "WiFi + LoRa";
    case WiFi_LoRa_BaseStation: return "WiFi + LoRa + BaseStation";
    default: return "Unknown";
  }
}

void updateSelectedHotspotsRSSI() {
  int n = WiFi.scanNetworks(false, false); // Re-scan networks
  if (n == 0) {
    selectedHotspots.clear();
    return;
  }

  std::vector<WiFiHotspot> networks;
  for (int i = 0; i < n; i++) {
    WiFiHotspot hotspot;
    hotspot.ssid = WiFi.SSID(i);
    hotspot.rssi = WiFi.RSSI(i);
    hotspot.x = 0.0;
    hotspot.y = 0.0;
    hotspot.rssiAt1m = -45.0; // Default value
    hotspot.pathLossExponent = 2.5; // Default value
    networks.push_back(hotspot);
  }

  // Create a new vector for hotspots that are still connected
  std::vector<WiFiHotspot> updatedHotspots;
  for (const auto& selected : selectedHotspots) {
    bool found = false;
    for (const auto& scanned : networks) {
      if (selected.ssid == scanned.ssid) {
        WiFiHotspot updatedHotspot = selected;
        updatedHotspot.rssi = scanned.rssi;
        updatedHotspots.push_back(updatedHotspot);
        found = true;
        break;
      }
    }
    if (!found) {
      Serial.print("Hotspot ");
      Serial.print(selected.ssid);
      Serial.println(" disconnected. Removing from selectedK selected hotspots.");
    }
  }

  // Replace selectedHotspots with updated
  selectedHotspots = std::move(updatedHotspots);

  WiFi.scanDelete();
}