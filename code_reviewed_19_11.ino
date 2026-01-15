#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <Stepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "loadingScreen.h"
#include <EEPROM.h>

// ===== SETTINGS =====
int portions = 1;          // 1–10
int frequency = 2;         // 1–5 φορές/μέρα
int feedingTimes[4][2];    // max 4 φορές [hour, minute]
int timeCount = frequency; // αριθμός ωρών βάσει frequency

//====AUTOFEED=====
static int lastDay = -1;
static int lastMonth = -1;
static int lastYear = -1;
bool feedingDoneToday[5] = {false, false, false, false, false};
unsigned long afLastCheck = 0;

enum SettingsState
{
  SET_PORTIONS,
  SET_FREQUENCY,
  SET_TIMES,
  SETTINGS_DONE
};
SettingsState settingsState = SET_PORTIONS;

int settingsCursor = 0;
int settingsSubCursor = 0;

enum AppState
{
  APP_SHOW_MENU,
  APP_FEED_NOW,
  APP_AUTO_FEED,
  APP_SETTINGS,
  APP_SET_TIME
};
AppState appState = APP_SHOW_MENU;

enum MenuSettings
{
  MANUAL_FEEDING,
  AUTO_FEEDING,
  SETTINGS,
  DATE_TIME_SETTING,
  MENU_COUNT
};
MenuSettings menuSettings = MANUAL_FEEDING;

enum AF_states
{
  AF_IDLE_STATE,
  AF_CHECK_TIME_STATE,
  AF_RESET_FLAGS
};
AF_states af_states = AF_IDLE_STATE;

//===== DATE ======
struct TimeData
{
  int hour;
  int minute;
  int seconds;
};
TimeData CurrentTime;

struct DateData
{
  int year;
  int month;
  int day;
};
DateData CurrentDate;

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== STEPPER =====
#define STEPS 2048
Stepper stepperMotor(STEPS, 14, 26, 27, 25); // ini1 => 14, ini3 => 26 , ini2 => 27, ini4 => 25

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
int lastDisplayedHour = -1;
int lastDisplayedMinute = -1;
int stepAmount = 512;
int backPressCount = 0;
bool isLocked = false;
int previousIndex = 0;

// ===== PROTOTYPES =====
void updateTimeDate();
void showMenu();
bool handleButtons();
void feedNow();
void checkLockToggle();
void handleSettings();
void saveSettings();
void loadSettings();
void autoFeeding();
bool rtcTimeIsValid();
void setRTCtimeOnBoot();
void autoFeedAnimation();
void drawTime(int x, int y, uint16_t textColor = WHITE, uint16_t bgColor = BLACK, uint8_t size = 2);
void drawDate(int x, int y, uint16_t textColor = WHITE, uint16_t bgColor = BLACK, uint8_t size = 1);

void setup()
{
  Serial.begin(9600);

  // Pin installation
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);
  pinMode(buttonEnter, INPUT_PULLUP);
  pinMode(buttonBack, INPUT_PULLUP);

  // Set motor speed
  stepperMotor.setSpeed(5);

  // Initialize OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("OLED INITIALIZED");
  display.display();
  delay(1000);

  // Initialize RTC
  if (!rtc.begin())
  {
    Serial.println("RTC not found!");
    while (1)
      ;
  }

  display.setCursor(0, 10);
  display.println("RTC INITIALIZED");
  display.display();
  delay(1000);

  // Create space for EEprom usage
  EEPROM.begin(512);
  display.setCursor(0, 20);
  display.println("EEPROM CREATED");
  display.display();
  delay(1000);

  // Load previous settings
  loadSettings();
  display.setCursor(0, 30);
  display.println("SETTINGS LOADED");
  display.display();
  delay(2000);

  if (!rtcTimeIsValid())
  {
    setRTCtimeOnBoot();
  }
  else
  {
    display.clearDisplay();

    display.setTextColor(1);
    display.setTextWrap(false);
    display.setCursor(11, 2);
    display.print("RTC Time i correct");

    display.setCursor(14, 53);
    display.print("Let's get started");

    display.drawBitmap(43, 8, image_checked_bits_startup, 42, 48, 1);

    display.display();
    delay(1000);
  }
}

