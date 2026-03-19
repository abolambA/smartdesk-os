"""
api_server.py — Flask REST API + App Lab dashboard + UDP discovery
Run with: python3 api_server.py
"""
import json, socket, threading, time
from datetime import datetime
from pathlib import Path
from flask import Flask, jsonify, request, send_from_directory
from flask_cors import CORS
from sensor_manager import SensorManager
from session_engine import SessionEngine
from camera_inference import CameraInference
from behavioral_engine import BehavioralEngine

app = Flask(__name__, static_folder="dashboard", static_url_path="")
CORS(app)
DASHBOARD_DIR  = Path(__file__).parent.parent / "dashboard"
API_PORT       = 8080
DISCOVERY_PORT = 5001

sensors = session = camera = behavioral = None

def ok(data=None):
    return jsonify({"status": "ok", **({"data": data} if data else {})})

def err(msg, code=400):
    return jsonify({"status": "error", "message": msg}), code

# ── Endpoints ─────────────────────────────────────────────────────────────────

@app.route("/status")
def get_status():
    cam = camera.state if camera else {}
    sen = sensors.current if sensors else {}
    return jsonify({
        "focus_state":        cam.get("focus_state",        "absent"),
        "posture_state":      cam.get("posture_state",      "upright"),
        "focus_confidence":   cam.get("focus_confidence",   0.0),
        "posture_confidence": cam.get("posture_confidence", 0.0),
        "sensors":            sen,
        "camera_active":      sen.get("camera_active",      False),
        "session_active":     session.is_active if session else False,
        "timestamp":          datetime.now().isoformat(),
    })

@app.route("/session")
def get_session():
    return jsonify(session.get_current_session() if session else {})

@app.route("/session/start", methods=["POST"])
def start_session():
    if not session: return err("Session engine not ready")
    return ok({"session_id": session.start_session()})

@app.route("/session/stop", methods=["POST"])
def stop_session():
    if not session: return err("Session engine not ready")
    sid = session.stop_session()
    if behavioral: behavioral.refresh_now()
    return ok({"session_id": sid})

@app.route("/history")
def get_history():
    limit = request.args.get("limit", 20, type=int)
    return jsonify(session.get_history(limit=limit) if session else [])

@app.route("/history/<int:session_id>/events")
def get_events(session_id):
    return jsonify(session.get_events(session_id) if session else [])

@app.route("/insights")
def get_insights():
    return jsonify(behavioral.insights if behavioral else {})

@app.route("/camera/toggle", methods=["POST"])
def toggle_camera():
    if not sensors: return err("Sensor manager not ready")
    body   = request.get_json(silent=True) or {}
    active = body.get("active", not sensors.camera_active)
    sensors.set_camera_active(active)
    return ok({"camera_active": active})

@app.route("/")
def serve_dashboard():
    if DASHBOARD_DIR.exists():
        return send_from_directory(str(DASHBOARD_DIR), "index.html")
    return "<h2>SmartDesk OS API running</h2>", 200

@app.route("/<path:filename>")
def serve_static(filename):
    if DASHBOARD_DIR.exists():
        return send_from_directory(str(DASHBOARD_DIR), filename)
    return err("Not found", 404)

# ── Bridge state pusher ───────────────────────────────────────────────────────
# Runs in background — pushes MPU state to MCU Bridge keys so the
# display and LED ring always have fresh data without polling from MCU

def _bridge_push_loop():
    """Push current state to Bridge every 2 seconds for MCU display."""
    try:
        from bridge import Bridge as ArduinoBridge
        bridge = ArduinoBridge()
        bridge.begin(115200)
        print("[BRIDGE] Bridge push loop started")
    except ImportError:
        print("[BRIDGE] Bridge not available — skipping MCU state push")
        return

    while True:
        try:
            if camera and session:
                cam  = camera.state
                sess = session.get_current_session()
                ins  = behavioral.insights if behavioral else {}

                # Push focus/posture state to MCU display
                bridge.put("focus_state",       cam.get("focus_state", "absent"))
                bridge.put("posture_state",      cam.get("posture_state", "upright"))
                bridge.put("focus_score",        str(round(sess.get("focus_score", 0.0), 3)))
                bridge.put("distraction_count",  str(sess.get("distraction_count", 0)))
                bridge.put("session_active",     "1" if session.is_active else "0")
                bridge.put("session_secs",       str(sess.get("duration_seconds", 0)))
                bridge.put("burnout_risk",       str(round(ins.get("burnout_risk_score", 0.0), 3)))

                # Push current time for clock mode
                bridge.put("current_time", datetime.now().strftime("%H:%M"))

                # Check if TTP223 touch button was pressed
                toggle = bridge.get("touch_session_toggle")
                if toggle == "1":
                    bridge.put("touch_session_toggle", "")
                    if session.is_active:
                        sid = session.stop_session()
                        if behavioral: behavioral.refresh_now()
                        print(f"[BRIDGE] Touch → session {sid} stopped")
                    else:
                        sid = session.start_session()
                        print(f"[BRIDGE] Touch → session {sid} started")

        except Exception as e:
            print(f"[BRIDGE] Push error: {e}")

        time.sleep(2)

# ── UDP discovery ─────────────────────────────────────────────────────────────

def _discovery_listener():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", DISCOVERY_PORT))
        print(f"[API] Discovery listener on UDP:{DISCOVERY_PORT}")
        while True:
            data, addr = sock.recvfrom(1024)
            if data.decode().strip() == "SMARTDESK_DISCOVER":
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.connect(("8.8.8.8", 80))
                local_ip = s.getsockname()[0]
                s.close()
                sock.sendto(f"SMARTDESK_HERE:{local_ip}".encode(), addr)
                print(f"[API] Discovery → {addr[0]}")
    except Exception as e:
        print(f"[API] Discovery error: {e}")

# ── Boot ──────────────────────────────────────────────────────────────────────

def main():
    global sensors, session, camera, behavioral

    print("=" * 50)
    print("  SmartDesk OS — MPU Stack Starting")
    print("=" * 50)

    print("[BOOT] Sensor manager...")
    sensors = SensorManager()

    print("[BOOT] Session engine...")
    session = SessionEngine()

    print("[BOOT] Behavioral engine...")
    behavioral = BehavioralEngine()
    behavioral.start()

    print("[BOOT] Camera inference...")
    camera = CameraInference(session, sensors)
    camera.start()

    # Bridge push loop (only runs when Bridge is available on UNO Q)
    threading.Thread(target=_bridge_push_loop,    daemon=True).start()
    threading.Thread(target=_discovery_listener,  daemon=True).start()

    print(f"\n[BOOT] All systems nominal")
    print(f"[BOOT] Dashboard : http://localhost:{API_PORT}")
    print(f"[BOOT] API status: http://localhost:{API_PORT}/status\n")

    app.run(host="0.0.0.0", port=API_PORT, debug=False, use_reloader=False)

if __name__ == "__main__":
    main()
