"""
camera_inference.py
────────────────────
Runs the two Edge Impulse models (Focus Detection + Posture Detection)
on live camera frames every 2 seconds. Orchestrates all physical responses:
LED ring, vibration motor, desk lamp relay. Calls session_engine to log events.

Falls back to simulation mode when the camera or EI SDK is unavailable
(development on laptop before hardware arrives).
"""

import time
import threading
import random
from datetime import datetime, timedelta

# ── Attempt Edge Impulse Linux SDK import ─────────────────────────────────────
try:
    from edge_impulse_linux.image import ImageImpulseRunner
    EI_AVAILABLE = True
except ImportError:
    EI_AVAILABLE = False
    print("[CAM] Edge Impulse SDK not available — running in simulation mode")

# ── Attempt OpenCV import ─────────────────────────────────────────────────────
try:
    import cv2
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False
    print("[CAM] OpenCV not available — running in simulation mode")

from pathlib import Path

FOCUS_MODEL_PATH   = Path(__file__).parent / "models" / "focus_model.eim"
POSTURE_MODEL_PATH = Path(__file__).parent / "models" / "posture_model.eim"

INFERENCE_INTERVAL  = 2    # seconds between focus inferences
POSTURE_INTERVAL    = 5    # seconds between posture inferences
MIN_NUDGE_GAP       = 300  # seconds minimum between posture nudges (5 min)
DISTRACT_THRESHOLD  = 3    # consecutive distracted ticks before response
ABSENT_LAMP_OFF     = 90   # consecutive absent ticks before lamp off (3 min)
LAMP_DIM_PERCENT    = 0.6  # lamp brightness when focused

# ── CameraInference ───────────────────────────────────────────────────────────

