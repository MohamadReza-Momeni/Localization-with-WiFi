#include <WiFi.h>
#include <EEPROM.h>
#include <vector>

// Structure to store WiFi hotspot details
struct WiFiHotspot {
  String ssid;
  int32_t rssi;
  float x, y;  // Coordinates
};

// Class for finding location using trilateration
class LocationFinder {
private:
  const float PATH_LOSS_EXPONENT = 2.5;  // n, environment-dependent
  const float RSSI_AT_1M = -45.0;        // A, RSSI at 1 meter
  const int MIN_HOTSPOTS = 2;            // Minimum hotspots for trilateration
  std::vector<WiFiHotspot> hotspots;

  // Convert RSSI to distance (meters)
  float rssiToDistance(int32_t rssi) {
    return pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
  }

public:
  LocationFinder(const std::vector<WiFiHotspot>& selectedHotspots)
    : hotspots(selectedHotspots) {}

  // Perform trilateration to estimate position
  bool findLocation(float& x, float& y) {
    if (hotspots.size() < MIN_HOTSPOTS) {
      Serial.println("Not Enough Hotspots where selected");
      return false;
    }

    // For simplicity, use a least-squares approximation for trilateration
    // Solve system of equations: (x - xi)^2 + (y - yi)^2 = di^2
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

    // Average to normalize
    size_t n = hotspots.size();
    A /= n;
    B /= n;
    C /= n;
    D /= n;
    E /= n;
    F /= n;

    // Solve linear system for x, y (simplified least-squares)
    float denominator = C * C - 1;
    // if (fabs(denominator) < 1e-6) {
    //   return false;  // Avoid division by zero
    // }

    x = (A * D - E * C) / denominator;
    y = (B * D - F * C) / denominator;

    return true;
  }
};

// Array to store selected hotspots
std::vector<WiFiHotspot> selectedHotspots;

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

