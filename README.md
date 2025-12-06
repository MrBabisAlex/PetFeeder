

🐾 Automatic Pet Feeder (Arduino Project)

Ένα πλήρως αυτόνομο σύστημα αυτόματου ταΐσματος για κατοικίδια, βασισμένο σε Arduino, με οθόνη OLED, πραγματικό ρολόι RTC DS3231, και stepper motor για ακριβή διανομή τροφής.

Το σύστημα υποστηρίζει:

Μη αυτόματο τάισμα (Feed Now)

Αυτόματο τάισμα με έως 4 φορές/ημέρα

Ρύθμιση μερίδων (1–10)

Επεξεργασία ώρας και ημέρας στη συσκευή

Αποθήκευση ρυθμίσεων σε EEPROM

Λειτουργία οθόκλειδου (lock)

Γραφικές οθόνες και animations

📌 Features
✔️ 1. Manual Feeding

Με το πάτημα του Enter στο κύριο menu, η συσκευή ενεργοποιεί τον stepper και εμφανίζει progress bar.

✔️ 2. Auto Feeding

Έως 4 προγραμματισμένες ώρες

Αυτόματη επαναφορά timer καθημερινά

Animation λειτουργίας auto mode

✔️ 3. Settings Menu

Ρυθμίσεις που αποθηκεύονται μόνιμα:

Μερίδες (1–10)

Συχνότητα τάισματος (1–4 φορές/ημέρα)

Ώρες τάισματος (HH:MM)

✔️ 4. Real Time Clock (DS3231)

Η συσκευή εμφανίζει και ενημερώνει:

Ημερομηνία

Ώρα

Ρύθμιση ημερομηνίας/ώρας κατά το boot

✔️ 5. OLED UI

Custom bitmaps

Menu navigation

Animations

Ενδείξεις χρόνου/ημερομηνίας

🛠️ Hardware Requirements

Arduino (ESP32 προτείνεται, σύμφωνα με τους pins)

OLED 128×64 (SSD1306, I2C)

RTC DS3231

Stepper motor (με driver 28BYJ-48 / ULN2003 ή παρόμοιο)

4 κουμπιά: Up / Down / Enter / Back

Τροφοδοσία για κινητήρα & Arduino

EEPROM (πάνω στη συσκευή)

📂 Code Overview

Ο κώδικας περιλαμβάνει:

RTC Management

Stepper control

OLED rendering

Main menu

Auto feeding scheduler

Settings state machine

EEPROM save/load

Lock mechanism

Custom graphics (bitmaps)

Κύριες ενότητες:

setup() – αρχικοποίηση συσκευής

loop() – ενημέρωση ώρας, menu handling

feedNow() – manual feeding

autoFeeding() – αυτόματο πρόγραμμα

handleSettings() – ρύθμιση παραμέτρων

saveSettings() / loadSettings()

setRTCtimeOnBoot() – αρχική ρύθμιση χρόνου

▶️ How to Use

Ανάψε τη συσκευή → Ζητάει ρύθμιση ημερομηνίας/ώρας.

Με τα κουμπιά κατεβαίνεις στο menu:

Feed Now

Auto Feeding

Settings

Πατάς Enter για επιλογή.

Οι ρυθμίσεις αποθηκεύονται αυτόματα.

Πατώντας το Back 3 φορές γρήγορα → Κλειδώνει/Ξεκλειδώνει η συσκευή.

📦 Libraries Used

SPI.h

Wire.h

RTClib.h

Stepper.h

Adafruit_GFX.h

Adafruit_SSD1306.h

EEPROM.h

📌 Notes

Οι εικόνες (bitmaps) βρίσκονται στο loadingScreen.h και άλλα αρχεία.

Ο κώδικας είναι συμβατός με ESP32 (I2C pins 21/22).

Μπορεί να επεκταθεί με WiFi logging ή mobile notifications.
