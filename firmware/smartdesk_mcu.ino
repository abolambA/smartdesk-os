/*
 * SmartDesk OS — MCU Firmware
 * ────────────────────────────
 * Runs on the STM32U585 microcontroller (Arduino Core / Zephyr RTOS side)
 * of the Arduino UNO Q board.
 *
 * Responsibilities:
 *   - Read all sensors (BH1750 light, DHT22 temp/humidity, MAX4466 sound)
 *   - Monitor privacy toggle switch (hardware interrupt)
 *   - Control WS2812B NeoPixel LED ring
 *   - Trigger vibration motor (posture nudge)
 *   - Control desk lamp relay
 *   - Communicate with Linux MPU via Arduino Bridge RPC
 *
 * Architecture principle: this sketch is intentionally stateless and minimal.
 * All decision logic lives on the Python MPU side. The MCU is purely a
 * sensor/actuator I/O driver that obeys commands from the MPU.
 *
 * Wiring:
 *   BH1750    → SDA/SCL (I2C)
 *   DHT22     → D4
 *   MAX4466   → A0
 *   NeoPixel  → D6   (12-pixel WS2812B ring)
 *   Vibration → D9   (via NPN transistor, PWM)
 *   Relay     → D7
 *   Privacy   → D2   (interrupt, INPUT_PULLUP, switch to GND)
 *   Privacy LED → D3 (green LED + 220Ω resistor)
 */

#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <Bridge.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
#define PIN_DHT          4
#define PIN_SOUND        A0
#define PIN_NEOPIXEL     6
#define PIN_VIBRATION    9
#define PIN_RELAY        7
#define PIN_PRIVACY_SW   2
#define PIN_PRIVACY_LED  3

// ── Sensor config ─────────────────────────────────────────────────────────────
#define DHT_TYPE         DHT22
#define NEOPIXEL_COUNT   12
#define SOUND_SAMPLES    10   // ADC samples to average per reading

// ── Read intervals (milliseconds) ─────────────────────────────────────────────
#define INTERVAL_LIGHT   5000
#define INTERVAL_TEMP    30000
#define INTERVAL_SOUND   1000

// ── Objects ───────────────────────────────────────────────────────────────────
BH1750            lightMeter;
DHT               dht(PIN_DHT, DHT_TYPE);
Adafruit_NeoPixel ring(NEOPIXEL_COUNT, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── State ─────────────────────────────────────────────────────────────────────
volatile bool privacySwitchChanged = false;
volatile bool cameraEnabled        = true;

unsigned long lastLightRead  = 0;
unsigned long lastTempRead   = 0;
unsigned long lastSoundRead  = 0;
unsigned long lastBridgeSync = 0;

float  currentLux      = 0.0;
float  currentTempC    = 0.0;
float  currentHumidity = 0.0;
float  currentSoundDb  = 0.0;

// LED ring state
String currentLedState = "off";
String currentLedColor = "cyan";
unsigned long pulseStart  = 0;
bool          pulsing     = false;
int           pulseCount  = 0;
int           flashCount  = 0;
unsigned long flashStart  = 0;
bool          flashing    = false;

// ── Interrupt service routine for privacy switch ──────────────────────────────
void privacyISR() {
  privacySwitchChanged = true;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("[MCU] SmartDesk OS firmware starting..."));

  // Bridge init (MPU communication)
  Bridge.begin();
  Serial.println(F("[MCU] Bridge connected"));

  // I2C + BH1750
  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(F("[MCU] BH1750 ready"));

  // DHT22
  dht.begin();
  Serial.println(F("[MCU] DHT22 ready"));

  // NeoPixel ring
  ring.begin();
  ring.setBrightness(80);
  ring.clear();
  ring.show();
  Serial.println(F("[MCU] NeoPixel ring ready"));

  // Output pins
  pinMode(PIN_VIBRATION,   OUTPUT);
  pinMode(PIN_RELAY,       OUTPUT);
  pinMode(PIN_PRIVACY_LED, OUTPUT);
  digitalWrite(PIN_RELAY,       LOW);
  digitalWrite(PIN_VIBRATION,   LOW);
  digitalWrite(PIN_PRIVACY_LED, HIGH);  // camera on by default

  // Privacy switch interrupt (falling = switch closed to GND)
  pinMode(PIN_PRIVACY_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_PRIVACY_SW), privacyISR, CHANGE);

  // Boot animation: sweep cyan
  bootAnimation();

  Serial.println(F("[MCU] Ready"));
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Handle privacy switch ─────────────────────────────────────────────────
  if (privacySwitchChanged) {
    privacySwitchChanged = false;
    cameraEnabled = (digitalRead(PIN_PRIVACY_SW) == HIGH);
    digitalWrite(PIN_PRIVACY_LED, cameraEnabled ? HIGH : LOW);
    Bridge.put("privacy_switch", cameraEnabled ? "0" : "1");
    Serial.print(F("[MCU] Privacy switch: "));
    Serial.println(cameraEnabled ? F("camera ON") : F("camera OFF"));
  }

  // ── Read light sensor (every 5s) ──────────────────────────────────────────
  if (now - lastLightRead >= INTERVAL_LIGHT) {
    lastLightRead = now;
    if (lightMeter.measurementReady()) {
      currentLux = lightMeter.readLightLevel();
      if (currentLux < 0) currentLux = 0;
      Bridge.put("light_lux", String(currentLux, 1));
    }
  }

  // ── Read DHT22 (every 30s) ────────────────────────────────────────────────
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

  // ── Read sound sensor (every 1s, averaged) ────────────────────────────────
  if (now - lastSoundRead >= INTERVAL_SOUND) {
    lastSoundRead = now;
    long sum = 0;
    for (int i = 0; i < SOUND_SAMPLES; i++) {
      sum += analogRead(PIN_SOUND);
      delayMicroseconds(100);
    }
    int avg      = sum / SOUND_SAMPLES;
    // Convert ADC (0-1023) to approximate dB (30–90 dB range)
    currentSoundDb = 30.0 + (avg / 1023.0) * 60.0;
    Bridge.put("sound_db", String(currentSoundDb, 1));
  }

  // ── Check for incoming commands from MPU ──────────────────────────────────
  if (now - lastBridgeSync >= 200) {   // check every 200ms
    lastBridgeSync = now;
    checkCommands();
  }

  // ── Update LED ring animation ─────────────────────────────────────────────
  updateLedRing();
}