// EEPROM database settings
#define EEPROM_SIZE 512
#define MAX_HOTSPOTS 12  // Max 12 hotspots (40 bytes each: 32 SSID + 4 x + 4 y)
#define SSID_MAX_LEN 32
struct HotspotEntry {
  char ssid[SSID_MAX_LEN];
  float x, y;
};

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
void saveHotspotToEEPROM(const String& ssid, float x, float y);
bool loadHotspotFromEEPROM(const String& ssid, float& x, float& y);
void readEEPROMDatabase(std::vector<HotspotEntry>& database);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("WiFi Localization System Started");

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

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
        if (loadHotspotFromEEPROM(newHotspot.ssid, x, y)) {
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
          saveHotspotToEEPROM(newHotspot.ssid, newHotspot.x, newHotspot.y);
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

// void findLocation() {
//   const int MIN_HOTSPOTS = 2;  // Minimum number of hotspots required
//   if (selectedHotspots.size() < MIN_HOTSPOTS) {
//     Serial.println("\nError: At least " + String(MIN_HOTSPOTS) + " hotspots are required for localization.");
//     Serial.println("Please select more hotspots using option 2.");
//   } else {
//     Serial.println("\nFind Location selected. Ready to proceed with localization.");
//     LocationFinder locator(selectedHotspots);
//     float x, y;
//     if (locator.findLocation(x, y)) {
//       Serial.print("Estimated ESP32 location: (" + String(x) + ", " + String(y) + ")");
//     } else {
//       Serial.println("Failed to estimate location. Ensure hotspots have valid positions.");
//     }
//   }
//   currentMode = IDLE;
//   previousMode = FIND_LOCATION;
// }

void findLocation() {
    static unsigned long lastScanTime = 0;
    const unsigned long scanInterval = 5000; // 5 seconds

    // Perform scan and localization if it's time
    if (millis() - lastScanTime >= scanInterval) {
        if (selectedHotspots.size() < 3) {
            Serial.println("\nError: At least 3 hotspots are required for localization.");
            Serial.println("Please select more hotspots using option 2.");
            Serial.println("Enter 'q' to return to menu:");
            lastScanTime = millis();
            return;
        }

        // Update RSSI values for selected hotspots
        updateSelectedHotspotsRSSI();

        // Perform localization
        Serial.println("\nFinding location using trilateration...");
        LocationFinder locator(selectedHotspots);
        float x, y;
        if (locator.findLocation(x, y)) {
            Serial.print("Estimated ESP32 location: (");
            Serial.print(x);
            Serial.print(", ");
            Serial.print(y);
            Serial.println(")");
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
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(selectedHotspots[i].ssid);
    Serial.print(" (Current Pos: (");
    Serial.print(selectedHotspots[i].x);
    Serial.print(", ");
    Serial.print(selectedHotspots[i].y);
    Serial.println("))");
  }

  Serial.println("\nEnter the number of the hotspot to change location, or '0' to return to menu:");
  while (!Serial.available()) {
    delay(100);
  }
  String input = Serial.readStringUntil('\n');
  input.trim();
  int choice = input.toInt();

  if (choice == 0) {
    Serial.println("Returning to menu.");
    currentMode = IDLE;
    previousMode = CHANGE_LOCATION;
    return;
  }

  if (choice > 0 && choice <= (int)selectedHotspots.size()) {
    WiFiHotspot& hotspot = selectedHotspots[choice - 1];
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
    saveHotspotToEEPROM(hotspot.ssid, hotspot.x, hotspot.y);
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
  readEEPROMDatabase(database);

  Serial.println("\n=== Database Contents ===");
  if (database.empty()) {
    Serial.println("Database is empty.");
  } else {
    for (size_t i = 0; i < database.size(); i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(database[i].ssid);
      Serial.print(" (Pos: (");
      Serial.print(database[i].x);
      Serial.print(", ");
      Serial.print(database[i].y);
      Serial.println("))");
    }
  }

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

void saveHotspotToEEPROM(const String& ssid, float x, float y) {
  std::vector<HotspotEntry> database;
  readEEPROMDatabase(database);

  // Check if SSID already exists
  int existingIndex = -1;
  for (size_t i = 0; i < database.size(); i++) {
    if (String(database[i].ssid) == ssid) {
      existingIndex = i;
      break;
    }
  }

  HotspotEntry entry;
  strncpy(entry.ssid, ssid.c_str(), SSID_MAX_LEN - 1);
  entry.ssid[SSID_MAX_LEN - 1] = '\0';
  entry.x = x;
  entry.y = y;

  int address = 0;
  if (existingIndex != -1) {
    // Update existing entry
    address = existingIndex * sizeof(HotspotEntry);
  } else if (database.size() < MAX_HOTSPOTS) {
    // Add new entry
    address = database.size() * sizeof(HotspotEntry);
    database.push_back(entry);
  } else {
    // Overwrite oldest entry (FIFO)
    address = 0;
    for (size_t i = 0; i < database.size() - 1; i++) {
      database[i] = database[i + 1];
    }
    database[database.size() - 1] = entry;
  }

  // Write to EEPROM
  EEPROM.put(address, entry);
  EEPROM.commit();

  // Write rest of database if FIFO shift occurred
  if (existingIndex == -1 && database.size() >= MAX_HOTSPOTS) {
    for (size_t i = 0; i < database.size(); i++) {
      EEPROM.put(i * sizeof(HotspotEntry), database[i]);
    }
    EEPROM.commit();
  }
}

bool loadHotspotFromEEPROM(const String& ssid, float& x, float& y) {
  std::vector<HotspotEntry> database;
  readEEPROMDatabase(database);

  for (const auto& entry : database) {
    if (String(entry.ssid) == ssid) {
      x = entry.x;
      y = entry.y;
      return true;
    }
  }
  return false;
}

void readEEPROMDatabase(std::vector<HotspotEntry>& database) {
  database.clear();
  HotspotEntry entry;
  for (int i = 0; i < MAX_HOTSPOTS; i++) {
    int address = i * sizeof(HotspotEntry);
    EEPROM.get(address, entry);
    if (entry.ssid[0] != '\0' && entry.ssid[0] != 0xFF) {  // Check for valid entry
      database.push_back(entry);
    }
  }
}