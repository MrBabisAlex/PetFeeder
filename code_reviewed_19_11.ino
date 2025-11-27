#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <Stepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "loadingScreen.h"
#include <EEPROM.h>

// ===== SETTINGS =====
int portions = 1;           // 1–10
int frequency = 2;          // 1–5 φορές/μέρα
int feedingTimes[4][2];     // max 5 φορές [hour, minute]
int timeCount = frequency;  // αριθμός ωρών βάσει frequency

enum SettingsState { SET_PORTIONS,
                     SET_FREQUENCY,
                     SET_TIMES,
                     SETTINGS_DONE
};
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
RTC_DS3231 rtc;

// ===== BUTTONS =====
const int buttonUp = 19;
const int buttonDown = 18;
const int buttonEnter = 17;
const int buttonBack = 16;

// ===== TIMERS =====
const unsigned long pressTimeout = 1000;
unsigned long lastBackPress = 0;

// ===== MENU =====
String menuItems[] = { "MANUAL", "AUTO", "SETTINGS" };
int menuSize = 3;
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
void autoFeedAnimation();

void setup() {
  Serial.begin(9600);

  // Initialize OLED
  Wire.begin(21, 22);
  Wire.setClock(100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1)
      ;
  }

  // Pin installation
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
  pinMode(buttonEnter, INPUT_PULLUP);
  pinMode(buttonBack, INPUT_PULLUP);

  // Set motor speed
  stepperMotor.setSpeed(5);

  // Create space for EEprom usage
  EEPROM.begin(512);

  // Load previous settings
  loadSettings();

  // Set the time manualy
  setRTCtimeOnBoot();
}

void loop() {
  now = rtc.now();
  checkLockToggle();
  if (isLocked) return;
  handleButtons();
  showMenu();
}


void setRTCtimeOnBoot() {
  int year = 2025, month = 1, day = 1, hour = 0, minute = 0;
  int cursor = 0;  // 0=year, 1=month, 2=day, 3=hour, 4=minute
  bool done = false;

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Set Date & Time");
  display.display();
  delay(1000);

  while (!done) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Set Date & Time");
    display.setCursor(0, 20);
    display.printf("Y:%04d M:%02d D:%02d", year, month, day);
    display.setCursor(0, 40);
    display.printf("T:%02d:%02d", hour, minute);

    switch (cursor) {
      case 0: display.setCursor(30, 30); break;  // Year
      case 1: display.setCursor(60, 30); break;  // Month
      case 2: display.setCursor(90, 30); break;  // Day
      case 3: display.setCursor(19, 50); break;  // Hour
      case 4: display.setCursor(36, 50); break;  // Minute
    }
    display.print("^");
    display.display();

    if (digitalRead(buttonDown) == LOW) {
      switch (cursor) {
        case 0: year++; break;
        case 1: month = (month < 12) ? month + 1 : 1; break;
        case 2: day = (day < 31) ? day + 1 : 1; break;
        case 3: hour = (hour + 1) % 24; break;
        case 4: minute = (minute + 1) % 60; break;
      }
      delay(200);
    }

    if (digitalRead(buttonUp) == LOW) {
      switch (cursor) {
        case 0:
          year--;
          if (year < 2020) year = 2025;
          break;
        case 1: month = (month > 1) ? month - 1 : 12; break;
        case 2: day = (day > 1) ? day - 1 : 31; break;
        case 3: hour = (hour == 0) ? 23 : hour - 1; break;
        case 4: minute = (minute == 0) ? 59 : minute - 1; break;
      }
      delay(200);
    }

    if (digitalRead(buttonEnter) == LOW) {
      cursor++;
      if (cursor > 4) {
        rtc.adjust(DateTime(year, month, day, hour, minute, 0));
        display.clearDisplay();
        display.setCursor(10, 25);
        display.println("Time Saved!");
        display.display();
        delay(1000);
        done = true;
      }
      delay(300);
    }

    if (digitalRead(buttonBack) == LOW) {
      rtc.adjust(DateTime(year, month, day, hour, minute, 0));
      done = true;
      delay(300);
    }
  }
  showMenu();
}

void showMenu() {
  now = rtc.now();

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setFont(NULL);
  display.setCursor(64, 10);
  display.printf("%02d:%02d", now.hour(), now.minute());
  display.setCursor(64, 30);
  display.setTextSize(1);
  display.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());

  // --- Σχεδίαση 3 επιλογών σε δικά τους πλαίσια ---
  for (int i = 0; i < menuSize; i++) {
    int y = 0 + i * 21;  // απόσταση μεταξύ επιλογών

    // Πλαίσιο επιλογής
    display.drawRoundRect(5, y, 54, 20, 9, WHITE);

    // Αν είναι επιλεγμένη → inverted text
    if (i == menuIndex) {
      display.fillRoundRect(5, y, 54, 20, 9, WHITE);
      display.setTextColor(BLACK, WHITE);
    } else {
      display.setTextColor(WHITE, BLACK);
    }
    display.setCursor(9, y + 6);
    display.print(menuItems[i]);
  }
  display.display();
}

