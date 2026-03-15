# SmartDesk OS — Wiring Reference
# Arduino UNO Q (STM32 MCU headers)

## Sensors

| Component         | Pin  | Notes                                      |
|-------------------|------|--------------------------------------------|
| BH1750 SDA        | SDA  | I2C — share bus with other I2C devices     |
| BH1750 SCL        | SCL  | I2C                                        |
| BH1750 VCC        | 3.3V |                                            |
| BH1750 GND        | GND  |                                            |
| DHT22 DATA        | D4   | Add 10kΩ pull-up to 3.3V on data line      |
| DHT22 VCC         | 3.3V |                                            |
| DHT22 GND         | GND  |                                            |
| MAX4466 OUT       | A0   | Analog microphone amplifier output         |
| MAX4466 VCC       | 3.3V |                                            |
| MAX4466 GND       | GND  |                                            |

## Outputs

| Component         | Pin  | Notes                                      |
|-------------------|------|--------------------------------------------|
| NeoPixel DATA     | D6   | WS2812B 12-pixel ring, 5V power            |
| NeoPixel VCC      | 5V   | Use 300Ω series resistor on data line      |
| NeoPixel GND      | GND  |                                            |
| Vibration Motor + | D9   | Via NPN transistor (2N2222 or BC547)       |
| Vibration Motor - | GND  | Add flyback diode across motor terminals   |
| Relay IN          | D7   | 5V relay module signal pin                 |
| Relay VCC         | 5V   |                                            |
| Relay GND         | GND  |                                            |
| Privacy LED +     | D3   | Green 5mm LED + 220Ω resistor to GND       |
| Privacy LED -     | GND  |                                            |

## Privacy Switch

| Component         | Pin  | Notes                                      |
|-------------------|------|--------------------------------------------|
| Switch terminal 1 | D2   | Uses INPUT_PULLUP — switch connects D2→GND |
| Switch terminal 2 | GND  |                                            |

## Camera (MIPI-CSI — connects to MPU, not MCU)

| Component         | Port      | Notes                                   |
|-------------------|-----------|-----------------------------------------|
| Camera module     | MIPI-CSI  | Connect to UNO Q MIPI-CSI connector     |
| Camera VCC        | Via switch| Privacy switch cuts power to camera VCC |

## Transistor wiring for vibration motor (D9)

```
D9 ──[1kΩ]── Base (2N2222)
              Collector ── Motor (+)
              Emitter   ── GND
Motor (-)  ── GND
Flyback diode across motor: cathode to Motor(+), anode to GND
```

## Relay for desk lamp

```
D7 → Relay IN (signal)
5V → Relay VCC
GND → Relay GND

Relay COM  → Live wire of lamp power
Relay NO   → Lamp
(Normally Open — lamp off when relay not energized)
```

## Power notes
- UNO Q powered via USB-C (5V/3A recommended)
- NeoPixel ring draws up to 720mA at full white — use external 5V supply for ring if needed
- All logic levels are 3.3V on UNO Q MCU headers
