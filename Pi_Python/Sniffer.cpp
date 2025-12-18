#include <WiFi.h>
#include<Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// wifi
const char* ssid = "ATTe5mu4cI";
const char* password = "e8i5kfgthaks";

// arduino config
const int LED_PIN = 5;
const long SCAN_INTERVAL = 30000;    // SCAN EVERY 30 SECONDS
const int MAX_KNOWN_DEVICES = 50;    // Limit stored devices to prevent memory issues
const int WIFI_TIMEOUT = 20000;      // 20 second WiFi connection timeout
const int NEW_DEVICE_DISPLAY_TIME = 5000; // Show new device for 5 seconds

// global vars
std::vector<String> knownDevices;
unsigned long previousScanMillis = 0;

// Vendor Detection (MAC OUI Lookup)
String getVendor(String bssid) {
    bssid.toUpperCase();

    // Apple
  if (bssid.startsWith("34:12:98") || bssid.startsWith("A8:BE:27") || 
      bssid.startsWith("00:03:93") || bssid.startsWith("AC:DE:48")) return "Apple";
  
  // Raspberry Pi
  if (bssid.startsWith("B8:27:EB") || bssid.startsWith("DC:A6:32") || 
      bssid.startsWith("E4:5F:01")) return "RaspberryPi";
  
  // Google
  if (bssid.startsWith("70:B3:D5") || bssid.startsWith("00:1A:11") || 
      bssid.startsWith("3C:5A:B4")) return "Google";
  
  // Samsung
  if (bssid.startsWith("B8:8A:EC") || bssid.startsWith("E8:50:8B") || 
      bssid.startsWith("34:23:87")) return "Samsung";
  
  // Amazon
  if (bssid.startsWith("74:C2:46") || bssid.startsWith("00:FC:8B")) return "Amazon";
  
  // Router Manufacturers
  if (bssid.startsWith("00:09:A5")) return "Netgear";
  if (bssid.startsWith("00:14:BF")) return "Linksys";
  if (bssid.startsWith("00:25:9C") || bssid.startsWith("50:C7:BF")) return "TP-Link";
  if (bssid.startsWith("00:18:E7")) return "Asus";
  
  // Microsoft
  if (bssid.startsWith("00:50:F2") || bssid.startsWith("7C:ED:8D")) return "Microsoft";
  
  // Intel
  if (bssid.startsWith("00:13:E8") || bssid.startsWith("A4:34:D9")) return "Intel";
  
  return "Unknown";
}

// LED Notification

void notifyNewDevice() {
    for (int i=0; i < 5; i++){
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}

// Display Update fns

void displayMessage(cont char* message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(message);
    display.display();
}

void displayNewDevice(String mac, String vendor) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0,0);
    display.println("New AP!");
    display.setTextSize(1);
    display.println();
    display.println(mac);
    display.print("Vendor: ");
    display.println(vendor);
    display.display();
}

void displayKnownDevices() {
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.print("Known APs: ");
    display.println(knownDevices.size());
    display.printlin("-------------------");

    // display 4 most recent devices (to fit on screen)
    int startIndex = max(0, (int)knownDevices.size() - 4);
    for (int i = startIndex; i < knownDevices.size(); i++) {
        String vendor = getVendor(knownDevices[i]);
        // show last 8 chars of MAC + vendor
        display.print(knownDevices[i].substring(9));
        display.print(" ");
        display.println(vendor.substring(0,8)); // truncate vendor if too long
    }
    display.display();
}