void loop()
{
  updateTimeDate();
  checkLockToggle();

  if (isLocked)
    return;
  // returns true if a button is pressed
  bool buttonPressed = handleButtons();

  switch (appState)
  {
  case APP_SHOW_MENU:
    // refresh oled only if something is changed or a button is pressed
    if (CurrentTime.hour != lastDisplayedHour || CurrentTime.minute != lastDisplayedMinute || buttonPressed || menuSettings != previousIndex)
    {
      showMenu();
      lastDisplayedHour = CurrentTime.hour;
      lastDisplayedMinute = CurrentTime.minute;
      previousIndex = menuSettings;
    }
    break;

  case APP_FEED_NOW:
    feedNow();
    appState = APP_SHOW_MENU;
    showMenu();
    break;

  case APP_AUTO_FEED:
    autoFeeding();
    break;

  case APP_SETTINGS:
    handleSettings();
    appState = APP_SHOW_MENU;
    break;

  case APP_SET_TIME:
    setRTCtimeOnBoot();
    appState = APP_SHOW_MENU;
    break;

  default:
    break;
  }
}

void showMenu()
{
  switch (menuSettings)
  {
  case MANUAL_FEEDING:
    display.clearDisplay();

    drawDate(0, 0, WHITE, BLACK, 1);
    drawTime(97, 0, WHITE, BLACK, 1);

    display.drawLine(1, 53, 126, 53, 1);

    display.drawRoundRect(39, 14, 51, 36, 17, 1);
    display.drawBitmap(56, 24, image_music_next_bits, 16, 16, 1);

    display.drawLine(1, 9, 126, 9, 1);
    display.drawBitmap(5, 57, image_SmallArrowUp_bits, 7, 4, 1);
    display.drawBitmap(17, 57, image_SmallArrowDown_bits, 8, 4, 1);
    display.drawBitmap(107, 55, image_checked_bits, 7, 8, 1);
    display.drawBitmap(118, 55, image_Lock_bits, 7, 8, 1);

    display.display();

    break;

  case AUTO_FEEDING:
    display.clearDisplay();

    drawDate(0, 0, WHITE, BLACK, 1);
    drawTime(97, 0, WHITE, BLACK, 1);

    display.drawLine(1, 53, 126, 53, 1);

    display.drawRoundRect(39, 14, 51, 36, 17, 1);
    display.drawBitmap(51, 16, image_device_reset_bits_menu, 26, 32, 1);

    display.drawLine(1, 9, 126, 9, 1);
    display.drawBitmap(5, 57, image_SmallArrowUp_bits, 7, 4, 1);
    display.drawBitmap(17, 57, image_SmallArrowDown_bits, 8, 4, 1);
    display.drawBitmap(107, 55, image_checked_bits, 7, 8, 1);
    display.drawBitmap(118, 55, image_Lock_bits, 7, 8, 1);

    display.display();
    break;

  case SETTINGS:
    display.clearDisplay();

    drawDate(0, 0, WHITE, BLACK, 1);
    drawTime(97, 0, WHITE, BLACK, 1);

    display.drawLine(1, 53, 126, 53, 1);

    display.drawRoundRect(39, 14, 51, 36, 17, 1);
    display.drawBitmap(48, 16, image_menu_settings_gear_bits, 32, 32, 1);

    display.drawLine(1, 9, 126, 9, 1);
    display.drawBitmap(5, 57, image_SmallArrowUp_bits, 7, 4, 1);
    display.drawBitmap(17, 57, image_SmallArrowDown_bits, 8, 4, 1);
    display.drawBitmap(107, 55, image_checked_bits, 7, 8, 1);
    display.drawBitmap(118, 55, image_Lock_bits, 7, 8, 1);

    display.display();
    break;

  case DATE_TIME_SETTING:
    display.clearDisplay();

    drawDate(0, 0, WHITE, BLACK, 1);
    drawTime(97, 0, WHITE, BLACK, 1);

    display.drawLine(1, 53, 126, 53, 1);

    display.drawRoundRect(39, 14, 51, 36, 17, 1);
    display.drawBitmap(73, 24, image_menu_tools_bits, 14, 16, 1);
    display.drawBitmap(42, 17, image_set_clock_quarters_bits, 30, 32, 1);

    display.drawLine(1, 9, 126, 9, 1);
    display.drawBitmap(5, 57, image_SmallArrowUp_bits, 7, 4, 1);
    display.drawBitmap(17, 57, image_SmallArrowDown_bits, 8, 4, 1);
    display.drawBitmap(107, 55, image_checked_bits, 7, 8, 1);
    display.drawBitmap(118, 55, image_Lock_bits, 7, 8, 1);

    display.display();
    break;

  default:
    break;
  }
}