// ===== BUTTON HANDLER =====
void handleButtons() {
  now = rtc.now();

  if (digitalRead(buttonUp) == LOW) {
    menuIndex++;
    if (menuIndex > 2) menuIndex = 0;
    showMenu();
    delay(200);
  }

  if (digitalRead(buttonDown) == LOW) {
    menuIndex--;
    if (menuIndex < 0) menuIndex = 2;
    showMenu();
    delay(200);
  }

  if (digitalRead(buttonEnter) == LOW) {
    if (menuIndex == 0) manualFeeding();
    if (menuIndex == 1) autoFeeding();
    if (menuIndex == 2) handleSettings();
    delay(200);
  }
}

// ===== MANUAL FEEDING =====
void manualFeeding() {
  stepperMotor.step(stepAmount);
  animation();
  display.clearDisplay();
  display.drawBitmap(0, 0, loading_screen_bitmapallArray[5], 128, 64, SSD1306_WHITE);
  display.display();
  delay(2000);
  showMenu();
}

// ===== AUTO FEEDING =====
void autoFeeding() {
  bool exitMode = false;
  static int lastDay = -1;
  bool feedingDoneToday[5] = { false, false, false, false, false };

  unsigned long lastLoop = 0;
  const unsigned long loopInterval = 500;  // τρέχει κάθε 0.5s

  while (!exitMode) {
    now = rtc.now();
    autoFeedAnimation();
    if (millis() - lastLoop >= loopInterval) {
      lastLoop = millis();
      now = rtc.now();

      // reset flags κάθε μέρα
      if (now.day() != lastDay) {
        for (int i = 0; i < 5; i++) feedingDoneToday[i] = false;
        lastDay = now.day();
      }

      // Έλεγχος για ώρες τάισματος
      for (int i = 0; i < frequency; i++) {
        int feedHour = feedingTimes[i][0];
        int feedMin = feedingTimes[i][1];

        if (!feedingDoneToday[i] && now.hour() == feedHour && now.minute() == feedMin) {
          display.clearDisplay();
          animation();
          display.display();

          stepperMotor.step(portions * stepAmount);
          feedingDoneToday[i] = true;

          delay(1000);  // μικρό delay για να δει την οθόνη
          autoFeedAnimation();
        }
      }
    }

    // Έλεγχος για back button
    static bool backPrevState = HIGH;
    bool backState = digitalRead(buttonBack);

    if (backState == LOW && backPrevState == HIGH) {
      unsigned long pressStart = millis();
      while (digitalRead(buttonBack) == LOW) {
        if (millis() - pressStart > 1000) {
          exitMode = true;
          break;
        }
      }
    }
    backPrevState = backState;
  }
  showMenu();
}


// ===== SETTINGS =====
void handleSettings() {
  now = rtc.now();
  bool done = false;
  settingsState = SET_PORTIONS;
  settingsCursor = 0;
  settingsSubCursor = 0;

  while (!done) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("%02d/%02d/%04d %02d:%02d",
                   rtc.now().day(), rtc.now().month(), rtc.now().year(),
                   rtc.now().hour(), rtc.now().minute());

    switch (settingsState) {
      case SET_PORTIONS:
        display.setCursor(0, 20);
        display.println("Set Portions:");
        display.setTextSize(2);
        display.setCursor(0, 35);
        display.println(portions);
        break;

      case SET_FREQUENCY:
        display.setTextSize(1);
        display.setCursor(0, 20);
        display.println("Set Frequency:");
        display.setTextSize(2);
        display.setCursor(0, 35);
        display.println(frequency);
        break;

      case SET_TIMES:
        display.setTextSize(1);
        display.setCursor(0, 15);
        display.println("Feeding Times:");
        for (int i = 0; i < timeCount; i++) {
          display.setCursor(0, 25 + i * 10);
          display.printf("%d: %02d:%02d", i + 1, feedingTimes[i][0], feedingTimes[i][1]);
          if (i == settingsCursor) {
            display.setCursor(60, 25 + i * 10);
            display.print(settingsSubCursor == 0 ? "Set hours" : "Set minutes");
          }
        }
        break;

      case SETTINGS_DONE:
        done = true;
        saveSettings();
        showMenu();
        return;
    }

    display.display();
    delay(100);

    // Buttons
    if (digitalRead(buttonDown) == LOW) {
      if (settingsState == SET_PORTIONS && portions < 10) portions++;
      else if (settingsState == SET_FREQUENCY && frequency < 4) {
        frequency++;
        timeCount = frequency;  // ενημέρωση timeCount
      } else if (settingsState == SET_TIMES) {
        if (settingsSubCursor == 0)
          feedingTimes[settingsCursor][0] = (feedingTimes[settingsCursor][0] + 1) % 24;
        else
          feedingTimes[settingsCursor][1] = (feedingTimes[settingsCursor][1] + 1) % 60;
      }
      delay(150);
    }

    if (digitalRead(buttonUp) == LOW) {
      if (settingsState == SET_PORTIONS && portions > 1) portions--;
      else if (settingsState == SET_FREQUENCY && frequency > 1) {
        frequency--;
        timeCount = frequency;  // ενημέρωση timeCount
      } else if (settingsState == SET_TIMES) {
        if (settingsSubCursor == 0)
          feedingTimes[settingsCursor][0] = (feedingTimes[settingsCursor][0] + 23) % 24;
        else
          feedingTimes[settingsCursor][1] = (feedingTimes[settingsCursor][1] + 59) % 60;
      }
      delay(150);
    }

    if (digitalRead(buttonEnter) == LOW) {
      if (settingsState == SET_TIMES) {
        settingsSubCursor++;
        if (settingsSubCursor > 1) {
          settingsSubCursor = 0;
          settingsCursor++;
          if (settingsCursor >= timeCount) settingsState = SETTINGS_DONE;
        }
      } else {
        if (settingsState == SET_PORTIONS) settingsState = SET_FREQUENCY;
        else if (settingsState == SET_FREQUENCY) settingsState = SET_TIMES;
      }
      delay(300);
    }

    if (digitalRead(buttonBack) == LOW) {
      settingsState = SETTINGS_DONE;
      delay(300);
    }
  }
}

