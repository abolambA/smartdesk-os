# SmartDesk OS — Arduino Library Dependencies
# Install all of these via Arduino IDE 2.0 Library Manager
# (Sketch → Include Library → Manage Libraries)

BH1750                    # by Christopher Laws — I2C light sensor
DHT sensor library        # by Adafruit — DHT22 temp/humidity
Adafruit NeoPixel         # by Adafruit — WS2812B LED ring
Bridge                    # by Arduino — MCU↔MPU RPC (built-in for UNO Q)

# Notes:
# - Wire.h is built-in, no install needed
# - Bridge.h is specific to Arduino UNO Q / Yún boards
# - NeoPixel requires Adafruit_BusIO as a dependency (installed automatically)
# - DHT library requires Adafruit Unified Sensor (installed automatically)
