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
  }
}

void displayMenu() {
  Serial.println("\n=== WiFi Localization System Menu ===");
  Serial.println("Current Technique: " + printLocalizationTechnique(localizationTechnique));

  // Display selected hotspots
  Serial.println("\nSelected Hotspots:");
  if (!selectedHotspots.empty()) {
    for (size_t i = 0; i < selectedHotspots.size(); i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(selectedHotspots[i].ssid);
      Serial.print(" (RSSI: ");
      Serial.print(selectedHotspots[i].rssi);
      Serial.print(" dBm, Pos: (");
      Serial.print(selectedHotspots[i].x);
      Serial.print(", ");
      Serial.print(selectedHotspots[i].y);
      Serial.println("))");
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
    networks.push_back(hotspot);
  }

  // Sort networks by RSSI (descending order)
  std::sort(networks.begin(), networks.end(), compareByRSSI);

  // Display sorted networks
  Serial.println("Networks found (sorted by RSSI):");
  for (size_t i = 0; i < networks.size(); i++) {
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(networks[i].ssid);
    Serial.print(" (RSSI: ");
    Serial.print(networks[i].rssi);
    Serial.println(" dBm)");
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
        float x, y;
        if (hotspotDB.load(newHotspot.ssid, x, y)) {
          newHotspot.x = x;
          newHotspot.y = y;
          Serial.print("Loaded position from database: (");
          Serial.print(x);
          Serial.print(", ");
          Serial.print(y);
          Serial.println(")");
        } else {
          // Prompt for position
          Serial.print("Enter x coordinate for ");
          Serial.print(newHotspot.ssid);
          Serial.println(":");
          while (!Serial.available()) {
            delay(100);
          }
          newHotspot.x = Serial.readStringUntil('\n').toFloat();

          Serial.print("Enter y coordinate for ");
          Serial.print(newHotspot.ssid);
          Serial.println(":");
          while (!Serial.available()) {
            delay(100);
          }
          newHotspot.y = Serial.readStringUntil('\n').toFloat();

          // Save to EEPROM
          hotspotDB.save(newHotspot.ssid, newHotspot.x, newHotspot.y);
          Serial.print("Saved position to database: (");
          Serial.print(newHotspot.x);
          Serial.print(", ");
          Serial.print(newHotspot.y);
          Serial.println(")");
        }
        selectedHotspots.push_back(newHotspot);
        Serial.print("Selected: ");
        Serial.print(newHotspot.ssid);
        Serial.print(" (RSSI: ");
        Serial.print(newHotspot.rssi);
        Serial.print(" dBm, Pos: (");
        Serial.print(newHotspot.x);
        Serial.print(", ");
        Serial.print(newHotspot.y);
        Serial.println("))");
      } else {
        Serial.print("Hotspot ");
        Serial.print(networks[sel - 1].ssid);
        Serial.println(" already selected.");
      }
    } else {
      Serial.print("Invalid selection: ");
      Serial.println(sel);
    }
  }

  WiFi.scanDelete();
}


void findLocation() {
  static unsigned long lastScanTime = 0;
  const unsigned long scanInterval = 2000;  // 5 seconds

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

    // // Update RSSI values for selected hotspots
    updateSelectedHotspotsRSSI();

    // Perform localization
    Serial.println("\nFinding location using trilateration...");
    LocationFinder locator(selectedHotspots);
    float x, y;
    if (locator.findLocation(x, y)) {
      Serial.print("Estimated ESP32 location: (" + String(x) + ", " + String(y) + ")");
    } else {
      Serial.println("Failed to estimate location. Ensure hotspots have valid positions and RSSI values.");
    }
    lastScanTime = millis();
  }

  // Check for user input to exit
  Serial.println("\nEnter 'q' to return to menu:");
  unsigned long startTime = millis();
  while (!Serial.available() && millis() - startTime < scanInterval) {
    delay(100);
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
    Serial.print(String(i + 1) + ": ");
    Serial.print(selectedHotspots[i].ssid);
    Serial.print(" (Current Pos: (" + String(selectedHotspots[i].x) + ", " + String(selectedHotspots[i].y) + "))");
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
  }
  else if (input == "c") {
    hotspotDB.clear();
  }

  else if (input.toInt() > 0 && input.toInt() <= (int)selectedHotspots.size()) {
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

    // Update EEPROM
    hotspotDB.save(hotspot.ssid, hotspot.x, hotspot.y);
    Serial.print("Updated position in database: (");
    Serial.print(hotspot.x);
    Serial.print(", ");
    Serial.print(hotspot.y);
    Serial.println(")");
  } else {
    Serial.println("Invalid selection.");
  }

  currentMode = IDLE;
  previousMode = CHANGE_LOCATION;
}

void viewDatabase() {
  std::vector<HotspotEntry> database;
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
  Serial.println("Updating RSSI values for selected hotspots...");
  int n = WiFi.scanNetworks(false, false); // Re-scan networks
  if (n == 0) {
    Serial.println("No networks found during RSSI update.");
    return;
  }
  std::vector<WiFiHotspot> networks;
  for (int i = 0; i < n; i++) {
    WiFiHotspot hotspot;
    hotspot.ssid = WiFi.SSID(i);
    hotspot.rssi = WiFi.RSSI(i);
    networks.push_back(hotspot);
  }
  for (auto& selected : selectedHotspots) {
    bool found = false;
    for (const auto& scanned : networks) {
      if (selected.ssid == scanned.ssid) {
        selected.rssi = scanned.rssi;
        found = true;
        Serial.print("Updated RSSI for ");
        Serial.print(selected.ssid);
        Serial.print(": ");
        Serial.print(selected.rssi);
        Serial.println(" dBm");
        break;
      }
    }
    if (!found) {
      Serial.print("Warning: Hotspot ");
      Serial.print(selected.ssid);
      Serial.println(" not found in current scan.");
      selected.rssi = -100; // Set a default low RSSI if not found
    }
  }
  WiFi.scanDelete();
}