void setRTCtimeOnBoot()
{
  int year = 2025, month = 1, day = 1;
  int hour = 0, minute = 0;

  int cursor = 0; // 0=year,1=month,2=day,3=hour,4=minute
  int mode = 0;   // 0 = Date bitmap, 1 = Time bitmap
  bool done = false;

  while (!done)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);

    if (mode == 0)
      display.drawBitmap(42, 0, image_calendar_bits, 45, 48, 1);
    else
      display.drawBitmap(42, 0, image_clock_quarters_bits, 45, 48, 1);

    // date mode
    if (mode == 0)
    {
      display.setCursor(5, 48);
      if (cursor == 0)
        display.fillRect(4, 47, 24, 16, WHITE), display.setTextColor(BLACK);
      display.printf("%02d", day);
      display.setTextColor(WHITE);
      display.print("/");

      if (cursor == 1)
        display.fillRect(40, 47, 24, 16, WHITE), display.setTextColor(BLACK);
      display.printf("%02d", month);
      display.setTextColor(WHITE);
      display.print("/");

      if (cursor == 2)
        display.fillRect(76, 47, 48, 16, WHITE), display.setTextColor(BLACK);
      display.printf("%04d", year);
      display.setTextColor(WHITE);
    }
    // time mode
    else
    {
      display.setTextSize(2);
      display.setCursor(35, 48);
      if (cursor == 3)
        display.fillRect(33, 47, 27, 16, WHITE), display.setTextColor(BLACK);
      display.printf("%02d", hour);
      display.setTextColor(WHITE);
      display.print(":");

      if (cursor == 4)
        display.fillRect(68, 47, 27, 16, WHITE), display.setTextColor(BLACK);
      display.printf("%02d", minute);
      display.setTextColor(WHITE);
    }

    display.display();

    // button handling

    if (digitalRead(buttonDown) == LOW)
    {
      if (cursor == 0)
        day = (day < 31) ? day + 1 : 1;
      if (cursor == 1)
        month = (month < 12) ? month + 1 : 1;
      if (cursor == 2)
        year++;
      if (cursor == 3)
        hour = (hour + 1) % 24;
      if (cursor == 4)
        minute = (minute + 1) % 60;
      delay(200);
    }

    if (digitalRead(buttonUp) == LOW)
    {
      if (cursor == 0)
        day = (day > 1) ? day - 1 : 31;
      if (cursor == 1)
        month = (month > 1) ? month - 1 : 12;
      if (cursor == 2)
        year = (year > 2020) ? year - 1 : 2025;
      if (cursor == 3)
        hour = (hour == 0) ? 23 : hour - 1;
      if (cursor == 4)
        minute = (minute == 0) ? 59 : minute - 1;
      delay(200);
    }

    if (digitalRead(buttonEnter) == LOW)
    {
      cursor++;

      if (cursor == 3 && mode == 0)
      {
        // done with date switch to time
        mode = 1;
      }

      if (cursor > 4)
      {
        rtc.adjust(DateTime(year, month, day, hour, minute, 0));
        done = true;
      }
      delay(300);
    }

    if (digitalRead(buttonBack) == LOW)
    {
      rtc.adjust(DateTime(year, month, day, hour, minute, 0));
      done = true;
      delay(300);
    }
  }
}

