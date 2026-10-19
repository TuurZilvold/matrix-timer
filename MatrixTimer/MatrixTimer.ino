#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>

// ==========================================
// 1. Hardware & Wi-Fi Instellingen
// ==========================================
const char* ssid     = "Wie dit leest trekt een bak";
const char* password = "Bennywintderingvaart";

// Zet op 'true' voor vast IP, of op 'false' als je automatisch een IP wilt (DHCP)
const bool USE_STATIC_IP = true;
IPAddress local_IP(192, 168, 68, 200); 
IPAddress gateway(192, 168, 68, 1);    
IPAddress subnet(255, 255, 255, 0);    

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   16

#define CLK_PIN    D5
#define DATA_PIN   D7
#define CS_PIN     D8
#define BUTTON_PIN D2 

// RobotDyn AC Dimmer Pinnen
// Let op: Rood zit op D3 (of D0 als D3 bezet is, maar haal D3 los tijdens USB-flashen indien nodig)
#define PIN_GREEN  D1
#define PIN_YELLOW D6
#define PIN_RED    D3

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
ESP8266WebServer server(80);

// ==========================================
// 2. Status & Timer Variabelen
// ==========================================
enum DisplayMode { MODE_TIMER, MODE_TEXT };
DisplayMode currentMode = MODE_TIMER;

const unsigned long DEFAULT_SECONDS = 213; // 03:33
unsigned long currentSeconds = DEFAULT_SECONDS;
unsigned long lastTick       = 0;
int brightness               = 4;

bool displayOn        = true;   
bool timerRunning     = false;  
bool turboMode        = false;  

// 00:00 Knipper Variabelen
bool isFlashing              = false;
unsigned long flashStartTime = 0;
const unsigned long FLASH_DURATION_MS = 60000UL; // 1 minuut knipperen

// Scrollende Tekst Variabelen
String customText = "";
int scrollX = 64; 
unsigned long lastScrollTime = 0;
const int SCROLL_SPEED_MS = 40; 

// Auto-sleep Variabelen (33 seconden inactiviteit)
unsigned long lastActivityTime = 0;
const unsigned long SLEEP_TIMEOUT_MS = 33000UL; 

// Knop State Machine Variabelen
bool lastBtnReading              = false;
bool btnStableState              = false;
bool isLongPress                 = false;
unsigned long lastDebounceTime   = 0;
unsigned long btnPressTime       = 0;
unsigned long lastReleaseTime    = 0;

const unsigned long DEBOUNCE_MS     = 35;  
const unsigned long LONG_PRESS_MS   = 400; // Na 0,4s inhouden start 3.3x turbo
const unsigned long DOUBLE_CLICK_MS = 400; // Max tijd tussen 2 klikken voor reset

// ==========================================
// 3. Font Data (180 Graden Gedraaid)
// ==========================================
#define FONT_COLS 6

const uint8_t BIG_FONT_TOP[11][FONT_COLS] = {
  {0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF}, {0x00, 0x00, 0x00, 0x07, 0xFF, 0xFF}, 
  {0x83, 0x83, 0x83, 0x83, 0xFF, 0xFF}, {0x83, 0x83, 0x83, 0x83, 0xFF, 0xFF}, 
  {0xFF, 0xFF, 0x80, 0x80, 0xFF, 0xFF}, {0xFF, 0xFF, 0x83, 0x83, 0x83, 0x83}, 
  {0xFF, 0xFF, 0x83, 0x83, 0x83, 0x83}, {0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF}, 
  {0xFF, 0xFF, 0x83, 0x83, 0xFF, 0xFF}, {0xFF, 0xFF, 0x83, 0x83, 0xFF, 0xFF}, 
  {0x18, 0x18, 0x00, 0x00, 0x00, 0x00}
};

const uint8_t BIG_FONT_BOT[11][FONT_COLS] = {
  {0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF}, {0x00, 0x00, 0xC0, 0xE0, 0xFF, 0xFF}, 
  {0xFF, 0xFF, 0xC1, 0xC1, 0xC1, 0xC1}, {0xC1, 0xC1, 0xC1, 0xC1, 0xFF, 0xFF}, 
  {0x01, 0x01, 0x01, 0x01, 0xFF, 0xFF}, {0xC1, 0xC1, 0xC1, 0xC1, 0xFF, 0xFF}, 
  {0xFF, 0xFF, 0xC1, 0xC1, 0xFF, 0xFF}, {0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F}, 
  {0xFF, 0xFF, 0xC1, 0xC1, 0xFF, 0xFF}, {0xC1, 0xC1, 0xC1, 0xC1, 0xFF, 0xFF}, 
  {0x18, 0x18, 0x00, 0x00, 0x00, 0x00}
};
const uint8_t GLYPH_WIDTH[11] = {6,6,6,6,6,6,6,6,6,6,2};

const uint8_t SYS_FONT[41][5] = {
  {0x00, 0x00, 0x00, 0x00, 0x00}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},