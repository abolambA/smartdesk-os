/*
 * SmartDesk OS — MCU Firmware v2
 * ────────────────────────────────
 * STM32U585 (Arduino UNO Q MCU side)
 *
 * NEW in v2:
 *   - 2" IPS ST7789 240×320 display via SPI
 *   - Animated face expressions reacting to focus state
 *   - 4 display modes (Face / Stats / Clock / Burnout)
 *   - Rotary encoder to cycle display modes
 *   - Capacitive touch button (TTP223) for session start/stop
 *   - Passive buzzer for audio feedback
 *
 * Pin map:
 *   ST7789 SCK   → D13 (SPI SCK)
 *   ST7789 MOSI  → D11 (SPI MOSI)
 *   ST7789 CS    → D10
 *   ST7789 DC    → D8
 *   ST7789 RST   → D5
 *   ST7789 BL    → D3  (PWM backlight — replaces privacy LED pin, move LED to D3_ALT)
 *
 *   Rotary ENC_A → A1
 *   Rotary ENC_B → A2
 *   Rotary SW    → A3  (encoder push button)
 *
 *   TTP223 touch → A4  (capacitive button output)
 *   Buzzer       → D12 (passive piezo)
 *
 *   BH1750    → SDA/SCL
 *   DHT22     → D4
 *   MAX4466   → A0
 *   NeoPixel  → D6
 *   Vibration → D9
 *   Relay     → D7
 *   Privacy SW→ D2  (interrupt)
 *   Privacy LED→ A5 (moved from D3 to free up PWM for backlight)
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <Bridge.h>

// ── Display pins ──────────────────────────────────────────────────────────────
#define TFT_CS    10
#define TFT_DC     8
#define TFT_RST    5
#define TFT_BL     3   // PWM backlight control

// ── Other pins ────────────────────────────────────────────────────────────────
#define PIN_DHT        4
#define PIN_SOUND      A0
#define PIN_NEOPIXEL   6
#define PIN_VIBRATION  9
#define PIN_RELAY      7
#define PIN_PRIVACY_SW 2
#define PIN_PRIVACY_LED A5
#define PIN_ENC_A      A1
#define PIN_ENC_B      A2
#define PIN_ENC_SW     A3
#define PIN_TOUCH      A4
#define PIN_BUZZER     12

// ── Sensor config ─────────────────────────────────────────────────────────────
#define DHT_TYPE       DHT22
#define NEOPIXEL_COUNT 12
#define SOUND_SAMPLES  10

// ── Read intervals (ms) ───────────────────────────────────────────────────────
#define INTERVAL_LIGHT    5000
#define INTERVAL_TEMP    30000
#define INTERVAL_SOUND    1000
#define INTERVAL_DISPLAY   100   // 10fps display refresh
#define INTERVAL_BRIDGE    200

// ── Display colors (RGB565) ───────────────────────────────────────────────────
#define C_BLACK    0x0000
#define C_WHITE    0xFFFF
#define C_BG       0x0841   // deep navy
#define C_ACCENT   0x07FF   // cyan
#define C_GREEN    0x07E0
#define C_ORANGE   0xFC60
#define C_RED      0xF800
#define C_YELLOW   0xFFE0
#define C_GRAY     0x4208
#define C_DARKGRAY 0x2104

// ── Objects ───────────────────────────────────────────────────────────────────
Adafruit_ST7789  tft  = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
BH1750           lightMeter;
DHT              dht(PIN_DHT, DHT_TYPE);
Adafruit_NeoPixel ring(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── State ─────────────────────────────────────────────────────────────────────
volatile bool privacySwitchChanged = false;
bool cameraEnabled   = true;
bool sessionActive   = false;

// Sensor values
float currentLux      = 400.0;
float currentTempC    = 23.0;
float currentHumidity = 50.0;
float currentSoundDb  = 35.0;

// Focus state from MPU
String focusState    = "absent";   // focused | distracted | absent
String postureState  = "upright";  // upright | slouching | craning
float  focusScore    = 0.0;        // 0.0–1.0
int    distractions  = 0;
int    sessionSecs   = 0;

// Display
uint8_t  displayMode    = 0;   // 0=face 1=stats 2=clock 3=burnout
uint8_t  prevDisplayMode = 255;
String   prevFocusState  = "";
unsigned long lastDisplayUpdate = 0;
unsigned long lastBlink         = 0;
bool          blinkOpen         = true;
uint8_t       blinkPhase        = 0;
unsigned long animTick          = 0;

// Rotary encoder
int  encLastA     = HIGH;
bool encSWPressed = false;
unsigned long encSWLast = 0;

// Touch button
bool  touchLast    = false;
unsigned long touchDebounce = 0;

// Bridge timers
unsigned long lastLightRead  = 0;
unsigned long lastTempRead   = 0;
unsigned long lastSoundRead  = 0;
unsigned long lastBridgeSync = 0;

// LED ring
String ledState = "off";
String ledColor = "cyan";
bool   pulsing  = false;
bool   flashing = false;
unsigned long pulseStart = 0;
unsigned long flashStart = 0;

// ── ISR ───────────────────────────────────────────────────────────────────────
void privacyISR() { privacySwitchChanged = true; }

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Bridge
  Bridge.begin();

  // I2C sensors
  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  dht.begin();

  // NeoPixel
  ring.begin();
  ring.setBrightness(80);
  ring.clear();
  ring.show();

  // Output pins
  pinMode(PIN_VIBRATION,   OUTPUT);
  pinMode(PIN_RELAY,       OUTPUT);
  pinMode(PIN_PRIVACY_LED, OUTPUT);
  pinMode(PIN_BUZZER,      OUTPUT);
  digitalWrite(PIN_RELAY,       LOW);
  digitalWrite(PIN_VIBRATION,   LOW);
  digitalWrite(PIN_PRIVACY_LED, HIGH);

  // Backlight PWM
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 200);  // ~80% brightness

  // Input pins
  pinMode(PIN_PRIVACY_SW, INPUT_PULLUP);
  pinMode(PIN_ENC_A,      INPUT_PULLUP);
  pinMode(PIN_ENC_B,      INPUT_PULLUP);
  pinMode(PIN_ENC_SW,     INPUT_PULLUP);
  pinMode(PIN_TOUCH,      INPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_PRIVACY_SW), privacyISR, CHANGE);

  // Display init
  tft.init(240, 320);
  tft.setRotation(0);
  tft.fillScreen(C_BG);

  // Boot sequence
  bootSequence();

  Serial.println(F("[MCU] SmartDesk OS v2 ready"));
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Privacy switch
  if (privacySwitchChanged) {
    privacySwitchChanged = false;
    cameraEnabled = (digitalRead(PIN_PRIVACY_SW) == HIGH);
    digitalWrite(PIN_PRIVACY_LED, cameraEnabled ? HIGH : LOW);
    Bridge.put("privacy_switch", cameraEnabled ? "0" : "1");
    playTone(cameraEnabled ? 880 : 440, 80);
  }

  // Light sensor
  if (now - lastLightRead >= INTERVAL_LIGHT) {
    lastLightRead = now;
    if (lightMeter.measurementReady()) {
      currentLux = lightMeter.readLightLevel();
      if (currentLux < 0) currentLux = 0;
      Bridge.put("light_lux", String(currentLux, 1));
    }
  }

  // DHT22
  if (now - lastTempRead >= INTERVAL_TEMP) {
    lastTempRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTempC    = t;
      currentHumidity = h;
      Bridge.put("temperature_c", String(t, 1));
      Bridge.put("humidity",      String(h, 1));
    }
  }

  // Sound
  if (now - lastSoundRead >= INTERVAL_SOUND) {
    lastSoundRead = now;
    long sum = 0;
    for (int i = 0; i < SOUND_SAMPLES; i++) {
      sum += analogRead(PIN_SOUND);
      delayMicroseconds(100);
    }
    currentSoundDb = 30.0 + (sum / SOUND_SAMPLES / 1023.0) * 60.0;
    Bridge.put("sound_db", String(currentSoundDb, 1));
  }

  // Bridge commands
  if (now - lastBridgeSync >= INTERVAL_BRIDGE) {
    lastBridgeSync = now;
    checkCommands();
    readMPUState();
  }

  // Rotary encoder
  handleEncoder();

  // Touch button (session start/stop)
  handleTouch();

  // LED ring animation
  updateLedRing();

  // Display refresh
  if (now - lastDisplayUpdate >= INTERVAL_DISPLAY) {
    lastDisplayUpdate = now;
    animTick++;
    updateDisplay();
  }
}

// ── Read state pushed by MPU ──────────────────────────────────────────────────
void readMPUState() {
  String fs = Bridge.get("focus_state");
  if (fs.length() > 0) focusState = fs;

  String ps = Bridge.get("posture_state");
  if (ps.length() > 0) postureState = ps;

  String sc = Bridge.get("focus_score");
  if (sc.length() > 0) focusScore = sc.toFloat();

  String dc = Bridge.get("distraction_count");
  if (dc.length() > 0) distractions = dc.toInt();

  String ss = Bridge.get("session_active");
  if (ss.length() > 0) sessionActive = (ss == "1");

  String secs = Bridge.get("session_secs");
  if (secs.length() > 0) sessionSecs = secs.toInt();
}

// ── Bridge commands ───────────────────────────────────────────────────────────
void checkCommands() {
  String ledCmd = Bridge.get("led_cmd");
  if (ledCmd.length() > 0) {
    int sep = ledCmd.indexOf(':');
    if (sep > 0) setLedState(ledCmd.substring(0, sep), ledCmd.substring(sep + 1));
    Bridge.put("led_cmd", "");
  }

  String vibrCmd = Bridge.get("vibrate_ms");
  if (vibrCmd.length() > 0) {
    int d = vibrCmd.toInt();
    if (d > 0 && d <= 2000) triggerVibration(d);
    Bridge.put("vibrate_ms", "");
  }

  String relayCmd = Bridge.get("relay");
  if (relayCmd.length() > 0) {
    digitalWrite(PIN_RELAY, relayCmd == "1" ? HIGH : LOW);
    Bridge.put("relay", "");
  }

  String blCmd = Bridge.get("backlight");
  if (blCmd.length() > 0) {
    analogWrite(TFT_BL, constrain(blCmd.toInt(), 0, 255));
    Bridge.put("backlight", "");
  }

  String modeCmd = Bridge.get("display_mode");
  if (modeCmd.length() > 0) {
    displayMode = constrain(modeCmd.toInt(), 0, 3);
    Bridge.put("display_mode", "");
  }
}

// ── Display update dispatcher ─────────────────────────────────────────────────
void updateDisplay() {
  bool modeChanged  = (displayMode   != prevDisplayMode);
  bool stateChanged = (focusState    != prevFocusState);

  if (modeChanged) {
    tft.fillScreen(C_BG);
    drawModeHeader();
    prevDisplayMode = displayMode;
  }

  switch (displayMode) {
    case 0: drawFaceMode(stateChanged);   break;
    case 1: drawStatsMode();              break;
    case 2: drawClockMode();              break;
    case 3: drawBurnoutMode();            break;
  }

  prevFocusState = focusState;
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 0 — FACE MODE
// Animated pixel-art expression that reacts to focus state
// ─────────────────────────────────────────────────────────────────────────────
void drawFaceMode(bool forceRedraw) {
  uint16_t faceColor;
  String   moodLabel;

  if (focusState == "focused") {
    faceColor = C_GREEN;
    moodLabel = "FOCUSED";
  } else if (focusState == "distracted") {
    faceColor = C_ORANGE;
    moodLabel = "DISTRACTED";
  } else {
    faceColor = C_GRAY;
    moodLabel = "ABSENT";
  }

  // Face circle — center at 120,140, radius 80
  int cx = 120, cy = 140, r = 80;

  if (forceRedraw) {
    tft.fillScreen(C_BG);
    drawModeHeader();
    // Face outline
    tft.fillCircle(cx, cy, r, C_DARKGRAY);
    tft.drawCircle(cx, cy, r,     faceColor);
    tft.drawCircle(cx, cy, r - 1, faceColor);
  }

  // Update face color ring if state changed
  if (forceRedraw) {
    tft.drawCircle(cx, cy, r,     faceColor);
    tft.drawCircle(cx, cy, r - 1, faceColor);
  }

  // ── Eyes ─────────────────────────────────────────────────────────────────
  // Blink logic: open for 3s, close for 0.15s
  bool shouldBlink = false;
  if (focusState == "focused") {
    // Normal slow blink every 3 seconds
    shouldBlink = ((animTick % 30) < 2);
  } else if (focusState == "distracted") {
    // Rapid nervous blink
    shouldBlink = ((animTick % 8) < 1);
  } else {
    // Absent = sleepy half-closed always
    shouldBlink = true;
  }

  int eyeY  = cy - 20;
  int eyeLX = cx - 28;
  int eyeRX = cx + 28;
  int eyeW  = 18, eyeH = 14;

  // Erase previous eyes
  tft.fillRect(eyeLX - eyeW/2 - 2, eyeY - eyeH/2 - 2, eyeW + 4, eyeH + 4, C_DARKGRAY);
  tft.fillRect(eyeRX - eyeW/2 - 2, eyeY - eyeH/2 - 2, eyeW + 4, eyeH + 4, C_DARKGRAY);

  if (focusState == "absent") {
    // Sleepy half-closed eyes — just a line with drooping lid
    tft.fillRect(eyeLX - eyeW/2, eyeY,         eyeW, eyeH/2 + 2, faceColor);
    tft.fillRect(eyeRX - eyeW/2, eyeY,         eyeW, eyeH/2 + 2, faceColor);
    tft.fillRect(eyeLX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH/2,    C_DARKGRAY);
    tft.fillRect(eyeRX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH/2,    C_DARKGRAY);
  } else if (shouldBlink) {
    // Closed eye — just a line
    tft.fillRect(eyeLX - eyeW/2, eyeY - 1, eyeW, 3, faceColor);
    tft.fillRect(eyeRX - eyeW/2, eyeY - 1, eyeW, 3, faceColor);
  } else {
    // Open eyes
    tft.fillRoundRect(eyeLX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH, 4, faceColor);
    tft.fillRoundRect(eyeRX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH, 4, faceColor);
    // Pupils
    tft.fillCircle(eyeLX + (focusState == "distracted" ? 3 : 0), eyeY, 4, C_BG);
    tft.fillCircle(eyeRX + (focusState == "distracted" ? 3 : 0), eyeY, 4, C_BG);
    // Eye shine
    tft.fillCircle(eyeLX - 4, eyeY - 3, 2, C_WHITE);
    tft.fillCircle(eyeRX - 4, eyeY - 3, 2, C_WHITE);
  }

  // ── Eyebrows ──────────────────────────────────────────────────────────────
  int browY = eyeY - eyeH/2 - 8;
  // Erase
  tft.fillRect(eyeLX - eyeW/2 - 2, browY - 5, eyeW + 4, 8, C_DARKGRAY);
  tft.fillRect(eyeRX - eyeW/2 - 2, browY - 5, eyeW + 4, 8, C_DARKGRAY);

  if (focusState == "focused") {
    // Relaxed flat brows
    tft.fillRoundRect(eyeLX - eyeW/2, browY, eyeW, 4, 2, faceColor);
    tft.fillRoundRect(eyeRX - eyeW/2, browY, eyeW, 4, 2, faceColor);
  } else if (focusState == "distracted") {
    // Raised inner brows (concerned/alert)
    for (int i = 0; i < eyeW; i++) {
      int yOff = (i < eyeW/2) ? (eyeW/2 - i) / 3 : 0;
      tft.drawPixel(eyeLX - eyeW/2 + i, browY - yOff, faceColor);
      tft.drawPixel(eyeLX - eyeW/2 + i, browY - yOff + 1, faceColor);
    }
    for (int i = 0; i < eyeW; i++) {
      int yOff = (i > eyeW/2) ? (i - eyeW/2) / 3 : 0;
      tft.drawPixel(eyeRX - eyeW/2 + i, browY - yOff, faceColor);
      tft.drawPixel(eyeRX - eyeW/2 + i, browY - yOff + 1, faceColor);
    }
  } else {
    // Absent — drooping brows
    for (int i = 0; i < eyeW; i++) {
      int yOff = (i > eyeW/2) ? (i - eyeW/2) / 3 : 0;
      tft.drawPixel(eyeLX - eyeW/2 + i, browY + yOff, faceColor);
      tft.drawPixel(eyeLX - eyeW/2 + i, browY + yOff + 1, faceColor);
      tft.drawPixel(eyeRX - eyeW/2 + i, browY + yOff, faceColor);
      tft.drawPixel(eyeRX - eyeW/2 + i, browY + yOff + 1, faceColor);
    }
  }

  // ── Mouth ────────────────────────────────────────────────────────────────
  int mouthY  = cy + 35;
  int mouthW  = 50;
  // Erase mouth area
  tft.fillRect(cx - mouthW/2 - 4, mouthY - 12, mouthW + 8, 24, C_DARKGRAY);

  if (focusState == "focused") {
    // Gentle smile — upward arc
    for (int i = -mouthW/2; i <= mouthW/2; i++) {
      int yOff = (i * i) / (mouthW * 2);
      tft.fillRect(cx + i - 1, mouthY + yOff - 2, 3, 4, faceColor);
    }
  } else if (focusState == "distracted") {
    // Slight frown
    for (int i = -mouthW/2; i <= mouthW/2; i++) {
      int yOff = -(i * i) / (mouthW * 2);
      tft.fillRect(cx + i - 1, mouthY + yOff, 3, 4, faceColor);
    }
  } else {
    // Absent — flat line mouth, slightly open (O shape)
    tft.fillEllipse(cx, mouthY, 16, 10, faceColor);
    tft.fillEllipse(cx, mouthY, 10, 6, C_DARKGRAY);
  }

  // ── ZZZ for absent ────────────────────────────────────────────────────────
  if (focusState == "absent") {
    tft.setTextColor(C_GRAY);
    tft.setTextSize(1);
    int zx = cx + 50;
    tft.setCursor(zx,      cy - 30); tft.print("z");
    tft.setCursor(zx + 6,  cy - 40); tft.print("z");
    tft.setCursor(zx + 14, cy - 52); tft.print("Z");
  }

  // ── Mood label ────────────────────────────────────────────────────────────
  tft.fillRect(20, 240, 200, 20, C_BG);
  tft.setTextColor(faceColor);
  tft.setTextSize(2);
  int labelX = 120 - (moodLabel.length() * 6);
  tft.setCursor(labelX, 242);
  tft.print(moodLabel);

  // ── Bottom stats strip ────────────────────────────────────────────────────
  drawBottomStrip();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 1 — STATS MODE
// ─────────────────────────────────────────────────────────────────────────────
void drawStatsMode() {
  static float lastScore = -1;
  if (abs(focusScore - lastScore) < 0.01 && focusState == prevFocusState) return;
  lastScore = focusScore;

  tft.fillRect(0, 40, 240, 250, C_BG);

  // Focus score arc gauge — center 120,150 radius 70
  int cx = 120, cy = 150, r = 70;
  drawArcGauge(cx, cy, r, focusScore, stateColor());

  // Score number
  tft.setTextSize(3);
  tft.setTextColor(stateColor());
  String scoreStr = String((int)(focusScore * 100)) + "%";
  int sw = scoreStr.length() * 18;
  tft.setCursor(cx - sw/2, cy - 18);
  tft.print(scoreStr);

  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(cx - 24, cy + 10);
  tft.print("FOCUS SCORE");

  // Session time
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(20, 238);
  tft.print(formatTime(sessionSecs));

  // Distractions
  tft.setTextColor(C_ORANGE);
  tft.setCursor(140, 238);
  tft.print("D:");
  tft.print(distractions);

  drawBottomStrip();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 2 — CLOCK MODE
// ─────────────────────────────────────────────────────────────────────────────
void drawClockMode() {
  // Update every 10 ticks (1 second)
  if (animTick % 10 != 0) return;

  // Get time from Bridge (MPU pushes it)
  String timeStr = Bridge.get("current_time");
  if (timeStr.length() == 0) timeStr = "--:--";

  tft.fillRect(0, 40, 240, 200, C_BG);

  // Large time display
  tft.setTextSize(4);
  tft.setTextColor(C_WHITE);
  int tw = timeStr.length() * 24;
  tft.setCursor(120 - tw/2, 100);
  tft.print(timeStr);

  // Thin focus score bar at bottom of clock area
  tft.drawRect(20, 190, 200, 8, C_GRAY);
  tft.fillRect(20, 190, (int)(200 * focusScore), 8, stateColor());

  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(20, 204);
  tft.print("FOCUS");
  tft.setCursor(170, 204);
  tft.print(String((int)(focusScore * 100)) + "%");

  // Sensor mini strip
  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(20, 225);
  tft.print(String(currentTempC, 1) + "C  ");
  tft.print(String(currentLux, 0) + "lx  ");
  tft.print(String(currentSoundDb, 0) + "dB");

  drawBottomStrip();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 3 — BURNOUT METER
// ─────────────────────────────────────────────────────────────────────────────
void drawBurnoutMode() {
  if (animTick % 20 != 0) return;

  String burnoutStr = Bridge.get("burnout_risk");
  float burnout = burnoutStr.length() > 0 ? burnoutStr.toFloat() : 0.0;

  tft.fillRect(0, 40, 240, 250, C_BG);

  tft.setTextColor(C_GRAY);
  tft.setTextSize(1);
  tft.setCursor(60, 55);
  tft.print("BURNOUT RISK");

  // Big percentage
  uint16_t bColor = burnout < 0.3 ? C_GREEN : burnout < 0.6 ? C_YELLOW : C_RED;
  tft.setTextSize(5);
  tft.setTextColor(bColor);
  String bStr = String((int)(burnout * 100)) + "%";
  int bw = bStr.length() * 30;
  tft.setCursor(120 - bw/2, 80);
  tft.print(bStr);

  // Animated battery-style bar
  int barX = 30, barY = 155, barW = 180, barH = 40;
  int tipW = 8, tipH = 20;
  // Battery outline
  tft.drawRect(barX, barY, barW, barH, C_WHITE);
  tft.fillRect(barX + barW, barY + (barH - tipH)/2, tipW, tipH, C_WHITE);
  // Fill
  int fillW = (int)((barW - 4) * burnout);
  tft.fillRect(barX + 2, barY + 2, barW - 4, barH - 4, C_BG);
  tft.fillRect(barX + 2, barY + 2, fillW, barH - 4, bColor);

  // Status label
  tft.setTextSize(2);
  tft.setTextColor(bColor);
  String bLabel = burnout < 0.3 ? "LOW RISK" : burnout < 0.6 ? "MODERATE" : "HIGH RISK";
  int blw = bLabel.length() * 12;
  tft.setCursor(120 - blw/2, 215);
  tft.print(bLabel);

  drawBottomStrip();
}

// ── Shared UI helpers ─────────────────────────────────────────────────────────
void drawModeHeader() {
  // Top bar
  tft.fillRect(0, 0, 240, 36, C_DARKGRAY);
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(1);
  tft.setCursor(8, 8);
  tft.print("SMARTDESK OS");

  // Mode indicator dots
  for (int i = 0; i < 4; i++) {
    uint16_t dotColor = (i == displayMode) ? C_ACCENT : C_GRAY;
    tft.fillCircle(160 + i * 14, 18, 4, dotColor);
  }

  // Camera state indicator
  tft.fillCircle(228, 18, 5, cameraEnabled ? C_GREEN : C_RED);
}

void drawBottomStrip() {
  // Bottom info bar
  tft.fillRect(0, 290, 240, 30, C_DARKGRAY);
  tft.setTextSize(1);

  // Session indicator
  tft.setTextColor(sessionActive ? C_GREEN : C_GRAY);
  tft.setCursor(8, 300);
  tft.print(sessionActive ? "● SESSION" : "○ NO SESSION");

  // Posture indicator
  uint16_t pColor = (postureState == "upright") ? C_GREEN : (postureState == "slouching") ? C_ORANGE : C_YELLOW;
  tft.setTextColor(pColor);
  tft.setCursor(140, 300);
  tft.print(postureState.substring(0, 7));
}

void drawArcGauge(int cx, int cy, int r, float value, uint16_t color) {
  // Draw background arc (gray) from -150° to +150°
  // Then draw value arc in color
  float startAngle = -150.0 * PI / 180.0;
  float totalAngle =  300.0 * PI / 180.0;
  int   steps = 120;

  for (int i = 0; i < steps; i++) {
    float angle = startAngle + (totalAngle * i / steps);
    int x = cx + (int)((r - 4) * cos(angle));
    int y = cy + (int)((r - 4) * sin(angle));
    uint16_t c = (i < (int)(steps * value)) ? color : C_DARKGRAY;
    tft.fillCircle(x, y, 4, c);
  }
}

uint16_t stateColor() {
  if (focusState == "focused")    return C_GREEN;
  if (focusState == "distracted") return C_ORANGE;
  return C_GRAY;
}

String formatTime(int secs) {
  int h = secs / 3600;
  int m = (secs % 3600) / 60;
  int s = secs % 60;
  char buf[9];
  if (h > 0) sprintf(buf, "%d:%02d:%02d", h, m, s);
  else        sprintf(buf, "%02d:%02d", m, s);
  return String(buf);
}

// ── Rotary encoder ────────────────────────────────────────────────────────────
void handleEncoder() {
  int a = digitalRead(PIN_ENC_A);
  if (a != encLastA && a == LOW) {
    int b = digitalRead(PIN_ENC_B);
    if (b != a) {
      // Clockwise → next mode
      displayMode = (displayMode + 1) % 4;
    } else {
      // Counter-clockwise → previous mode
      displayMode = (displayMode + 3) % 4;
    }
    tft.fillScreen(C_BG);
    drawModeHeader();
    playTone(1200, 30);
  }
  encLastA = a;

  // Encoder button — toggle backlight
  if (digitalRead(PIN_ENC_SW) == LOW && !encSWPressed) {
    encSWPressed = true;
    encSWLast = millis();
    static uint8_t bl = 200;
    bl = (bl == 200) ? 100 : (bl == 100) ? 30 : 200;
    analogWrite(TFT_BL, bl);
    playTone(800, 40);
  }
  if (digitalRead(PIN_ENC_SW) == HIGH) encSWPressed = false;
}

// ── Touch button ──────────────────────────────────────────────────────────────
void handleTouch() {
  bool touched = digitalRead(PIN_TOUCH) == HIGH;
  unsigned long now = millis();
  if (touched && !touchLast && (now - touchDebounce > 500)) {
    touchDebounce = now;
    // Toggle session via Bridge
    Bridge.put("touch_session_toggle", "1");
    playTone(sessionActive ? 600 : 900, 100);
    delay(80);
    playTone(sessionActive ? 400 : 1200, 80);
  }
  touchLast = touched;
}

// ── Audio feedback ────────────────────────────────────────────────────────────
void playTone(int freq, int durationMs) {
  tone(PIN_BUZZER, freq, durationMs);
}

void playSessionStart() {
  tone(PIN_BUZZER, 523, 100); delay(120);  // C
  tone(PIN_BUZZER, 659, 100); delay(120);  // E
  tone(PIN_BUZZER, 784, 150); delay(200);  // G
}

void playSessionEnd() {
  tone(PIN_BUZZER, 784, 100); delay(120);
  tone(PIN_BUZZER, 659, 100); delay(120);
  tone(PIN_BUZZER, 523, 200); delay(250);
}

void playDistraction() {
  tone(PIN_BUZZER, 440, 60); delay(80);
  tone(PIN_BUZZER, 440, 60);
}

// ── LED ring ──────────────────────────────────────────────────────────────────
void setLedState(String state, String color) {
  ledState = state; ledColor = color;
  pulsing = false; flashing = false;
  if (state == "off") { ring.clear(); ring.show(); }
  else if (state == "solid")  fillRing(getRingColor(color, 180));
  else if (state == "pulse") { pulsing = true; pulseStart = millis(); }
  else if (state == "flash") { flashing = true; flashStart = millis(); }
}

void updateLedRing() {
  unsigned long now = millis();
  if (pulsing) {
    unsigned long e = (now - pulseStart) % 1200;
    int br = (e < 600) ? map(e, 0, 600, 20, 200) : map(e, 600, 1200, 200, 20);
    fillRing(dimColor(getRingColor(ledColor, 255), br));
    if (now - pulseStart > 3600) { pulsing = false; fillRing(getRingColor(ledColor, 40)); }
  }
  if (flashing) {
    bool on = ((now - flashStart) % 400) < 200;
    if (on) fillRing(getRingColor(ledColor, 220)); else ring.clear();
    ring.show();
    if (now - flashStart > 1200) flashing = false;
  }
}

void fillRing(uint32_t color) {
  for (int i = 0; i < NEOPIXEL_COUNT; i++) ring.setPixelColor(i, color);
  ring.show();
}

uint32_t dimColor(uint32_t color, int br) {
  return ring.Color(((color>>16)&0xFF)*br/255, ((color>>8)&0xFF)*br/255, (color&0xFF)*br/255);
}

uint32_t getRingColor(String name, int br) {
  float s = br / 255.0;
  if (name == "cyan")   return ring.Color(0,        212*s, 255*s);
  if (name == "green")  return ring.Color(0,        255*s, 136*s);
  if (name == "orange") return ring.Color(255*s,    107*s, 0);
  if (name == "red")    return ring.Color(255*s,    0,     0);
  return ring.Color(0, 212*s, 255*s);
}

// ── Vibration ─────────────────────────────────────────────────────────────────
void triggerVibration(int durationMs) {
  digitalWrite(PIN_VIBRATION, HIGH);
  delay(durationMs);
  digitalWrite(PIN_VIBRATION, LOW);
}

// ── Boot sequence ──────────────────────────────────────────────────────────────
void bootSequence() {
  // Splash screen
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(28, 100);
  tft.print("SMARTDESK OS");
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(50, 130);
  tft.print("AI Productivity Hub");
  tft.setCursor(60, 150);
  tft.print("Initializing...");

  // NeoPixel boot sweep
  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    ring.setPixelColor(i, getRingColor("cyan", 160));
    ring.show();
    delay(40);
  }

  // Startup chime
  playTone(523, 80); delay(100);
  playTone(659, 80); delay(100);
  playTone(784, 80); delay(100);
  playTone(1047, 150);

  delay(400);

  // Fade ring out
  for (int b = 160; b >= 0; b -= 8) {
    fillRing(dimColor(getRingColor("cyan", 255), b));
    delay(20);
  }
  ring.clear(); ring.show();

  // Clear to face mode
  tft.fillScreen(C_BG);
  drawModeHeader();
  delay(200);
}
