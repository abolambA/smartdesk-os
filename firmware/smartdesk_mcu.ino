/*
 * SmartDesk OS — MCU Firmware v3
 * ────────────────────────────────
 * Maximum personality — Eliq/Cozmo style interactive display
 *
 * NEW in v3:
 *   - Full idle animation system (breathing, random blinks, looking around)
 *   - Reaction animations (jump on distraction, bounce on session start)
 *   - Rich audio personality (distinct sounds for every event)
 *   - Smooth state transitions with easing
 *   - Achievement sounds (focus milestones)
 *   - Ambient mode when no session active
 *   - Posture nudge face reaction (wince animation)
 *   - Deep focus mode face (determined/intense expression)
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <Bridge.h>
#include <math.h>

// ── Pin map ───────────────────────────────────────────────────────────────────
#define TFT_CS     10
#define TFT_DC      8
#define TFT_RST     5
#define TFT_BL      3
#define PIN_DHT     4
#define PIN_SOUND   A0
#define PIN_NEOPIXEL 6
#define PIN_VIBRATION 9
#define PIN_RELAY   7
#define PIN_PRIVACY_SW  2
#define PIN_PRIVACY_LED A5
#define PIN_ENC_A   A1
#define PIN_ENC_B   A2
#define PIN_ENC_SW  A3
#define PIN_TOUCH   A4
#define PIN_BUZZER  12

// ── Hardware config ───────────────────────────────────────────────────────────
#define DHT_TYPE        DHT22
#define NEOPIXEL_COUNT  12
#define SOUND_SAMPLES   10

// ── Timing ────────────────────────────────────────────────────────────────────
#define INTERVAL_LIGHT    5000
#define INTERVAL_TEMP    30000
#define INTERVAL_SOUND    1000
#define INTERVAL_DISPLAY    50   // 20fps
#define INTERVAL_BRIDGE    200

// ── Colors (RGB565) ───────────────────────────────────────────────────────────
#define C_BLACK    0x0000
#define C_WHITE    0xFFFF
#define C_BG       0x0841
#define C_BG2      0x10A2
#define C_ACCENT   0x07FF
#define C_GREEN    0x07E0
#define C_LGREEN   0x47F0
#define C_ORANGE   0xFC60
#define C_RED      0xF800
#define C_YELLOW   0xFFE0
#define C_PURPLE   0x801F
#define C_PINK     0xF81F
#define C_GRAY     0x4208
#define C_DGRAY    0x2104
#define C_LGRAY    0x8410

// ── Objects ───────────────────────────────────────────────────────────────────
Adafruit_ST7789   tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
BH1750            lightMeter;
DHT               dht(PIN_DHT, DHT_TYPE);
Adafruit_NeoPixel ring(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── System state ──────────────────────────────────────────────────────────────
volatile bool privacySwitchChanged = false;
bool cameraEnabled  = true;
bool sessionActive  = false;
bool prevSessionActive = false;

float  currentLux      = 400.0;
float  currentTempC    = 23.0;
float  currentHumidity = 50.0;
float  currentSoundDb  = 35.0;

String focusState   = "absent";
String postureState = "upright";
String prevFocusState = "";
float  focusScore   = 0.0;
int    distractions = 0;
int    sessionSecs  = 0;
float  burnoutRisk  = 0.0;
int    prevDistractions = 0;
int    focusMilestone   = 0;  // last milestone achieved (25, 50, 75, 100%)

// ── Display state ─────────────────────────────────────────────────────────────
uint8_t  displayMode     = 0;
uint8_t  prevDisplayMode = 255;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBridgeSync    = 0;
uint32_t animTick = 0;

// ── Face animation state ──────────────────────────────────────────────────────

// Eye positions (for look-around animation)
int eyeOffsetX = 0, eyeOffsetY = 0;
int targetEyeX = 0, targetEyeY = 0;

// Blink state
bool      blinkClosed    = false;
uint32_t  nextBlinkTick  = 80;   // when to next blink
uint8_t   blinkDuration  = 3;    // ticks eye stays closed

// Breathing (subtle face scale)
float     breathPhase    = 0.0;

// Reaction animation
bool      reacting       = false;
uint8_t   reactionType   = 0;    // 0=jump, 1=shake, 2=bounce, 3=wince, 4=celebrate
uint8_t   reactionFrame  = 0;
uint8_t   reactionFrames = 0;
int       faceOffsetY    = 0;    // vertical shake offset
int       faceOffsetX    = 0;    // horizontal shake offset

// Look-around timer
uint32_t  nextLookTick   = 150;

// Deep focus (sustained focused > 5 min)
bool      deepFocus      = false;
uint32_t  focusedTicks   = 0;

// Achievement flash
bool      achievementActive = false;
uint8_t   achievementFrame  = 0;

// ── Encoder & touch ───────────────────────────────────────────────────────────
int  encLastA     = HIGH;
bool encSWPressed = false;
bool touchLast    = false;
unsigned long touchDebounce = 0;

// ── Sensor timers ─────────────────────────────────────────────────────────────
unsigned long lastLightRead = 0, lastTempRead = 0, lastSoundRead = 0;

// ── LED ring ──────────────────────────────────────────────────────────────────
String ledState = "off", ledColor = "cyan";
bool   pulsing = false, flashing = false;
unsigned long pulseStart = 0, flashStart = 0;
bool   ringBreathing = false;

// ── ISR ───────────────────────────────────────────────────────────────────────
void privacyISR() { privacySwitchChanged = true; }

// ─────────────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Bridge.begin();

  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  dht.begin();

  ring.begin();
  ring.setBrightness(80);
  ring.clear();
  ring.show();

  pinMode(PIN_VIBRATION,   OUTPUT);
  pinMode(PIN_RELAY,       OUTPUT);
  pinMode(PIN_PRIVACY_LED, OUTPUT);
  pinMode(PIN_BUZZER,      OUTPUT);
  pinMode(TFT_BL,          OUTPUT);
  digitalWrite(PIN_RELAY,       LOW);
  digitalWrite(PIN_VIBRATION,   LOW);
  digitalWrite(PIN_PRIVACY_LED, HIGH);
  analogWrite(TFT_BL, 200);

  pinMode(PIN_PRIVACY_SW, INPUT_PULLUP);
  pinMode(PIN_ENC_A,      INPUT_PULLUP);
  pinMode(PIN_ENC_B,      INPUT_PULLUP);
  pinMode(PIN_ENC_SW,     INPUT_PULLUP);
  pinMode(PIN_TOUCH,      INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_PRIVACY_SW), privacyISR, CHANGE);

  tft.init(240, 320);
  tft.setRotation(0);
  tft.fillScreen(C_BG);

  bootSequence();
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN LOOP
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Privacy switch
  if (privacySwitchChanged) {
    privacySwitchChanged = false;
    cameraEnabled = (digitalRead(PIN_PRIVACY_SW) == HIGH);
    digitalWrite(PIN_PRIVACY_LED, cameraEnabled ? HIGH : LOW);
    Bridge.put("privacy_switch", cameraEnabled ? "0" : "1");
    if (cameraEnabled) playSfx(SFX_CAMERA_ON);
    else               playSfx(SFX_CAMERA_OFF);
  }

  // Sensors
  if (now - lastLightRead >= INTERVAL_LIGHT) {
    lastLightRead = now;
    if (lightMeter.measurementReady()) {
      currentLux = lightMeter.readLightLevel();
      if (currentLux < 0) currentLux = 0;
      Bridge.put("light_lux", String(currentLux, 1));
    }
  }
  if (now - lastTempRead >= INTERVAL_TEMP) {
    lastTempRead = now;
    float t = dht.readTemperature(), h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTempC = t; currentHumidity = h;
      Bridge.put("temperature_c", String(t, 1));
      Bridge.put("humidity",      String(h, 1));
    }
  }
  if (now - lastSoundRead >= INTERVAL_SOUND) {
    lastSoundRead = now;
    long sum = 0;
    for (int i = 0; i < SOUND_SAMPLES; i++) { sum += analogRead(PIN_SOUND); delayMicroseconds(100); }
    currentSoundDb = 30.0 + (sum / SOUND_SAMPLES / 1023.0) * 60.0;
    Bridge.put("sound_db", String(currentSoundDb, 1));
  }

  // Bridge
  if (now - lastBridgeSync >= INTERVAL_BRIDGE) {
    lastBridgeSync = now;
    checkCommands();
    readMPUState();
  }

  // Input
  handleEncoder();
  handleTouch();

  // LED
  updateLedRing();

  // Display
  if (now - lastDisplayUpdate >= INTERVAL_DISPLAY) {
    lastDisplayUpdate = now;
    animTick++;
    updateAnimationState();
    updateDisplay();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// ANIMATION STATE MACHINE
// ─────────────────────────────────────────────────────────────────────────────
void updateAnimationState() {
  // ── Session start/stop reactions ─────────────────────────────────────────
  if (sessionActive && !prevSessionActive) {
    triggerReaction(2);  // bounce
    playSfx(SFX_SESSION_START);
    setLedState("pulse", "green");
    focusMilestone = 0;
  }
  if (!sessionActive && prevSessionActive) {
    triggerReaction(4);  // celebrate
    playSfx(SFX_SESSION_END);
    setLedState("off", "cyan");
  }
  prevSessionActive = sessionActive;

  // ── Focus state change reactions ─────────────────────────────────────────
  if (focusState != prevFocusState) {
    if (focusState == "distracted" && prevFocusState == "focused") {
      triggerReaction(0);  // jump (surprise)
      playSfx(SFX_DISTRACTED);
      setLedState("pulse", "orange");
    } else if (focusState == "focused" && prevFocusState == "distracted") {
      triggerReaction(2);  // bounce (happy)
      playSfx(SFX_REFOCUSED);
      setLedState("solid", "green");
    } else if (focusState == "absent") {
      setLedState("off", "cyan");
    }
  }

  // ── New distraction event ─────────────────────────────────────────────────
  if (distractions > prevDistractions && sessionActive) {
    triggerReaction(1);   // shake
    prevDistractions = distractions;
  }

  // ── Focus milestones (25 / 50 / 75 / 100%) ───────────────────────────────
  if (sessionActive && focusScore > 0) {
    int milestone = (int)(focusScore * 100 / 25) * 25;
    if (milestone > focusMilestone && milestone <= 100) {
      focusMilestone = milestone;
      achievementActive = true;
      achievementFrame  = 0;
      playSfx(SFX_MILESTONE);
    }
  }

  // ── Deep focus detection (focused > 5 min continuous) ────────────────────
  if (focusState == "focused") {
    focusedTicks++;
    deepFocus = (focusedTicks > 6000);  // 5min at 20fps
  } else {
    focusedTicks = 0;
    deepFocus    = false;
  }

  // ── Idle look-around ─────────────────────────────────────────────────────
  if (animTick >= nextLookTick && !reacting) {
    nextLookTick = animTick + random(100, 400);
    if (focusState == "absent" || !sessionActive) {
      // Wander around more
      targetEyeX = random(-8, 9);
      targetEyeY = random(-5, 6);
    } else if (focusState == "focused") {
      // Mostly centered, slight drift
      targetEyeX = random(-3, 4);
      targetEyeY = random(-2, 3);
    } else {
      // Distracted — look around nervously
      targetEyeX = random(-10, 11);
      targetEyeY = random(-6, 7);
    }
  }

  // Smooth eye movement toward target
  eyeOffsetX += (targetEyeX - eyeOffsetX) > 0 ? 1 : (targetEyeX - eyeOffsetX) < 0 ? -1 : 0;
  eyeOffsetY += (targetEyeY - eyeOffsetY) > 0 ? 1 : (targetEyeY - eyeOffsetY) < 0 ? -1 : 0;

  // ── Blink logic ───────────────────────────────────────────────────────────
  if (!blinkClosed && animTick >= nextBlinkTick) {
    blinkClosed = true;
    blinkDuration = (focusState == "distracted") ? 1 : 3;
  }
  if (blinkClosed) {
    blinkDuration--;
    if (blinkDuration <= 0) {
      blinkClosed  = false;
      nextBlinkTick = animTick + random(
        focusState == "distracted" ? 15 : 40,
        focusState == "distracted" ? 30 : 120
      );
    }
  }

  // ── Breathing ─────────────────────────────────────────────────────────────
  breathPhase += (focusState == "focused") ? 0.04 : 0.08;
  if (breathPhase > TWO_PI) breathPhase -= TWO_PI;

  // ── Reaction frames ───────────────────────────────────────────────────────
  if (reacting) {
    reactionFrame++;
    switch (reactionType) {
      case 0: // jump — bounce up then down
        faceOffsetY = (reactionFrame < 5) ? -reactionFrame * 4
                    : (reactionFrame < 10) ? -(10 - reactionFrame) * 4 : 0;
        break;
      case 1: // shake — horizontal
        faceOffsetX = (reactionFrame % 4 < 2) ? -6 : 6;
        break;
      case 2: // bounce — small up/down
        faceOffsetY = (reactionFrame < 4) ? -reactionFrame * 2
                    : (reactionFrame < 8) ? -(8 - reactionFrame) * 2 : 0;
        break;
      case 3: // wince — squint + backward
        faceOffsetY = (reactionFrame < 6) ? reactionFrame * 2 : 0;
        break;
      case 4: // celebrate — repeated small bounces
        faceOffsetY = (reactionFrame % 6 < 3) ? -4 : 0;
        break;
    }
    if (reactionFrame >= reactionFrames) {
      reacting    = false;
      faceOffsetX = 0;
      faceOffsetY = 0;
    }
  }

  // ── Achievement overlay ───────────────────────────────────────────────────
  if (achievementActive) {
    achievementFrame++;
    if (achievementFrame > 40) achievementActive = false;
  }
}

void triggerReaction(uint8_t type) {
  reacting       = true;
  reactionType   = type;
  reactionFrame  = 0;
  reactionFrames = (type == 4) ? 30 : 12;
  faceOffsetX = 0; faceOffsetY = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// DISPLAY DISPATCHER
// ─────────────────────────────────────────────────────────────────────────────
void updateDisplay() {
  bool modeChanged = (displayMode != prevDisplayMode);
  if (modeChanged) {
    tft.fillScreen(C_BG);
    drawModeHeader();
    prevDisplayMode = displayMode;
  }

  switch (displayMode) {
    case 0: drawFaceMode();   break;
    case 1: drawStatsMode();  break;
    case 2: drawClockMode();  break;
    case 3: drawBurnoutMode();break;
  }

  prevFocusState = focusState;
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 0 — FACE MODE (fully animated)
// ─────────────────────────────────────────────────────────────────────────────
void drawFaceMode() {
  // Face center — apply reaction offsets
  int cx = 120 + faceOffsetX;
  int cy = 145 + faceOffsetY;

  // Breathing scale (subtle — just affects eye/mouth positions slightly)
  float breathScale = 1.0 + sin(breathPhase) * 0.015;

  uint16_t faceColor;
  if (deepFocus)                         faceColor = C_ACCENT;
  else if (focusState == "focused")      faceColor = C_GREEN;
  else if (focusState == "distracted")   faceColor = C_ORANGE;
  else                                   faceColor = C_GRAY;

  // Only redraw background when state or mode changes
  static String lastState = "";
  static bool   lastDeep  = false;
  bool needFullRedraw = (focusState != lastState || deepFocus != lastDeep || displayMode != prevDisplayMode);
  if (needFullRedraw) {
    tft.fillRect(0, 36, 240, 248, C_BG);
    // Face bg circle
    tft.fillCircle(cx, cy, 84, C_DGRAY);
    lastState = focusState;
    lastDeep  = deepFocus;
  }

  // Face glow ring — color changes with state
  tft.drawCircle(cx, cy, 84, faceColor);
  tft.drawCircle(cx, cy, 83, faceColor);
  if (deepFocus) tft.drawCircle(cx, cy, 82, faceColor);  // extra ring for deep focus

  // ── Eyes ─────────────────────────────────────────────────────────────────
  int eyeY  = cy - 18 + eyeOffsetY;
  int eyeLX = cx - 26 + eyeOffsetX;
  int eyeRX = cx + 26 + eyeOffsetX;
  int eyeW  = 20, eyeH = 14;

  // Erase eye area
  tft.fillRect(eyeLX - eyeW/2 - 4, eyeY - eyeH/2 - 6, eyeW + 8, eyeH + 12, C_DGRAY);
  tft.fillRect(eyeRX - eyeW/2 - 4, eyeY - eyeH/2 - 6, eyeW + 8, eyeH + 12, C_DGRAY);

  if (focusState == "absent") {
    // Sleepy half-closed: top half of eye covered
    tft.fillRoundRect(eyeLX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH, 4, faceColor);
    tft.fillRect(eyeLX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH/2 + 2, C_DGRAY);
    tft.fillRoundRect(eyeRX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH, 4, faceColor);
    tft.fillRect(eyeRX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH/2 + 2, C_DGRAY);
  } else if (blinkClosed) {
    // Blink — just a horizontal line
    tft.fillRect(eyeLX - eyeW/2, eyeY - 1, eyeW, 3, faceColor);
    tft.fillRect(eyeRX - eyeW/2, eyeY - 1, eyeW, 3, faceColor);
  } else if (deepFocus) {
    // Deep focus — determined squint (slightly narrowed)
    int sH = eyeH - 4;
    tft.fillRoundRect(eyeLX - eyeW/2, eyeY - sH/2, eyeW, sH, 3, faceColor);
    tft.fillRoundRect(eyeRX - eyeW/2, eyeY - sH/2, eyeW, sH, 3, faceColor);
    tft.fillCircle(eyeLX, eyeY, 5, C_BG);
    tft.fillCircle(eyeRX, eyeY, 5, C_BG);
    tft.fillCircle(eyeLX + 1, eyeY - 1, 2, C_WHITE);
    tft.fillCircle(eyeRX + 1, eyeY - 1, 2, C_WHITE);
  } else {
    // Normal open eyes
    tft.fillRoundRect(eyeLX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH, 5, faceColor);
    tft.fillRoundRect(eyeRX - eyeW/2, eyeY - eyeH/2, eyeW, eyeH, 5, faceColor);
    // Pupils with eye offset
    int pupilX = (focusState == "distracted") ? 4 : 0;
    tft.fillCircle(eyeLX + pupilX, eyeY, 5, C_BG);
    tft.fillCircle(eyeRX + pupilX, eyeY, 5, C_BG);
    // Shine
    tft.fillCircle(eyeLX - 3 + pupilX, eyeY - 2, 2, C_WHITE);
    tft.fillCircle(eyeRX - 3 + pupilX, eyeY - 2, 2, C_WHITE);
  }

  // ── Eyebrows ──────────────────────────────────────────────────────────────
  int browY = eyeY - eyeH/2 - 10;
  tft.fillRect(eyeLX - eyeW/2 - 2, browY - 4, eyeW + 4, 8, C_DGRAY);
  tft.fillRect(eyeRX - eyeW/2 - 2, browY - 4, eyeW + 4, 8, C_DGRAY);

  if (deepFocus) {
    // Furrowed inward brows — V shape
    tft.drawLine(eyeLX - eyeW/2, browY + 3, eyeLX + eyeW/2, browY, faceColor);
    tft.drawLine(eyeLX - eyeW/2, browY + 4, eyeLX + eyeW/2, browY + 1, faceColor);
    tft.drawLine(eyeRX - eyeW/2, browY, eyeRX + eyeW/2, browY + 3, faceColor);
    tft.drawLine(eyeRX - eyeW/2, browY + 1, eyeRX + eyeW/2, browY + 4, faceColor);
  } else if (focusState == "focused") {
    tft.fillRoundRect(eyeLX - eyeW/2, browY, eyeW, 4, 2, faceColor);
    tft.fillRoundRect(eyeRX - eyeW/2, browY, eyeW, 4, 2, faceColor);
  } else if (focusState == "distracted") {
    // Raised outer brows
    tft.drawLine(eyeLX - eyeW/2, browY + 3, eyeLX + eyeW/2, browY, faceColor);
    tft.drawLine(eyeLX - eyeW/2, browY + 4, eyeLX + eyeW/2, browY + 1, faceColor);
    tft.drawLine(eyeRX - eyeW/2, browY, eyeRX + eyeW/2, browY + 3, faceColor);
    tft.drawLine(eyeRX - eyeW/2, browY + 1, eyeRX + eyeW/2, browY + 4, faceColor);
  } else {
    // Absent — drooping
    tft.drawLine(eyeLX - eyeW/2, browY, eyeLX + eyeW/2, browY + 4, faceColor);
    tft.drawLine(eyeLX - eyeW/2, browY + 1, eyeLX + eyeW/2, browY + 5, faceColor);
    tft.drawLine(eyeRX - eyeW/2, browY + 4, eyeRX + eyeW/2, browY, faceColor);
    tft.drawLine(eyeRX - eyeW/2, browY + 5, eyeRX + eyeW/2, browY + 1, faceColor);
  }

  // ── Mouth ─────────────────────────────────────────────────────────────────
  int mouthY = cy + 40;
  int mouthW = 48;
  tft.fillRect(cx - mouthW/2 - 4, mouthY - 14, mouthW + 8, 28, C_DGRAY);

  if (deepFocus) {
    // Straight determined line
    tft.fillRoundRect(cx - mouthW/2, mouthY - 2, mouthW, 5, 2, faceColor);
  } else if (focusState == "focused") {
    // Smile arc
    for (int i = -mouthW/2; i <= mouthW/2; i++) {
      int yo = (i * i) / (mouthW + 8);
      tft.fillRect(cx + i - 1, mouthY + yo, 3, 4, faceColor);
    }
    // Cheek dots
    tft.fillCircle(cx - 38, mouthY - 5, 5, 0xF8D0);
    tft.fillCircle(cx + 38, mouthY - 5, 5, 0xF8D0);
  } else if (focusState == "distracted") {
    // Frown
    for (int i = -mouthW/2; i <= mouthW/2; i++) {
      int yo = -(i * i) / (mouthW + 8);
      tft.fillRect(cx + i - 1, mouthY + yo + 8, 3, 4, faceColor);
    }
  } else {
    // Absent — small O mouth
    tft.fillEllipse(cx, mouthY, 14, 10, faceColor);
    tft.fillEllipse(cx, mouthY, 8, 5, C_DGRAY);
    // ZZZ
    tft.setTextColor(C_LGRAY);
    tft.setTextSize(1);
    tft.setCursor(cx + 52, cy - 28); tft.print("z");
    tft.setCursor(cx + 58, cy - 38); tft.print("z");
    tft.setCursor(cx + 66, cy - 50); tft.print("Z");
  }

  // ── Achievement overlay ───────────────────────────────────────────────────
  if (achievementActive && achievementFrame < 40) {
    uint16_t aColor = (achievementFrame % 6 < 3) ? C_YELLOW : C_ORANGE;
    tft.setTextColor(aColor);
    tft.setTextSize(1);
    String aText = String(focusMilestone) + "% FOCUS!";
    tft.setCursor(120 - aText.length() * 3, cy - 95);
    tft.print(aText);
    // Star particles
    for (int i = 0; i < 4; i++) {
      int px = 40 + i * 50 + (achievementFrame % 3);
      int py = cy - 90 + (i % 2 == 0 ? -achievementFrame/4 : achievementFrame/4);
      tft.fillCircle(px, py, 2, aColor);
    }
  }

  // ── Mood label ────────────────────────────────────────────────────────────
  tft.fillRect(0, 248, 240, 22, C_BG);
  tft.setTextColor(faceColor);
  tft.setTextSize(2);
  String moodLabel = deepFocus ? "DEEP FOCUS"
    : focusState == "focused"    ? "FOCUSED"
    : focusState == "distracted" ? "DISTRACTED"
    : "ABSENT";
  int lx = 120 - (moodLabel.length() * 6);
  tft.setCursor(lx, 250);
  tft.print(moodLabel);

  drawBottomStrip();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 1 — STATS MODE
// ─────────────────────────────────────────────────────────────────────────────
void drawStatsMode() {
  static float lastScore = -1;
  if (abs(focusScore - lastScore) < 0.005) return;
  lastScore = focusScore;

  tft.fillRect(0, 36, 240, 248, C_BG);

  uint16_t sc = stateColor();
  int cx = 120, cy = 155, r = 72;
  drawArcGauge(cx, cy, r, focusScore, sc);

  tft.setTextSize(3);
  tft.setTextColor(sc);
  String sStr = String((int)(focusScore * 100)) + "%";
  tft.setCursor(cx - sStr.length() * 9, cy - 16);
  tft.print(sStr);
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(cx - 24, cy + 10);
  tft.print("FOCUS SCORE");

  tft.setTextColor(C_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(16, 244);
  tft.print(formatTime(sessionSecs));

  tft.setTextColor(C_ORANGE);
  tft.setCursor(144, 244);
  tft.print("D:"); tft.print(distractions);

  drawBottomStrip();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 2 — CLOCK MODE
// ─────────────────────────────────────────────────────────────────────────────
void drawClockMode() {
  if (animTick % 20 != 0) return;
  String timeStr = Bridge.get("current_time");
  if (timeStr.length() == 0) timeStr = "--:--";

  tft.fillRect(0, 36, 240, 248, C_BG);
  tft.setTextSize(4);
  tft.setTextColor(C_WHITE);
  int tw = timeStr.length() * 24;
  tft.setCursor(120 - tw/2, 105);
  tft.print(timeStr);

  tft.drawRect(20, 192, 200, 8, C_DGRAY);
  tft.fillRect(20, 192, (int)(200 * focusScore), 8, stateColor());
  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(20, 206); tft.print("FOCUS");
  tft.setCursor(170, 206); tft.print(String((int)(focusScore*100)) + "%");

  tft.setTextColor(C_DGRAY);
  tft.setCursor(20, 226);
  tft.print(String(currentTempC, 1) + "C  " + String(currentLux, 0) + "lx  " + String(currentSoundDb, 0) + "dB");

  drawBottomStrip();
}

// ─────────────────────────────────────────────────────────────────────────────
// MODE 3 — BURNOUT METER
// ─────────────────────────────────────────────────────────────────────────────
void drawBurnoutMode() {
  if (animTick % 20 != 0) return;

  tft.fillRect(0, 36, 240, 248, C_BG);
  uint16_t bc = burnoutRisk < 0.3 ? C_GREEN : burnoutRisk < 0.6 ? C_YELLOW : C_RED;

  tft.setTextColor(C_GRAY); tft.setTextSize(1);
  tft.setCursor(60, 52); tft.print("BURNOUT RISK");

  tft.setTextSize(5); tft.setTextColor(bc);
  String bStr = String((int)(burnoutRisk * 100)) + "%";
  tft.setCursor(120 - bStr.length() * 15, 80);
  tft.print(bStr);

  int bx = 30, by = 158, bw = 180, bh = 40, tw2 = 8, th = 20;
  tft.drawRect(bx, by, bw, bh, C_WHITE);
  tft.fillRect(bx + bw, by + (bh-th)/2, tw2, th, C_WHITE);
  tft.fillRect(bx+2, by+2, bw-4, bh-4, C_BG);

  // Animated fill
  int fillW = (int)((bw-4) * burnoutRisk);
  tft.fillRect(bx+2, by+2, fillW, bh-4, bc);

  // Pulsing dot at fill edge
  if (animTick % 10 < 5 && fillW > 0)
    tft.fillCircle(bx + 2 + fillW, by + bh/2, 4, C_WHITE);

  String bLbl = burnoutRisk < 0.3 ? "LOW RISK" : burnoutRisk < 0.6 ? "MODERATE" : "HIGH RISK";
  tft.setTextSize(2); tft.setTextColor(bc);
  tft.setCursor(120 - bLbl.length()*6, 218);
  tft.print(bLbl);

  drawBottomStrip();
}

// ── Shared UI ─────────────────────────────────────────────────────────────────
void drawModeHeader() {
  tft.fillRect(0, 0, 240, 36, C_DGRAY);
  tft.setTextColor(C_ACCENT); tft.setTextSize(1);
  tft.setCursor(8, 6); tft.print("SMARTDESK OS");

  // Mode dots
  String labels[] = {"FACE","STAT","TIME","RISK"};
  for (int i = 0; i < 4; i++) {
    if (i == displayMode) {
      tft.fillRoundRect(148 + i*24, 10, 20, 14, 4, C_ACCENT);
      tft.setTextColor(C_BG);
    } else {
      tft.drawRoundRect(148 + i*24, 10, 20, 14, 4, C_DGRAY);
      tft.setTextColor(C_GRAY);
    }
    tft.setTextSize(1);
    // Just dot for non-active modes
    if (i == displayMode) {
      tft.setCursor(150 + i*24, 13);
      tft.print(labels[i].substring(0,2));
    } else {
      tft.fillCircle(158 + i*24, 17, 3, C_GRAY);
    }
  }

  // Camera dot
  tft.fillCircle(232, 18, 5, cameraEnabled ? C_GREEN : C_RED);
}

void drawBottomStrip() {
  tft.fillRect(0, 288, 240, 32, C_DGRAY);
  tft.setTextSize(1);
  tft.setTextColor(sessionActive ? C_GREEN : C_GRAY);
  tft.setCursor(8, 298);
  tft.print(sessionActive ? "REC " + formatTime(sessionSecs) : "NO SESSION");

  uint16_t pc = (postureState == "upright") ? C_GREEN
              : (postureState == "slouching") ? C_ORANGE : C_YELLOW;
  tft.setTextColor(pc);
  tft.setCursor(148, 298);
  tft.print(postureState.substring(0, 8));
}

void drawArcGauge(int cx, int cy, int r, float val, uint16_t col) {
  float sa = -150.0 * PI / 180.0, ta = 300.0 * PI / 180.0;
  int   steps = 120;
  for (int i = 0; i < steps; i++) {
    float angle = sa + (ta * i / steps);
    int x = cx + (int)((r-4)*cos(angle));
    int y = cy + (int)((r-4)*sin(angle));
    tft.fillCircle(x, y, 4, (i < (int)(steps*val)) ? col : C_DGRAY);
  }
}

uint16_t stateColor() {
  if (focusState == "focused")    return C_GREEN;
  if (focusState == "distracted") return C_ORANGE;
  return C_GRAY;
}

String formatTime(int secs) {
  int h = secs/3600, m = (secs%3600)/60, s = secs%60;
  char buf[9];
  if (h > 0) sprintf(buf, "%d:%02d:%02d", h, m, s);
  else        sprintf(buf, "%02d:%02d", m, s);
  return String(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// AUDIO — Sound effects system
// ─────────────────────────────────────────────────────────────────────────────
enum SfxType {
  SFX_SESSION_START, SFX_SESSION_END, SFX_DISTRACTED, SFX_REFOCUSED,
  SFX_MILESTONE, SFX_POSTURE_NUDGE, SFX_MODE_CHANGE,
  SFX_CAMERA_ON, SFX_CAMERA_OFF, SFX_TOUCH
};

void playSfx(SfxType sfx) {
  switch (sfx) {
    case SFX_SESSION_START:
      // Upward arpeggio — C E G C
      tone(PIN_BUZZER, 523, 80); delay(100);
      tone(PIN_BUZZER, 659, 80); delay(100);
      tone(PIN_BUZZER, 784, 80); delay(100);
      tone(PIN_BUZZER, 1047, 150);
      break;
    case SFX_SESSION_END:
      // Downward arpeggio — G E C
      tone(PIN_BUZZER, 784, 80); delay(100);
      tone(PIN_BUZZER, 659, 80); delay(100);
      tone(PIN_BUZZER, 523, 200);
      break;
    case SFX_DISTRACTED:
      // Two low blips — alert
      tone(PIN_BUZZER, 330, 60); delay(80);
      tone(PIN_BUZZER, 277, 100);
      break;
    case SFX_REFOCUSED:
      // Short rising beep — positive
      tone(PIN_BUZZER, 659, 60); delay(80);
      tone(PIN_BUZZER, 880, 100);
      break;
    case SFX_MILESTONE:
      // Celebratory — E G A E
      tone(PIN_BUZZER, 659, 70); delay(90);
      tone(PIN_BUZZER, 784, 70); delay(90);
      tone(PIN_BUZZER, 880, 70); delay(90);
      tone(PIN_BUZZER, 1047, 150);
      break;
    case SFX_POSTURE_NUDGE:
      // Single soft beep
      tone(PIN_BUZZER, 440, 80);
      break;
    case SFX_MODE_CHANGE:
      // Single click
      tone(PIN_BUZZER, 1200, 25);
      break;
    case SFX_CAMERA_ON:
      tone(PIN_BUZZER, 880, 60); delay(70);
      tone(PIN_BUZZER, 1047, 80);
      break;
    case SFX_CAMERA_OFF:
      tone(PIN_BUZZER, 440, 80);
      break;
    case SFX_TOUCH:
      tone(PIN_BUZZER, 1000, 20);
      break;
  }
}

// ── Bridge read ───────────────────────────────────────────────────────────────
void readMPUState() {
  String v;
  v = Bridge.get("focus_state");    if (v.length()) focusState   = v;
  v = Bridge.get("posture_state");  if (v.length()) postureState = v;
  v = Bridge.get("focus_score");    if (v.length()) focusScore   = v.toFloat();
  v = Bridge.get("distraction_count"); if (v.length()) distractions = v.toInt();
  v = Bridge.get("session_active"); if (v.length()) sessionActive = (v == "1");
  v = Bridge.get("session_secs");   if (v.length()) sessionSecs  = v.toInt();
  v = Bridge.get("burnout_risk");   if (v.length()) burnoutRisk  = v.toFloat();
}

// ── Bridge commands ───────────────────────────────────────────────────────────
void checkCommands() {
  String v;
  v = Bridge.get("led_cmd");
  if (v.length()) { int s=v.indexOf(':'); if(s>0) setLedState(v.substring(0,s), v.substring(s+1)); Bridge.put("led_cmd",""); }

  v = Bridge.get("vibrate_ms");
  if (v.length()) {
    int d = v.toInt();
    if (d > 0 && d <= 2000) { triggerVibration(d); playSfx(SFX_POSTURE_NUDGE); }
    Bridge.put("vibrate_ms","");
  }

  v = Bridge.get("relay");
  if (v.length()) { digitalWrite(PIN_RELAY, v=="1"?HIGH:LOW); Bridge.put("relay",""); }

  v = Bridge.get("backlight");
  if (v.length()) { analogWrite(TFT_BL, constrain(v.toInt(),0,255)); Bridge.put("backlight",""); }

  v = Bridge.get("display_mode");
  if (v.length()) { displayMode = constrain(v.toInt(),0,3); Bridge.put("display_mode",""); }

  // Posture nudge from MPU triggers wince reaction
  v = Bridge.get("posture_reaction");
  if (v.length() && v=="1") { triggerReaction(3); Bridge.put("posture_reaction",""); }
}

// ── Encoder ───────────────────────────────────────────────────────────────────
void handleEncoder() {
  int a = digitalRead(PIN_ENC_A);
  if (a != encLastA && a == LOW) {
    displayMode = (digitalRead(PIN_ENC_B) != a)
      ? (displayMode + 1) % 4
      : (displayMode + 3) % 4;
    tft.fillScreen(C_BG);
    drawModeHeader();
    playSfx(SFX_MODE_CHANGE);
  }
  encLastA = a;

  if (digitalRead(PIN_ENC_SW) == LOW && !encSWPressed) {
    encSWPressed = true;
    static uint8_t bl = 200;
    bl = (bl == 200) ? 100 : (bl == 100) ? 30 : 200;
    analogWrite(TFT_BL, bl);
    playSfx(SFX_TOUCH);
  }
  if (digitalRead(PIN_ENC_SW) == HIGH) encSWPressed = false;
}

// ── Touch ─────────────────────────────────────────────────────────────────────
void handleTouch() {
  bool t = digitalRead(PIN_TOUCH) == HIGH;
  if (t && !touchLast && (millis() - touchDebounce > 500)) {
    touchDebounce = millis();
    Bridge.put("touch_session_toggle", "1");
    // Sound happens in updateAnimationState when session state changes
    playSfx(SFX_TOUCH);
  }
  touchLast = t;
}

// ── LED ring ──────────────────────────────────────────────────────────────────
void setLedState(String state, String color) {
  ledState = state; ledColor = color;
  pulsing = false; flashing = false; ringBreathing = false;
  if      (state == "off")      { ring.clear(); ring.show(); }
  else if (state == "solid")    fillRing(getRingColor(color, 180));
  else if (state == "pulse")    { pulsing = true; pulseStart = millis(); }
  else if (state == "flash")    { flashing = true; flashStart = millis(); }
  else if (state == "breathe")  ringBreathing = true;
}

void updateLedRing() {
  unsigned long n = millis();
  if (pulsing) {
    unsigned long e = (n - pulseStart) % 1200;
    int br = (e < 600) ? map(e, 0, 600, 20, 200) : map(e, 600, 1200, 200, 20);
    fillRing(dimColor(getRingColor(ledColor, 255), br));
    if (n - pulseStart > 3600) { pulsing = false; fillRing(getRingColor(ledColor, 40)); }
  }
  if (flashing) {
    bool on = ((n - flashStart) % 400) < 200;
    if (on) fillRing(getRingColor(ledColor, 220)); else { ring.clear(); ring.show(); }
    if (n - flashStart > 1200) flashing = false;
  }
  if (ringBreathing) {
    float phase = (n % 3000) / 3000.0 * TWO_PI;
    int br = 30 + (int)(80 * (sin(phase) + 1) / 2);
    fillRing(dimColor(getRingColor(ledColor, 255), br));
  }
}

void fillRing(uint32_t c) { for (int i=0;i<NEOPIXEL_COUNT;i++) ring.setPixelColor(i,c); ring.show(); }
uint32_t dimColor(uint32_t c, int br) {
  return ring.Color(((c>>16)&0xFF)*br/255, ((c>>8)&0xFF)*br/255, (c&0xFF)*br/255);
}
uint32_t getRingColor(String n, int br) {
  float s = br/255.0;
  if (n=="cyan")   return ring.Color(0,       212*s, 255*s);
  if (n=="green")  return ring.Color(0,       255*s, 136*s);
  if (n=="orange") return ring.Color(255*s,   107*s, 0);
  if (n=="red")    return ring.Color(255*s,   0,     0);
  if (n=="purple") return ring.Color(148*s,   0,     211*s);
  return ring.Color(0, 212*s, 255*s);
}

// ── Vibration ─────────────────────────────────────────────────────────────────
void triggerVibration(int ms) { digitalWrite(PIN_VIBRATION,HIGH); delay(ms); digitalWrite(PIN_VIBRATION,LOW); }

// ── Boot sequence ──────────────────────────────────────────────────────────────
void bootSequence() {
  // Splash
  tft.fillScreen(C_BG);

  // Animated face during boot — eyes opening from squint
  for (int openH = 2; openH <= 14; openH += 2) {
    tft.fillCircle(120, 145, 84, C_DGRAY);
    tft.drawCircle(120, 145, 84, C_ACCENT);
    tft.drawCircle(120, 145, 83, C_ACCENT);
    tft.fillRoundRect(94,  127, 20, openH, 4, C_ACCENT);
    tft.fillRoundRect(126, 127, 20, openH, 4, C_ACCENT);
    delay(60);
  }

  // Title
  tft.setTextColor(C_ACCENT); tft.setTextSize(2);
  tft.setCursor(24, 240); tft.print("SMARTDESK OS");
  tft.setTextColor(C_GRAY); tft.setTextSize(1);
  tft.setCursor(52, 264); tft.print("AI Productivity Hub");

  // NeoPixel sweep
  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    ring.setPixelColor(i, getRingColor("cyan", 160));
    ring.show(); delay(40);
  }

  // Startup chime
  tone(PIN_BUZZER, 523, 80); delay(100);
  tone(PIN_BUZZER, 659, 80); delay(100);
  tone(PIN_BUZZER, 784, 80); delay(100);
  tone(PIN_BUZZER, 1047, 200); delay(300);

  // Wink animation
  tft.fillRoundRect(94,  127, 20, 14, 4, C_ACCENT);   // left open
  tft.fillRect(126, 130, 20, 3, C_ACCENT);             // right wink
  delay(300);
  tft.fillRoundRect(126, 127, 20, 14, 4, C_ACCENT);   // both open
  delay(200);

  // Fade ring
  for (int b = 160; b >= 0; b -= 8) { fillRing(dimColor(getRingColor("cyan",255), b)); delay(20); }
  ring.clear(); ring.show();

  tft.fillScreen(C_BG);
  drawModeHeader();
}
