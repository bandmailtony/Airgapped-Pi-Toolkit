#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "esp_wifi.h"

// --- CONFIG ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 5
#define CLK_PIN 18
#define DATA_PIN 23
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define RXD2 16
#define TXD2 17

// --- MODES ---
enum SystemMode { MODE_MENU, MODE_CPU, MODE_WAR, MODE_RAIN, MODE_INFO, MODE_BRIDGE };
SystemMode currentMode = MODE_MENU;
SystemMode previousMode = MODE_MENU; // Track previous mode for cleanup

// --- GLOBALS ---
String menuText = "MAIN MENU"; 
String infoText = "";
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile int packetCount = 0;

#define SERIAL_TIMEOUT_MS 100

// --- TELNET SERVER ---
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// --- PROMISCUOUS CALLBACK ---
void IRAM_ATTR wifi_promiscuous_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  portENTER_CRITICAL_ISR(&mux);
  packetCount++;
  portEXIT_CRITICAL_ISR(&mux);
}

int getAndResetPacketCount() {
  portENTER_CRITICAL(&mux);
  int count = packetCount;
  packetCount = 0;
  portEXIT_CRITICAL(&mux);
  return count;
}

void cleanupMode(SystemMode mode) {
  switch (mode) {
    case MODE_RAIN:
      esp_wifi_set_promiscuous(false);
      break;
    case MODE_BRIDGE:
      if (telnetClient && telnetClient.connected()) {
        telnetClient.println("BRIDGE MODE ENDING");
        telnetClient.stop();
      }
      telnetServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      break;
    case MODE_WAR:
      WiFi.scanDelete(); // Free scan memory
      break;
    default:
      break;
  }
}

void setMode(SystemMode newMode) {
  if (newMode != currentMode) {
    cleanupMode(currentMode);
    previousMode = currentMode;
    currentMode = newMode;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial2.setRxBufferSize(1024);
  Serial2.setTimeout(SERIAL_TIMEOUT_MS); // Prevent indefinite blocking

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }
  display.setTextColor(WHITE);
  
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 1);
  mx.clear();
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  display.clearDisplay(); // Clear before drawing
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("CYBERDECK OS");
  display.setCursor(20, 35);
  display.println("v1.1"); // Version indicator
  display.display();
  delay(1500);
  
  drawMenu(); // Show menu on boot
}

void loop() {
  // --- SPECIAL HANDLING FOR BRIDGE MODE ---
  if (currentMode == MODE_BRIDGE) {
    runBridgeLoop();
    return;
  }

  // --- NORMAL COMMAND PARSER ---
  if (Serial2.available() > 0) {
    String cmd = Serial2.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.length() == 0) return; // Ignore empty commands

    if (cmd.startsWith("MENU:")) {
      setMode(MODE_MENU);
      menuText = cmd.substring(5);
      drawMenu();
    }
    else if (cmd == "MODE:WAR") {
      setMode(MODE_WAR);
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
    }
    else if (cmd == "MODE:RAIN") {
      setMode(MODE_RAIN);
      mx.clear();
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
    }
    else if (cmd == "MODE:BRIDGE") {
      setMode(MODE_BRIDGE);
      setupBridge();
    }
    else if (cmd.startsWith("INFO:")) {
      setMode(MODE_INFO);
      infoText = cmd.substring(5);
      drawInfo();
    }
    else if (cmd.startsWith("CPU:")) {
      if (currentMode != MODE_CPU) setMode(MODE_CPU);
      drawCpuGraph(cmd.substring(4).toInt());
    }
    else if (cmd == "MODE:CPU") {
      setMode(MODE_CPU);
    }
    else if (cmd == "MODE:MENU") { // Explicit menu command
      setMode(MODE_MENU);
      drawMenu();
    }
  }

  // --- MODE-SPECIFIC LOOPS ---
  if (currentMode == MODE_WAR) runWarDrive();
  if (currentMode == MODE_RAIN) runTrafficRain();
  if (currentMode == MODE_MENU) runKnightRider();
}

// ==========================================
//           BRIDGE MODE (TELNET)
// ==========================================

void setupBridge() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("TERMINAL MODE");
  display.println("Creating AP...");
  display.display();
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CYBERDECK_NET", "password123");
  delay(100); // Allow AP to stabilize
  
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("TERMINAL MODE");
  display.println("--------------");
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  display.println("Port: 23");
  display.println("");
  display.println("Waiting for client...");
  display.println("");
  display.println("[Pi sends MODE:MENU");
  display.println(" to exit]");
  display.display();
  
  mx.clear();
  mx.setColumn(0, 0xFF);
  mx.setColumn(31, 0xFF);
}

