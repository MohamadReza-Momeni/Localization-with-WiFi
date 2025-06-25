#include <WiFi.h>
#include <vector>

// Structure to store WiFi hotspot details
struct WiFiHotspot {
  String ssid;
  int32_t rssi;
};

// Array to store selected hotspots
std::vector<WiFiHotspot> selectedHotspots;

// Program modes
enum ProgramMode {
  IDLE,
  FIND_LOCATION,
  SELECT_HOTSPOTS,
  BASE_STATION
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
String modeToString(ProgramMode mode);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("WiFi Localization System Started");

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
      default:
        Serial.println("Invalid option, please choose 1, 2, or 3.");
        break;
    }
  } else if (currentMode == SELECT_HOTSPOTS) {
    selectHotspots();
  }
}

void displayMenu() {
  Serial.println("\n=== WiFi Localization System Menu ===");
  Serial.println("Current Mode: " + modeToString(localizationTechnique));

  // Display selected hotspots
  Serial.println("\nSelected Hotspots:");
  if (!selectedHotspots.empty()) {
    for (size_t i = 0; i < selectedHotspots.size(); i++) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(selectedHotspots[i].ssid);
      Serial.print(" (RSSI: ");
      Serial.print(selectedHotspots[i].rssi);
      Serial.println(" dBm)");
    }
  } else {
    Serial.println("No hotspots selected yet.");
  }

  Serial.println("\nOptions:");
  Serial.println("1. Find Location");
  Serial.println("2. Select WiFi Hotspots");
  Serial.println("3. Use Base Station ESP32");
  Serial.println("Enter your choice (1-3):");
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
        selectedHotspots.push_back(networks[sel - 1]);
        Serial.print("Selected: ");
        Serial.print(networks[sel - 1].ssid);
        Serial.print(" (RSSI: ");
        Serial.print(networks[sel - 1].rssi);
        Serial.println(" dBm)");
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
  const int MIN_HOTSPOTS = 3;  // Minimum number of hotspots required
  if (selectedHotspots.size() < MIN_HOTSPOTS) {
    Serial.println("\nError: At least " + String(MIN_HOTSPOTS) + " hotspots are required for localization.");
    Serial.println("Please select more hotspots using option 2.");
  } else {
    Serial.println("\nFind Location selected. Ready to proceed with localization.");
    // Placeholder for localization logic
  }
  currentMode = IDLE;
  previousMode = FIND_LOCATION;
}

void useBaseStation() {
  Serial.println("\nUse Base Station ESP32 selected.");
  // Placeholder for base station logic
  currentMode = IDLE;
  previousMode = BASE_STATION;
}

String modeToString(LocalizationTechnique localizationTechnique) {
  switch (localizationTechnique) {
    case WIFI: return "WiFi";
    case WiFi_BaseStation: return "WiFi + BaseStation";
    case WiFi_LoRa: return "WiFi + LoRa";
    case WiFi_LoRa_BaseStation: return "WiFi + LoRa + BaseStation";
    default: return "Unknown";
  }
}