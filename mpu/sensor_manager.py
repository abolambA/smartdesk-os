"""
sensor_manager.py
─────────────────
Reads all sensor values from the STM32 MCU via Arduino Bridge RPC.
Maintains a rolling 60-second buffer of readings and exposes current
and recent state to other modules via a shared in-memory data object.

When the UNO Q board is not connected (development mode), falls back
to simulated sensor values so the rest of the stack can run on any machine.
"""

import time
import threading
import random
from datetime import datetime
from collections import deque

# ── Attempt to import Bridge (only available on UNO Q) ───────────────────────
try:
    from bridge import Bridge
    BRIDGE_AVAILABLE = True
except ImportError:
    BRIDGE_AVAILABLE = False
    print("[SENSOR] Bridge not available — running in simulation mode")

# ── Constants ─────────────────────────────────────────────────────────────────

LIGHT_READ_INTERVAL   = 5    # seconds
TEMP_READ_INTERVAL    = 30   # seconds
SOUND_READ_INTERVAL   = 1    # seconds  (averaged over 10 samples)
PRIVACY_CHECK_INTERVAL = 1   # seconds
BUFFER_SECONDS        = 60   # rolling buffer depth

# ── SensorManager ─────────────────────────────────────────────────────────────

class SensorManager:
    def __init__(self):
        self._lock = threading.Lock()

        # Current readings
        self._light_lux    = 400.0
        self._temperature  = 23.0
        self._humidity     = 50.0
        self._sound_db     = 35.0
        self._camera_active = True
        self._privacy_switch = False

        # Rolling 60-second buffers (one entry per second)
        self._light_buffer = deque(maxlen=BUFFER_SECONDS)
        self._sound_buffer = deque(maxlen=BUFFER_SECONDS)

        # Bridge handle
        self._bridge = None
        if BRIDGE_AVAILABLE:
            try:
                self._bridge = Bridge()
                self._bridge.begin(115200)
                print("[SENSOR] Bridge connected")
            except Exception as e:
                print(f"[SENSOR] Bridge init failed: {e} — using simulation")

        # Start background polling thread
        self._running = True
        self._thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._thread.start()

    # ── Background polling ────────────────────────────────────────────────────

    def _poll_loop(self):
        tick = 0
        while self._running:
            try:
                # Sound: every second
                sound = self._read_sound()
                with self._lock:
                    self._sound_db = sound
                    self._sound_buffer.append(sound)

                # Light: every 5 seconds
                if tick % LIGHT_READ_INTERVAL == 0:
                    lux = self._read_light()
                    with self._lock:
                        self._light_lux = lux
                        self._light_buffer.append(lux)

                # Temperature + humidity: every 30 seconds
                if tick % TEMP_READ_INTERVAL == 0:
                    temp, hum = self._read_temp_humidity()
                    with self._lock:
                        self._temperature = temp
                        self._humidity    = hum

                # Privacy switch: every second
                priv = self._read_privacy_switch()
                with self._lock:
                    self._privacy_switch = priv

            except Exception as e:
                print(f"[SENSOR] Poll error: {e}")

            tick += 1
            time.sleep(1)

    # ── Low-level reads (Bridge or simulation) ────────────────────────────────

    def _read_light(self) -> float:
        if self._bridge:
            try:
                val = self._bridge.get("light_lux")
                return float(val)
            except Exception:
                pass
        # Simulation: gentle sine-wave variation around base value
        import math
        base = 400
        return round(base + 80 * math.sin(time.time() / 60), 1)

    def _read_sound(self) -> float:
        if self._bridge:
            try:
                val = self._bridge.get("sound_db")
                return float(val)
            except Exception:
                pass
        # Simulation: random ambient noise
        return round(random.uniform(28, 48), 1)

    def _read_temp_humidity(self):
        if self._bridge:
            try:
                temp = float(self._bridge.get("temperature_c"))
                hum  = float(self._bridge.get("humidity"))
                return temp, hum
            except Exception:
                pass
        # Simulation
        return round(23.0 + random.uniform(-0.5, 0.5), 1), round(48.0 + random.uniform(-2, 2), 1)

    def _read_privacy_switch(self) -> bool:
        if self._bridge:
            try:
                val = self._bridge.get("privacy_switch")
                return str(val).strip() == "1"
            except Exception:
                pass
        return False

    # ── MCU command senders ───────────────────────────────────────────────────

    def set_led_ring(self, state: str, color: str = "cyan"):
        """
        state: 'off' | 'solid' | 'pulse' | 'flash'
        color: 'cyan' | 'green' | 'orange' | 'red'
        """
        if self._bridge:
            try:
                self._bridge.put("led_cmd", f"{state}:{color}")
            except Exception as e:
                print(f"[SENSOR] LED command failed: {e}")
        else:
            print(f"[SENSOR SIM] LED ring → {state}:{color}")

    def trigger_vibration(self, duration_ms: int = 300):
        """Trigger the posture nudge vibration motor."""
        if self._bridge:
            try:
                self._bridge.put("vibrate_ms", str(duration_ms))
            except Exception as e:
                print(f"[SENSOR] Vibration command failed: {e}")
        else:
            print(f"[SENSOR SIM] Vibration → {duration_ms}ms")

    def set_relay(self, on: bool):
        """Control the desk lamp relay."""
        if self._bridge:
            try:
                self._bridge.put("relay", "1" if on else "0")
            except Exception as e:
                print(f"[SENSOR] Relay command failed: {e}")
        else:
            print(f"[SENSOR SIM] Relay → {'ON' if on else 'OFF'}")

    def set_camera_active(self, active: bool):
        with self._lock:
            self._camera_active = active
        print(f"[SENSOR] Camera → {'active' if active else 'disabled'}")

    # ── Public read API ───────────────────────────────────────────────────────

    @property
    def current(self) -> dict:
        with self._lock:
            return {
                "light_lux":      self._light_lux,
                "temperature_c":  self._temperature,
                "humidity":       self._humidity,
                "sound_db":       self._sound_db,
                "privacy_switch": self._privacy_switch,
                "camera_active":  self._camera_active,
                "timestamp":      datetime.now().isoformat(),
            }

    @property
    def camera_active(self) -> bool:
        with self._lock:
            return self._camera_active

    @property
    def is_noisy(self) -> bool:
        with self._lock:
            return self._sound_db > 65

    @property
    def is_low_light(self) -> bool:
        with self._lock:
            return self._light_lux < 100

    def avg_sound_60s(self) -> float:
        with self._lock:
            buf = list(self._sound_buffer)
        return round(sum(buf) / len(buf), 1) if buf else 0.0

    def avg_light_60s(self) -> float:
        with self._lock:
            buf = list(self._light_buffer)
        return round(sum(buf) / len(buf), 1) if buf else 0.0

    def stop(self):
        self._running = False