// ── Command handler ───────────────────────────────────────────────────────────
void checkCommands() {
  // LED ring command: "solid:cyan" | "pulse:orange" | "flash:red" | "off:cyan"
  String ledCmd = Bridge.get("led_cmd");
  if (ledCmd.length() > 0) {
    int sep = ledCmd.indexOf(':');
    if (sep > 0) {
      String state = ledCmd.substring(0, sep);
      String color = ledCmd.substring(sep + 1);
      setLedState(state, color);
    }
    Bridge.put("led_cmd", "");  // clear command
  }

  // Vibration command: duration in ms, e.g. "300"
  String vibrCmd = Bridge.get("vibrate_ms");
  if (vibrCmd.length() > 0) {
    int duration = vibrCmd.toInt();
    if (duration > 0 && duration <= 2000) {
      triggerVibration(duration);
    }
    Bridge.put("vibrate_ms", "");
  }

  // Relay command: "1" = on, "0" = off
  String relayCmd = Bridge.get("relay");
  if (relayCmd.length() > 0) {
    digitalWrite(PIN_RELAY, relayCmd == "1" ? HIGH : LOW);
    Bridge.put("relay", "");
  }
}

// ── LED ring control ──────────────────────────────────────────────────────────
void setLedState(String state, String color) {
  currentLedState = state;
  currentLedColor = color;
  pulsing  = false;
  flashing = false;

  if (state == "off") {
    ring.clear();
    ring.show();
  } else if (state == "solid") {
    fillRing(getColor(color, 180));
  } else if (state == "pulse") {
    pulsing    = true;
    pulseStart = millis();
    pulseCount = 0;
  } else if (state == "flash") {
    flashing   = true;
    flashStart = millis();
    flashCount = 0;
  }
}

void updateLedRing() {
  unsigned long now = millis();

  if (pulsing) {
    // 3 slow pulses then stay dim
    unsigned long elapsed = (now - pulseStart) % 1200;
    int brightness = (elapsed < 600)
      ? map(elapsed, 0, 600, 20, 200)
      : map(elapsed, 600, 1200, 200, 20);
    fillRing(dimColor(getColor(currentLedColor, 255), brightness));

    if (now - pulseStart > 3600) {  // 3 pulses done
      pulsing = false;
      fillRing(getColor(currentLedColor, 40));  // stay dim
    }
  }

  if (flashing) {
    unsigned long elapsed = (now - flashStart) % 400;
    bool on = (elapsed < 200);
    if (on) fillRing(getColor(currentLedColor, 220));
    else    ring.clear();
    ring.show();
    if (now - flashStart > 1200) flashing = false;  // 3 flashes
  }
}

void fillRing(uint32_t color) {
  for (int i = 0; i < NEOPIXEL_COUNT; i++) ring.setPixelColor(i, color);
  ring.show();
}

uint32_t dimColor(uint32_t color, int brightness) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >>  8) & 0xFF;
  uint8_t b = (color      ) & 0xFF;
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
  return ring.Color(r, g, b);
}

uint32_t getColor(String name, int brightness) {
  // brightness 0-255
  float scale = brightness / 255.0;
  if (name == "cyan")   return ring.Color(0, int(212 * scale), int(255 * scale));
  if (name == "green")  return ring.Color(0, int(255 * scale), int(136 * scale));
  if (name == "orange") return ring.Color(int(255 * scale), int(107 * scale), 0);
  if (name == "red")    return ring.Color(int(255 * scale), 0, 0);
  if (name == "white")  return ring.Color(int(200 * scale), int(200 * scale), int(200 * scale));
  return ring.Color(0, int(212 * scale), int(255 * scale));  // default cyan
}

// ── Vibration motor ───────────────────────────────────────────────────────────
void triggerVibration(int durationMs) {
  digitalWrite(PIN_VIBRATION, HIGH);
  delay(durationMs);
  digitalWrite(PIN_VIBRATION, LOW);
  Serial.print(F("[MCU] Vibration: "));
  Serial.print(durationMs);
  Serial.println(F("ms"));
}

// ── Boot animation ────────────────────────────────────────────────────────────
void bootAnimation() {
  // Sweep cyan around the ring
  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    ring.setPixelColor(i, getColor("cyan", 160));
    ring.show();
    delay(40);
  }
  delay(200);
  // Fade out
  for (int b = 160; b >= 0; b -= 8) {
    fillRing(dimColor(getColor("cyan", 255), b));
    delay(20);
  }
  ring.clear();
  ring.show();
}