void updateTimeDate()
{
  static unsigned long previous = 0;
  unsigned long current = millis();
  // update time and date every 1 second
  if (current - previous >= 1000)
  {
    previous = current;

    DateTime now = rtc.now();

    CurrentTime.hour = now.hour();
    CurrentTime.minute = now.minute();
    CurrentTime.seconds = now.second();

    if (CurrentDate.day != now.day() || CurrentDate.month != now.month() || CurrentDate.year != now.year())
    {
      CurrentDate.year = now.year();
      CurrentDate.month = now.month();
      CurrentDate.day = now.day();
    }
  }
}

// ===== BUTTON HANDLER =====
bool handleButtons()
{
  bool pressed = false;

  if (digitalRead(buttonUp) == LOW)
  {
    menuSettings = (MenuSettings)((menuSettings + MENU_COUNT - 1) % MENU_COUNT);
    pressed = true;
    delay(200);
  }

  if (digitalRead(buttonDown) == LOW)
  {
    menuSettings = (MenuSettings)((menuSettings + 1) % MENU_COUNT);
    pressed = true;
    delay(200);
  }

  if (digitalRead(buttonEnter) == LOW)
  {
    if (menuSettings == MANUAL_FEEDING)
      appState = APP_FEED_NOW;
    if (menuSettings == AUTO_FEEDING)
      appState = APP_AUTO_FEED;
    if (menuSettings == SETTINGS)
      appState = APP_SETTINGS;
    if (menuSettings == DATE_TIME_SETTING)
      appState = APP_SET_TIME;
    pressed = true;
    delay(200);
  }
  return pressed;
}

// ===== MANUAL FEEDING =====
void feedNow()
{
  display.clearDisplay();
  int fullBar = 64;
  int stepAmount = 512 * portions;

  display.drawRect(32, 51, 64, 10, 1);
  display.drawBitmap(35, 0, cat_image_bits, 51, 53, 1);
  for (int i = 0; i <= fullBar; i += 4)
  {
    display.fillRect(32, 51, i, 9, 1);
    display.display();
    if (stepAmount > 0)
    {
      stepperMotor.step(32 * portions);
      stepAmount -= 32 * portions;
    }
  }
}

// ===== AUTO FEEDING =====
void autoFeeding()
{
  // ===== BACK long press (EXIT AUTOFEED) =====
  static bool backPrev = HIGH;
  static unsigned long pressStart = 0;
  bool backNow = digitalRead(buttonBack);

  if (backNow == LOW)
  {
    if (backPrev == HIGH)
    {
      // when pressed
      pressStart = millis();
    }

    // long press
    if (millis() - pressStart > 1000)
    {
      af_states = AF_IDLE_STATE;
      appState = APP_SHOW_MENU;
      showMenu();
      return;
    }
  }

  backPrev = backNow;

  switch (af_states)
  {
  case AF_IDLE_STATE:
    autoFeedAnimation();

    if (millis() - afLastCheck >= 500)
    {
      afLastCheck = millis();
      af_states = AF_RESET_FLAGS;
    }
    break;

  case AF_RESET_FLAGS:
    if (CurrentDate.day != lastDay ||
        CurrentDate.month != lastMonth ||
        CurrentDate.year != lastYear)
    {
      for (int i = 0; i < frequency; i++)
        feedingDoneToday[i] = false;

      lastDay = CurrentDate.day;
      lastMonth = CurrentDate.month;
      lastYear = CurrentDate.year;
    }
    af_states = AF_CHECK_TIME_STATE;
    break;

  case AF_CHECK_TIME_STATE:
    for (int i = 0; i < frequency; i++)
    {
      if (!feedingDoneToday[i] &&
          CurrentTime.hour == feedingTimes[i][0] &&
          CurrentTime.minute == feedingTimes[i][1])
      {
        feedNow();
        feedingDoneToday[i] = true;
      }
    }
    af_states = AF_IDLE_STATE;
    break;
  }
}

