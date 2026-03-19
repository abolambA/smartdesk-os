# SmartDesk OS — Wiring Reference v2
# Arduino UNO Q (STM32 MCU headers)

## Display — ST7789 2" IPS 240×320 (SPI)

| ST7789 Pin | Arduino Pin | Notes                          |
|------------|-------------|--------------------------------|
| SCK        | D13         | SPI clock (hardware SPI)       |
| MOSI/SDA   | D11         | SPI data                       |
| CS         | D10         | Chip select                    |
| DC/RS      | D8          | Data/command select            |
| RST        | D5          | Reset                          |
| BL         | D3          | Backlight PWM (220Ω optional)  |
| VCC        | 3.3V        |                                |
| GND        | GND         |                                |

## Sensors

| Component      | Pin   | Notes                                    |
|----------------|-------|------------------------------------------|
| BH1750 SDA     | SDA   | I2C                                      |
| BH1750 SCL     | SCL   | I2C                                      |
| BH1750 VCC     | 3.3V  |                                          |
| DHT22 DATA     | D4    | 10kΩ pull-up to 3.3V on data line        |
| DHT22 VCC      | 3.3V  |                                          |
| MAX4466 OUT    | A0    | Analog mic amplifier                     |
| MAX4466 VCC    | 3.3V  |                                          |

## New components (v2)

| Component         | Pin | Notes                                       |
|-------------------|-----|---------------------------------------------|
| Rotary ENC_A      | A1  | INPUT_PULLUP                                |
| Rotary ENC_B      | A2  | INPUT_PULLUP                                |
| Rotary SW (btn)   | A3  | INPUT_PULLUP, push to GND                   |
| TTP223 touch OUT  | A4  | Capacitive touch — HIGH when touched        |
| TTP223 VCC        | 3.3V|                                             |
| Passive buzzer +  | D12 | Use tone() — do NOT use active buzzer       |
| Passive buzzer -  | GND |                                             |

## Outputs

| Component      | Pin | Notes                                        |
|----------------|-----|----------------------------------------------|
| NeoPixel DATA  | D6  | 300Ω series resistor on data line            |
| NeoPixel VCC   | 5V  | External 5V supply if drawing full power     |
| Vibration +    | D9  | Via NPN transistor (2N2222)                  |
| Relay IN       | D7  | 5V relay module                              |
| Privacy LED +  | A5  | Green LED + 220Ω to GND (moved from D3)      |
| Privacy SW     | D2  | Interrupt, INPUT_PULLUP, switch to GND       |

## Shopping list (new parts for v2)

| Part                    | Notes                                     |
|-------------------------|-------------------------------------------|
| ST7789 2" IPS 240×320   | SPI, 3.3V logic — you already have this  |
| Rotary encoder EC11     | With push button, common part ~$1         |
| TTP223 capacitive touch | Single-button module ~$0.50              |
| Passive piezo buzzer    | NOT active buzzer — passive only for tone()|
| 10kΩ resistors (×2)    | For DHT22 pull-up and encoder pull-ups    |
| 220Ω resistors (×2)    | LED current limiting                      |
| 300Ω resistor           | NeoPixel data line protection             |
| 2N2222 NPN transistor   | Vibration motor driver                    |
| 1N4001 diode            | Flyback protection for vibration motor    |

## Key design decisions

- **2" 240×320 is correct** — enough pixels for animated faces + readable text
- **Passive buzzer** — tone() function gives musical notes; active buzzer only beeps
- **TTP223** — capacitive touch looks premium, no physical button hole in enclosure
- **Rotary encoder** — mechanical, satisfying, intuitive for mode cycling
