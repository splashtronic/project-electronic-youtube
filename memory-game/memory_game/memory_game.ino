/*
  BRAIN BLITZ - Arduino Uno Memory Game
  ------------------------------------
  Hardware:
    - 4 push buttons : D2, D3, D4, D5  (other leg to GND, INPUT_PULLUP)
    - 4 LEDs         : D6, D7, D8, D9  (anode -> 1K resistor -> pin, cathode -> GND)
    - Passive buzzer : D10 (other leg to GND)
    - OLED SSD1306 128x64 I2C : SDA -> A4, SCL -> A5, VCC -> 5V/3.3V, GND -> GND

  Libraries (Library Manager):
    - Adafruit SSD1306
    - Adafruit GFX Library

  Pair 1 = Button D2 + LED D6, Pair 2 = D3 + D7, Pair 3 = D4 + D8, Pair 4 = D5 + D9
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// ===================== SETTINGS (easy to tweak) =====================
unsigned long LED_ON_MS      = 300;   // how long each LED is shown (0.8 s)
unsigned long LED_GAP_MS     = 50;   // pause between LEDs
unsigned long INPUT_TIMEOUT  = 5000;  // min wait per button press (ms), 0 = no timeout
const uint8_t MAX_ROUNDS     = 50;    // max rounds (indicators per round = round + 1)
const int     EEPROM_ADDR    = 0;     // top score stored here (1 byte)
// ====================================================================

const uint8_t BTN_PIN[4] = {2, 3, 4, 5};
const uint8_t LED_PIN[4] = {6, 7, 8, 9};
const uint8_t BUZZER_PIN = 10;

// A different tone for each LED/button pair
const uint16_t PAIR_TONE[4] = {262, 330, 392, 523};  // C4, E4, G4, C5

#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

uint8_t sequence[MAX_ROUNDS + 1];   // each round has (round + 1) indicators
uint8_t topScore = 0;

// ---------------------------------------------------------------
// Helpers: text + pads
// ---------------------------------------------------------------
void centerText(const __FlashStringHelper *txt, int y, uint8_t size) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, y);
  display.print(txt);
}

void centerTextBuf(const char *txt, int y, uint8_t size) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, y);
  display.print(txt);
}

// Four on-screen pads at the bottom, the active one is filled
void drawPads(int8_t active) {
  for (uint8_t i = 0; i < 4; i++) {
    int x = 4 + i * 31;
    if (i == active) {
      display.fillRoundRect(x, 48, 26, 16, 4, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRoundRect(x, 48, 26, 16, 4, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
    display.setTextSize(1);
    display.setCursor(x + 10, 52);
    display.print(i + 1);
  }
  display.setTextColor(SSD1306_WHITE);
}

// ---------------------------------------------------------------
// Sounds
// ---------------------------------------------------------------
void playNotes(const uint16_t *notes, const uint8_t *dur10ms, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    uint16_t d = dur10ms[i] * 10;
    if (notes[i] > 0) tone(BUZZER_PIN, notes[i], d);
    delay(d + 15);
  }
  noTone(BUZZER_PIN);
}

void soundStart() {  // "quiz begins" jingle
  const uint16_t n[] = {523, 659, 784, 659, 784, 1047};
  const uint8_t  d[] = {10, 10, 10, 8, 8, 30};
  playNotes(n, d, 6);
}

void soundSuccess() {  // round cleared
  const uint16_t n[] = {784, 988, 1175, 1568};
  const uint8_t  d[] = {8, 8, 8, 25};
  playNotes(n, d, 4);
}

void soundGameOver() {
  const uint16_t n[] = {392, 370, 349, 330, 294, 196};
  const uint8_t  d[] = {15, 15, 15, 15, 20, 60};
  playNotes(n, d, 6);
}

// ---------------------------------------------------------------
// Screens
// ---------------------------------------------------------------
void screenTitle(bool showPrompt) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  centerText(F("BRAIN"), 0, 2);
  centerText(F("BLITZ"), 17, 2);
  display.drawFastHLine(10, 36, 108, SSD1306_WHITE);

  char buf[16];
  snprintf(buf, sizeof(buf), "TOP SCORE: %u", topScore);
  centerTextBuf(buf, 40, 1);

  if (showPrompt) centerText(F("Press BTN 1 to start"), 54, 1);
  display.display();
}

void screenWatch(uint8_t round, int8_t active, uint8_t shown) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  char buf[16];
  snprintf(buf, sizeof(buf), "ROUND %u", round);
  centerTextBuf(buf, 0, 2);
  centerText(F("Watch closely..."), 22, 1);

  // progress dots
  uint8_t len = round + 1;
  uint8_t maxDots = min(len, (uint8_t)16);
  int startX = (SCREEN_W - maxDots * 8) / 2;
  for (uint8_t i = 0; i < maxDots; i++) {
    if (i < shown) display.fillCircle(startX + i * 8 + 3, 36, 2, SSD1306_WHITE);
    else           display.drawCircle(startX + i * 8 + 3, 36, 2, SSD1306_WHITE);
  }
  drawPads(active);
  display.display();
}

void screenInput(uint8_t round, uint8_t entered, int8_t active) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  char buf[16];
  snprintf(buf, sizeof(buf), "ROUND %u", round);
  centerTextBuf(buf, 0, 2);
  centerText(F("YOUR TURN! Repeat it"), 22, 1);

  snprintf(buf, sizeof(buf), "%u / %u", entered, (uint8_t)(round + 1));
  centerTextBuf(buf, 34, 1);
  drawPads(active);
  display.display();
}

void screenCool(uint8_t nextRound) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  centerText(F("Cool,"), 8, 3);
  char buf[24];
  snprintf(buf, sizeof(buf), "continue to round %u", nextRound);
  centerTextBuf(buf, 46, 1);
  display.display();
}

void screenGameOver(uint8_t cleared, bool newRecord) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  centerText(F("GAME OVER"), 2, 2);
  display.drawFastHLine(0, 22, 128, SSD1306_WHITE);

  char buf[24];
  snprintf(buf, sizeof(buf), "Rounds cleared: %u", cleared);
  centerTextBuf(buf, 29, 1);
  snprintf(buf, sizeof(buf), "Top score: %u", topScore);
  centerTextBuf(buf, 40, 1);
  if (newRecord) centerText(F("* NEW RECORD! *"), 54, 1);
  else           centerText(F("BTN 1: try again"), 54, 1);
  display.display();
}

// Celebration: bursting circles + fanfare, played together
void celebrate(uint8_t cleared) {
  const uint16_t fan[] = {523, 659, 784, 1047, 784, 1047, 1319, 1047,
                          1319, 1568, 1319, 1568, 2093, 1568, 2093, 2637};
  char buf[16];
  for (uint8_t f = 0; f < 16; f++) {
    display.clearDisplay();
    // fireworks
    const uint8_t cx[3] = {18, 110, 64};
    const uint8_t cy[3] = {16, 18, 50};
    for (uint8_t k = 0; k < 3; k++) {
      uint8_t r = (f * 2 + k * 7) % 22;
      display.drawCircle(cx[k], cy[k], r, SSD1306_WHITE);
      if (r > 5) display.drawCircle(cx[k], cy[k], r - 5, SSD1306_WHITE);
    }
    // blinking banner
    if (f % 2 == 0) {
      display.fillRect(0, 22, 128, 20, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    centerText(F("TOP SCORE!"), 25, 2);
    display.setTextColor(SSD1306_WHITE);
    snprintf(buf, sizeof(buf), "%u rounds!", cleared);
    centerTextBuf(buf, 54, 1);
    display.display();

    tone(BUZZER_PIN, fan[f], 90);
    delay(110);
  }
  noTone(BUZZER_PIN);
}

// ---------------------------------------------------------------
// Input
// ---------------------------------------------------------------
int8_t readButton() {
  for (uint8_t i = 0; i < 4; i++) if (digitalRead(BTN_PIN[i]) == LOW) return i;
  return -1;
}

// Returns pressed button index, or -1 on timeout
int8_t waitPress(unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (true) {
    int8_t b = readButton();
    if (b >= 0) {
      delay(20);                       // debounce
      if (readButton() == b) return b;
    }
    if (timeoutMs && millis() - t0 > timeoutMs) return -1;
  }
}

void waitRelease(uint8_t b) {
  while (digitalRead(BTN_PIN[b]) == LOW) { /* held */ }
  delay(20);
}