void animation() {
  for (int frame = 0; frame < loading_screen_bitmapallArray_LEN; frame++) {
    display.clearDisplay();
    display.drawBitmap(0, 0, loading_screen_bitmapallArray[frame], 128, 64, SSD1306_WHITE);
    display.display();
    delay(250);
  }
}

// ===== EEPROM =====
void saveSettings() {
  EEPROM.write(0, portions);
  EEPROM.write(1, frequency);
  for (int i = 0; i < timeCount; i++) {
    EEPROM.write(2 + i * 2, feedingTimes[i][0]);
    EEPROM.write(2 + i * 2 + 1, feedingTimes[i][1]);
  }
  EEPROM.commit();
}

void loadSettings() {
  portions = EEPROM.read(0);
  if (portions < 1 || portions > 10) portions = 3;

  frequency = EEPROM.read(1);
  if (frequency < 1 || frequency > 4) frequency = 2;

  timeCount = frequency;  // ενημέρωση timeCount κατά φόρτωση

  for (int i = 0; i < timeCount; i++) {
    feedingTimes[i][0] = EEPROM.read(2 + i * 2);
    feedingTimes[i][1] = EEPROM.read(2 + i * 2 + 1);
    if (feedingTimes[i][0] > 23) feedingTimes[i][0] = 0;
    if (feedingTimes[i][1] > 59) feedingTimes[i][1] = 0;
  }
}

// ===== LOCK SYSTEM =====
void checkLockToggle() {
  static bool prevButtonState = HIGH;
  bool buttonState = digitalRead(buttonBack);

  if (millis() - lastBackPress > pressTimeout) backPressCount = 0;

  if (buttonState == LOW && prevButtonState == HIGH) {
    backPressCount++;
    lastBackPress = millis();
    delay(150);
  }
  prevButtonState = buttonState;

  if (!isLocked && backPressCount >= 3) {
    isLocked = true;
    backPressCount = 0;
    display.clearDisplay();
    display.drawBitmap(30, 15, lock_screen, 64, 35, WHITE);
    display.display();
  }

  while (isLocked) {
    buttonState = digitalRead(buttonBack);
    if (buttonState == LOW && prevButtonState == HIGH) {
      backPressCount++;
      lastBackPress = millis();
      delay(150);
    }
    prevButtonState = buttonState;
    if (millis() - lastBackPress > pressTimeout) backPressCount = 0;
    if (backPressCount >= 3) {
      isLocked = false;
      backPressCount = 0;
      display.clearDisplay();
      display.drawBitmap(30, 15, unlock_screen, 64, 35, WHITE);
      display.display();
      delay(800);
      showMenu();
    }
  }
}


// ===== AUTO FEED ANIMATION =====
void autoFeedAnimation() {
  now = rtc.now();

  display.clearDisplay();
  display.setTextColor(1);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setCursor(35, 1);
  display.printf("%02d:%02d", now.hour(), now.minute());
  display.drawBitmap(58, 33, image_device_reset_bits, 13, 16, 1);
  display.setTextSize(1);
  display.setCursor(32, 57);
  display.print("Hold back  ");
  display.drawBitmap(90, 58, image_arrow_curved_left_up_down_bits, 7, 5, 1);
  display.setCursor(38, 24);
  display.print("Auto Mode");
  display.drawBitmap(5, 33, image_paws_bits, 26, 26, 1);
  display.drawBitmap(97, 5, image_paws_bits, 26, 26, 1);
  display.display();
}