// ===== AUTO FEED ANIMATION =====
void autoFeedAnimation()
{
  static unsigned long lastFrame = 0;
  const unsigned long frameSeparation = 500;

  unsigned long now = millis();

  if (now - lastFrame >= frameSeparation)
  {
    lastFrame = now;
    display.clearDisplay();
    drawTime(35, 1, WHITE, BLACK, 2);
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
}

void handleSettings()
{
  bool done = false;
  settingsState = SET_PORTIONS;
  settingsCursor = 0;    // Για SET_TIMES δείχνει ποιο feeding time ρυθμίζουμε
  settingsSubCursor = 0; // 0 = hours, 1 = minutes

  while (!done)
  {
    // ---- DRAW DISPLAY ----
    display.clearDisplay();
    drawDate(0, 0, WHITE, BLACK, 1);
    drawTime(95, 0, WHITE, BLACK, 1);

    switch (settingsState)
    {
    case SET_PORTIONS:
      display.drawBitmap(49, 10, image_food_bowl_1_bits, 30, 30, 1);

      display.setTextSize(2);
      display.setCursor(59, 46);
      display.println(portions);
      break;

    case SET_FREQUENCY:
      display.drawBitmap(49, 10, image_calendar_bits_settings, 30, 32, 1);

      display.setTextSize(2);
      display.setCursor(59, 46);
      display.println(frequency);
      break;

    case SET_TIMES:
      display.drawBitmap(25, 11, image_clock_alarm_bits, 30, 32, 1);
      display.setTextSize(1);
      display.setCursor(77, 21);
      display.print("feedings");
      display.setCursor(93, 34);
      display.printf("%d/%d",
                     settingsCursor + 1, timeCount);

      if (settingsSubCursor == 0)
      {
        display.fillRect(9, 44, 26, 18, WHITE), display.setTextColor(BLACK);
        display.setTextSize(2);
        display.setCursor(11, 46);
        display.printf("%02d", feedingTimes[settingsCursor][0]);
        display.setTextColor(WHITE);
        display.print(":");
        display.printf("%02d", feedingTimes[settingsCursor][1]);
      }
      else
      {
        display.setTextSize(2);
        display.setCursor(11, 46);
        display.setTextColor(WHITE);
        display.printf("%02d", feedingTimes[settingsCursor][0]);
        display.print(":");
        display.fillRect(46, 44, 26, 18, WHITE), display.setTextColor(BLACK);
        display.printf("%02d", feedingTimes[settingsCursor][1]);
      }
      break;

    case SETTINGS_DONE:
      saveSettings();
      showMenu();
      return;
    }
    display.display();
    delay(80);

    // ---- BUTTON HANDLING ----

    // === DOWN BUTTON ===
    if (digitalRead(buttonDown) == LOW)
    {
      if (settingsState == SET_PORTIONS)
      {
        if (portions < 10)
          portions++;
      }
      else if (settingsState == SET_FREQUENCY)
      {
        if (frequency < 4)
        {
          frequency++;
          timeCount = frequency;
        }
      }
      else if (settingsState == SET_TIMES)
      {
        if (settingsSubCursor == 0)
          feedingTimes[settingsCursor][0] = (feedingTimes[settingsCursor][0] + 1) % 24;
        else
          feedingTimes[settingsCursor][1] = (feedingTimes[settingsCursor][1] + 1) % 60;
      }
      delay(200);
    }

    // === UP BUTTON ===
    if (digitalRead(buttonUp) == LOW)
    {
      if (settingsState == SET_PORTIONS)
      {
        if (portions > 1)
          portions--;
      }
      else if (settingsState == SET_FREQUENCY)
      {
        if (frequency > 1)
        {
          frequency--;
          timeCount = frequency;
        }
      }
      else if (settingsState == SET_TIMES)
      {
        if (settingsSubCursor == 0)
          feedingTimes[settingsCursor][0] = (feedingTimes[settingsCursor][0] + 23) % 24;
        else
          feedingTimes[settingsCursor][1] = (feedingTimes[settingsCursor][1] + 59) % 60;
      }
      delay(200);
    }

    // === ENTER BUTTON ===
    if (digitalRead(buttonEnter) == LOW)
    {
      if (settingsState == SET_TIMES)
      {
        settingsSubCursor++;
        if (settingsSubCursor > 1)
        {
          settingsSubCursor = 0;
          settingsCursor++;

          if (settingsCursor >= timeCount)
            settingsState = SETTINGS_DONE;
        }
      }
      else
      {
        if (settingsState == SET_PORTIONS)
          settingsState = SET_FREQUENCY;
        else if (settingsState == SET_FREQUENCY)
          settingsState = SET_TIMES;
      }
      delay(250);
    }

    // === BACK BUTTON ===
    if (digitalRead(buttonBack) == LOW)
    {
      settingsState = SETTINGS_DONE;
      delay(250);
    }
  }
}

// ===== EEPROM =====
void saveSettings()
{
  EEPROM.write(0, portions);
  EEPROM.write(1, frequency);
  for (int i = 0; i < timeCount; i++)
  {
    EEPROM.write(2 + i * 2, feedingTimes[i][0]);
    EEPROM.write(2 + i * 2 + 1, feedingTimes[i][1]);
  }
  EEPROM.commit();
}

void loadSettings()
{
  portions = EEPROM.read(0);
  if (portions < 1 || portions > 10)
    portions = 3;

  frequency = EEPROM.read(1);
  if (frequency < 1 || frequency > 4)
    frequency = 2;

  timeCount = frequency; // ενημέρωση timeCount κατά φόρτωση

  for (int i = 0; i < timeCount; i++)
  {
    feedingTimes[i][0] = EEPROM.read(2 + i * 2);
    feedingTimes[i][1] = EEPROM.read(2 + i * 2 + 1);
    if (feedingTimes[i][0] > 23)
      feedingTimes[i][0] = 0;
    if (feedingTimes[i][1] > 59)
      feedingTimes[i][1] = 0;
  }
}

// ===== LOCK SYSTEM =====
void checkLockToggle()
{
  static bool prevButtonState = HIGH;
  bool buttonState = digitalRead(buttonBack);

  if (millis() - lastBackPress > pressTimeout)
    backPressCount = 0;

  if (buttonState == LOW && prevButtonState == HIGH)
  {
    backPressCount++;
    lastBackPress = millis();
    delay(150);
  }
  prevButtonState = buttonState;

  if (!isLocked && backPressCount >= 3)
  {
    isLocked = true;
    backPressCount = 0;
    for (int i = 0; i < FRAME_COUNT; i++)
    {
      display.clearDisplay();
      display.drawBitmap(40, 8, frames[i], 48, 48, 1);
      display.display();
    }
  }

  while (isLocked)
  {
    buttonState = digitalRead(buttonBack);
    if (buttonState == LOW && prevButtonState == HIGH)
    {
      backPressCount++;
      lastBackPress = millis();
      delay(150);
    }
    prevButtonState = buttonState;
    if (millis() - lastBackPress > pressTimeout)
      backPressCount = 0;
    if (backPressCount >= 3)
    {
      isLocked = false;
      backPressCount = 0;
      for (int i = 0; i < FRAME_COUNT; i++)
      {
        display.clearDisplay();
        display.drawBitmap(40, 8, frames[i], 48, 48, 1);
        display.display();
      }
    }
  }
}

void drawTime(int x, int y, uint16_t textColor, uint16_t bgColor, uint8_t size)
{
  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d", CurrentTime.hour, CurrentTime.minute);
  display.setTextColor(textColor, bgColor);
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(buf);
}

void drawDate(int x, int y, uint16_t textColor, uint16_t bgColor, uint8_t size)
{
  char buf[20];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d", CurrentDate.day, CurrentDate.month, CurrentDate.year);
  display.setTextColor(textColor, bgColor);
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(buf);
}

bool rtcTimeIsValid()
{
  if (rtc.lostPower())
    return false;

  DateTime now = rtc.now();

  if (now.year() < 2023)
    return false;

  return true;
}