void allLedsOff() {
  for (uint8_t i = 0; i < 4; i++) digitalWrite(LED_PIN[i], LOW);
}

// ---------------------------------------------------------------
// Game logic
// ---------------------------------------------------------------
void waitForStart() {
  bool blink = true;
  unsigned long last = 0;
  screenTitle(true);
  while (true) {
    if (millis() - last > 500) {
      blink = !blink;
      screenTitle(blink);
      last = millis();
    }
    if (digitalRead(BTN_PIN[0]) == LOW) {
      delay(20);
      if (digitalRead(BTN_PIN[0]) == LOW) {
        randomSeed(micros());          // random seed from human timing
        waitRelease(0);
        return;
      }
    }
  }
}

void showSequence(uint8_t round) {
  screenWatch(round, -1, 0);
  delay(600);
  for (uint8_t i = 0; i < round + 1; i++) {
    uint8_t p = sequence[i];
    digitalWrite(LED_PIN[p], HIGH);
    tone(BUZZER_PIN, PAIR_TONE[p]);
    screenWatch(round, p, i + 1);
    delay(LED_ON_MS);
    digitalWrite(LED_PIN[p], LOW);
    noTone(BUZZER_PIN);
    screenWatch(round, -1, i + 1);
    delay(LED_GAP_MS);
  }
}