class CameraInference:
    def __init__(self, session_engine, sensor_manager):
        self._session  = session_engine
        self._sensors  = sensor_manager

        self._running  = False
        self._thread   = None

        # State
        self._focus_state      = "absent"
        self._posture_state    = "upright"
        self._focus_confidence = 0.0
        self._posture_confidence = 0.0
        self._last_inference   = None

        # Hysteresis counters
        self._consec_distracted = 0
        self._consec_absent     = 0
        self._consec_focused    = 0

        # Timing
        self._last_posture_tick  = 0
        self._last_nudge_time    = datetime.min
        self._lamp_state         = "on"   # on | dim | off

        # Model runners (loaded lazily)
        self._focus_runner   = None
        self._posture_runner = None
        self._cap            = None

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def start(self):
        if self._running:
            return
        self._running = True
        self._thread  = threading.Thread(target=self._inference_loop, daemon=True)
        self._thread.start()
        print("[CAM] Inference loop started")

    def stop(self):
        self._running = False
        if self._cap:
            self._cap.release()
        if self._focus_runner:
            self._focus_runner.__exit__(None, None, None)
        if self._posture_runner:
            self._posture_runner.__exit__(None, None, None)
        print("[CAM] Inference loop stopped")

    # ── Main inference loop ───────────────────────────────────────────────────

    def _inference_loop(self):
        tick = 0
        while self._running:
            loop_start = time.time()

            if not self._sensors.camera_active:
                # Camera disabled via privacy switch — reset states
                self._focus_state   = "absent"
                self._posture_state = "upright"
                time.sleep(INFERENCE_INTERVAL)
                continue

            try:
                frame = self._capture_frame()

                # Focus inference every tick (2 s)
                focus_result   = self._run_focus(frame)
                self._focus_state      = focus_result["label"]
                self._focus_confidence = focus_result["confidence"]

                # Posture inference every 5 s
                if tick % (POSTURE_INTERVAL // INFERENCE_INTERVAL) == 0:
                    posture_result = self._run_posture(frame)
                    self._posture_state      = posture_result["label"]
                    self._posture_confidence = posture_result["confidence"]

                self._last_inference = datetime.now()

                # Record tick in session engine
                self._session.record_tick(self._focus_state, self._posture_state)

                # Trigger physical responses
                self._handle_focus_response()
                self._handle_posture_response()
                self._handle_environment()

            except Exception as e:
                print(f"[CAM] Inference error: {e}")

            tick += 1
            elapsed = time.time() - loop_start
            sleep_time = max(0, INFERENCE_INTERVAL - elapsed)
            time.sleep(sleep_time)

    # ── Frame capture ─────────────────────────────────────────────────────────

    def _capture_frame(self):
        if CV2_AVAILABLE:
            if self._cap is None:
                self._cap = cv2.VideoCapture(0)
                self._cap.set(cv2.CAP_PROP_FRAME_WIDTH,  96)
                self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 96)
            ret, frame = self._cap.read()
            if ret:
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                return cv2.resize(gray, (96, 96))
        # Simulation: return None (models will simulate)
        return None

    # ── Model inference ───────────────────────────────────────────────────────

    def _run_focus(self, frame) -> dict:
        if EI_AVAILABLE and frame is not None and FOCUS_MODEL_PATH.exists():
            try:
                if self._focus_runner is None:
                    self._focus_runner = ImageImpulseRunner(str(FOCUS_MODEL_PATH))
                    self._focus_runner.__enter__()
                features, _ = self._focus_runner.get_features_from_img(frame)
                result = self._focus_runner.classify(features)
                classification = result["result"]["classification"]
                label = max(classification, key=classification.get)
                return {"label": label, "confidence": round(classification[label], 3)}
            except Exception as e:
                print(f"[CAM] Focus model error: {e}")

        # Simulation
        return self._simulate_focus()

    def _run_posture(self, frame) -> dict:
        if EI_AVAILABLE and frame is not None and POSTURE_MODEL_PATH.exists():
            try:
                if self._posture_runner is None:
                    self._posture_runner = ImageImpulseRunner(str(POSTURE_MODEL_PATH))
                    self._posture_runner.__enter__()
                features, _ = self._posture_runner.get_features_from_img(frame)
                result = self._posture_runner.classify(features)
                classification = result["result"]["classification"]
                label = max(classification, key=classification.get)
                return {"label": label, "confidence": round(classification[label], 3)}
            except Exception as e:
                print(f"[CAM] Posture model error: {e}")

        # Simulation
        return self._simulate_posture()

    # ── Simulation helpers ────────────────────────────────────────────────────

    def _simulate_focus(self) -> dict:
        """Realistic simulation: mostly focused with occasional distractions."""
        r = random.random()
        if r < 0.70:
            return {"label": "focused",    "confidence": round(random.uniform(0.82, 0.97), 3)}
        elif r < 0.88:
            return {"label": "distracted", "confidence": round(random.uniform(0.75, 0.93), 3)}
        else:
            return {"label": "absent",     "confidence": round(random.uniform(0.80, 0.96), 3)}

    def _simulate_posture(self) -> dict:
        r = random.random()
        if r < 0.75:
            return {"label": "upright",    "confidence": round(random.uniform(0.80, 0.95), 3)}
        elif r < 0.92:
            return {"label": "slouching",  "confidence": round(random.uniform(0.75, 0.90), 3)}
        else:
            return {"label": "craning",    "confidence": round(random.uniform(0.72, 0.88), 3)}

    # ── Physical response logic ───────────────────────────────────────────────

    def _handle_focus_response(self):
        state = self._focus_state

        if state == "focused":
            self._consec_distracted = 0
            self._consec_absent     = 0
            self._consec_focused   += 1
            # After 2 consecutive focused ticks — solid cyan LED
            if self._consec_focused == 2:
                self._sensors.set_led_ring("solid", "cyan")
                if self._lamp_state != "dim":
                    self._sensors.set_relay(True)
                    self._lamp_state = "dim"

        elif state == "distracted":
            self._consec_distracted += 1
            self._consec_absent      = 0
            self._consec_focused     = 0
            # After 3 consecutive distracted ticks (6 s) — alert
            if self._consec_distracted == DISTRACT_THRESHOLD:
                self._sensors.set_led_ring("pulse", "orange")
                print(f"[CAM] Distraction alert — count: {self._session._distraction_count}")

            # Phone pickup sub-type handled in session engine tick
            if self._focus_confidence > 0.85 and "phone" in self._focus_state:
                self._session.record_phone_pickup()

        elif state == "absent":
            self._consec_absent     += 1
            self._consec_distracted  = 0
            self._consec_focused     = 0
            self._sensors.set_led_ring("off", "cyan")
            # After 3 min absent — turn lamp off
            if self._consec_absent >= ABSENT_LAMP_OFF and self._lamp_state != "off":
                self._sensors.set_relay(False)
                self._lamp_state = "off"
                print("[CAM] Lamp off — user absent 3+ min")

    def _handle_posture_response(self):
        if self._posture_state in ("slouching", "craning"):
            now = datetime.now()
            gap = (now - self._last_nudge_time).total_seconds()
            if gap >= MIN_NUDGE_GAP:
                self._sensors.trigger_vibration(300)
                self._session.record_posture_nudge(self._posture_state)
                self._last_nudge_time = now
                print(f"[CAM] Posture nudge — {self._posture_state}")

    def _handle_environment(self):
        # Auto-brighten lamp when light drops below threshold
        if self._sensors.is_low_light and self._lamp_state == "dim":
            self._sensors.set_relay(True)
            print("[CAM] Low light detected — lamp at full brightness")

        # Do-not-disturb LED when noisy
        if self._sensors.is_noisy:
            self._sensors.set_led_ring("solid", "red")

    # ── Public state API ──────────────────────────────────────────────────────

    @property
    def state(self) -> dict:
        return {
            "focus_state":        self._focus_state,
            "posture_state":      self._posture_state,
            "focus_confidence":   self._focus_confidence,
            "posture_confidence": self._posture_confidence,
            "last_inference":     self._last_inference.isoformat() if self._last_inference else None,
        }
