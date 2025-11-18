#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <Stepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "loadingScreen.h"
#include <EEPROM.h>

// ===== SETTINGS =====
int portions = 1;        // 1–10
int frequency = 2;       // 1–5 φορές/μέρα
int feedingTimes[4][2];  // max 5 φορές [hour, minute]
int timeCount = frequency;       // αριθμός ωρών βάσει frequency

enum SettingsState { SET_PORTIONS, SET_FREQUENCY, SET_TIMES, SETTINGS_DONE };
SettingsState settingsState = SET_PORTIONS;
int settingsCursor = 0;
int settingsSubCursor = 0;

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== STEPPER =====
#define STEPS 2048
Stepper stepperMotor(STEPS, 14, 26, 27, 25);  //ini1 => 16, ini3 => 14 , ini2 => 10, ini4 => 15

// ===== RTC =====
// RTC_DS3231 rtc;

// ===== BUTTONS =====
const int buttonUp = 19;
const int buttonDown = 18;
const int buttonEnter = 17;
const int buttonBack = 16;

// ===== TIMERS =====
const unsigned long pressTimeout = 1000;
unsigned long lastBackPress = 0;

// ===== MENU =====
int menuIndex = 0;
int stepAmount = 512;
int backPressCount = 0;
bool isLocked = false;
DateTime now;

// ===== PROTOTYPES =====
void showMenu();
void handleButtons();
void manualFeeding();
void animation();
void checkLockToggle();
void handleSettings();
void saveSettings();
void loadSettings();
void autoFeeding();
void setRTCtimeOnBoot();

void setup() {
  Serial.begin(9600);

  // Initialize OLED *πριν από οτιδήποτε άλλο*
  Wire.begin(21, 22);
  Wire.setClock(100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Μετά τα υπόλοιπα
  // if (!rtc.begin()) {
  //   Serial.println("RTC not found!");
  //   while (1);
  // }

  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
  pinMode(buttonEnter, INPUT_PULLUP);
  pinMode(buttonBack, INPUT_PULLUP);

  // stepperMotor.setSpeed(5);
  EEPROM.begin(512);
  // loadSettings();

  // setRTCtimeOnBoot();
  showMenu();
}

void loop(){

}

void showMenu() {
  // now = rtc.now();
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  // display.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());
  // display.setCursor(90, 0);
  // display.printf("%02d:%02d", now.hour(), now.minute());

  display.setCursor(0, 15);
  display.println(menuIndex==0?"> manual feeding":"  manual feeding");
  display.setCursor(0, 30);
  display.println(menuIndex==1?"> auto feeding":"  auto feeding");
  display.setCursor(0, 45);
  display.println(menuIndex==2?"> settings":"  settings");
  display.display();
}