// Returns true if the player repeats the whole sequence correctly
bool inputPhase(uint8_t round) {
  screenInput(round, 0, -1);
  for (uint8_t i = 0; i < round + 1; i++) {
    int8_t b = waitPress(INPUT_TIMEOUT + (round * 1000));
    if (b < 0) return false;           // too slow

    digitalWrite(LED_PIN[b], HIGH);
    tone(BUZZER_PIN, PAIR_TONE[b]);
    screenInput(round, i + 1, b);
    waitRelease(b);
    digitalWrite(LED_PIN[b], LOW);
    noTone(BUZZER_PIN);

    if ((uint8_t)b != sequence[i]) return false;
    screenInput(round, i + 1, -1);
  }
  return true;
}

void playGame() {
  soundStart();
  uint8_t round = 1;
  bool celebrated = false;

  while (round <= MAX_ROUNDS) {
    // brand-new random sequence every round, length = round + 1
    for (uint8_t i = 0; i < round + 1; i++) sequence[i] = random(4);
    showSequence(round);

    if (!inputPhase(round)) break;     // wrong or timeout

    // Round cleared
    uint8_t cleared = round;
    if (cleared > topScore) {
      bool wasPositive = (topScore > 0);
      topScore = cleared;
      EEPROM.update(EEPROM_ADDR, topScore);
      if (wasPositive && !celebrated) {
        delay(300);
        celebrate(cleared);
        celebrated = true;
      }
    }
    if (round == MAX_ROUNDS) { round++; break; }
    soundSuccess();
    screenCool(round + 1);
    delay(1200);
    round++;
  }

  // Game over screen
  uint8_t cleared = round - 1;
  bool newRecord = (cleared > 0 && cleared == topScore && celebrated);
  allLedsOff();
  noTone(BUZZER_PIN);
  delay(300);
  screenGameOver(cleared, newRecord);
  soundGameOver();

  // wait for BTN 1 to go back to title
  while (digitalRead(BTN_PIN[0]) == HIGH) { }
  waitRelease(0);
}

// ---------------------------------------------------------------
void setup() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    pinMode(LED_PIN[i], OUTPUT);
    digitalWrite(LED_PIN[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // OLED not found: blink all LEDs forever
    while (true) {
      for (uint8_t i = 0; i < 4; i++) digitalWrite(LED_PIN[i], !digitalRead(LED_PIN[i]));
      delay(300);
    }
  }

  topScore = EEPROM.read(EEPROM_ADDR);
  if (topScore == 255 || topScore > MAX_ROUNDS) {  // blank EEPROM
    topScore = 0;
    EEPROM.update(EEPROM_ADDR, 0);
  }
}

void loop() {
  waitForStart();
  playGame();
}
