# SmartDesk OS — Arduino Library Dependencies v2
# Install via Arduino IDE 2.0: Sketch → Include Library → Manage Libraries

Adafruit ST7789          # by Adafruit — TFT display driver
Adafruit GFX Library     # by Adafruit — graphics primitives (dependency of ST7789)
Adafruit BusIO           # by Adafruit — SPI/I2C bus (dependency, auto-installed)
BH1750                   # by Christopher Laws — I2C light sensor
DHT sensor library       # by Adafruit — DHT22 temp/humidity
Adafruit NeoPixel        # by Adafruit — WS2812B LED ring
Bridge                   # by Arduino — MCU↔MPU RPC (built-in for UNO Q)

# Notes:
# - Wire.h and SPI.h are built-in, no install needed
# - Adafruit GFX and BusIO install automatically as dependencies
# - tone() function is built-in for passive buzzer