void runBridgeLoop() {
  static String bridgeBuffer = "";
  
  // 1. Handle New Clients
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) {
        telnetClient.stop();
      }
      telnetClient = telnetServer.available();
      telnetClient.setNoDelay(true);
      telnetClient.println("CONNECTED TO CYBERDECK BRIDGE");
      telnetClient.println("Type commands for Pi. Pi sends 'MODE:MENU' to exit.");
      telnetClient.println("");
      
      display.fillRect(0, 40, 128, 24, BLACK);
      display.setCursor(0, 40);
      display.println("CLIENT CONNECTED!");
      display.display();
      
      for (int i = 0; i < 32; i++) {
        mx.setColumn(i, 0x81);
      }
    } else {
      // Reject additional clients
      telnetServer.available().stop();
    }
  }

  // 2. WiFi -> Pi (Client typing commands)
  if (telnetClient && telnetClient.connected()) {
    while (telnetClient.available()) {
      char c = telnetClient.read();
      Serial2.write(c);
    }
  }

  // 3. Pi -> WiFi (Pi sending output back)
  while (Serial2.available()) {
    char c = Serial2.read();
    bridgeBuffer += c;
    
    if (bridgeBuffer.endsWith("MODE:MENU\n") || bridgeBuffer.endsWith("MODE:MENU\r\n")) {
      // Exit bridge mode
      if (telnetClient && telnetClient.connected()) {
        telnetClient.println("\n[EXITING BRIDGE MODE]");
        telnetClient.stop();
      }
      telnetServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      
      currentMode = MODE_MENU;
      bridgeBuffer = "";
      drawMenu();
      return;
    }
    
    // Forward to telnet client
    if (telnetClient && telnetClient.connected()) {
      telnetClient.write(c);
    }
    
    if (bridgeBuffer.length() > 20) {
      bridgeBuffer = bridgeBuffer.substring(bridgeBuffer.length() - 20);
    }
  }
  
  yield();
}

// ==========================================
//           ANIMATION FUNCTIONS
// ==========================================

void runKnightRider() {
  static int pos = 0;
  static int dir = 1;
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate < 30) return;
  lastUpdate = millis();
  
  mx.clear();
  for (int i = 0; i < 3; i++) {
    int col = pos + i;
    if (col >= 0 && col < 32) {
      mx.setColumn(col, 0x18);
    }
  }
  
  pos += dir;
  if (pos >= 29) dir = -1; 
  if (pos <= 0) dir = 1;    
}

void runTrafficRain() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate < 50) return;
  lastUpdate = millis();
  
  int currentPackets = getAndResetPacketCount();
  
  // 1. UPDATE OLED
  display.clearDisplay();
  display.setCursor(0, 0); 
  display.println("PASSIVE RADAR");
  display.println("-------------");
  display.print("PKTS/SEC: ");
  display.println(currentPackets * 20);
  
  display.setCursor(0, 35);
  display.print("Intensity:");
  
  int barWidth = map(currentPackets, 0, 50, 0, 128); 
  barWidth = constrain(barWidth, 0, 128); // Prevent overflow
  display.fillRect(0, 45, barWidth, 10, WHITE);
  display.drawRect(0, 45, 128, 10, WHITE); // Border for visibility
  display.display();

  // 2. UPDATE MATRIX
  mx.transform(MD_MAX72XX::TSD);
  
  int dropsToSpawn = map(currentPackets, 0, 30, 0, 8);
  dropsToSpawn = constrain(dropsToSpawn, 0, 8);
  
  for (int i = 0; i < dropsToSpawn; i++) {
    int randomCol = random(32);
    mx.setPoint(7, randomCol, true);
  }
}

void runWarDrive() {
  static unsigned long lastScan = 0;
  
  if (millis() - lastScan < 3000) return; // Scan every 3 seconds
  lastScan = millis();
  
  esp_wifi_set_promiscuous(false);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WARDRIVER");
  display.println("Scanning...");
  display.display();

  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
  
  display.clearDisplay();
  mx.clear();

  if (n <= 0) {
    display.setCursor(0, 0);
    display.println("WARDRIVER");
    display.println("-------------");
    display.println("No networks found");
    display.println("or scan error");
  } else {
    display.setCursor(0, 0);
    display.print("NETWORKS: ");
    display.println(n);
    display.println("----------------");
    
    int displayCount = min(n, 5);
    for (int i = 0; i < displayCount; i++) {
      display.print(WiFi.RSSI(i));
      display.print(" ");
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) ssid = "[Hidden]";
      display.println(ssid.substring(0, 12));
    }

    // Matrix visualization
    int blockWidth = max(1, 32 / n);
    
    for (int i = 0; i < n && i < 32; i++) {
      int32_t rssi = WiFi.RSSI(i);
      int height = map(rssi, -100, -40, 1, 8);
      height = constrain(height, 1, 8);

      for (int w = 0; w < blockWidth && (i * blockWidth + w) < 32; w++) {
        int colIndex = (i * blockWidth) + w;
        for (int row = 0; row < height; row++) {
          mx.setPoint(row, colIndex, true);
        }
      }
    }
  }
  
  display.display();
  WiFi.scanDelete(); // Free memory from scan results
}

// ==========================================
//           DRAWING FUNCTIONS
// ==========================================

void drawMenu() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("=== CYBERDECK ===");
  display.setCursor(0, 20);
  display.print("> ");
  display.println(menuText);
  
  display.setCursor(0, 55);
  display.print("[ROTATE TO SELECT]");
  display.display();
}

void drawInfo() {
  display.clearDisplay();
  mx.clear();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SYSTEM STATUS:");
  display.println("--------------");
  display.println(infoText); 
  display.display();
  
  mx.setRow(0, 0xFF);
  mx.setRow(7, 0xFF);
}

void drawCpuGraph(int val) {
  val = constrain(val, 0, 100); // Safety bounds
  
  int cols = map(val, 0, 100, 0, 31);
  mx.clear();
  for (int i = 0; i <= cols; i++) {
    mx.setColumn(i, 0xFF);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); 
  display.println("CPU LOAD");
  display.setTextSize(3); 
  display.setCursor(20, 20); 
  display.print(val); 
  display.print("%");
  display.setTextSize(1);
  display.display();
}
