"""
api_server.py — Flask REST API + App Lab dashboard + UDP discovery
Run with: python api_server.py
"""
import json, socket, threading
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
DASHBOARD_DIR = Path(__file__).parent / "dashboard"
API_PORT, DISCOVERY_PORT = 5000, 5001
sensors = session = camera = behavioral = None

def ok(data=None): return jsonify({"status": "ok", **({"data": data} if data else {})})
def err(msg, code=400): return jsonify({"status": "error", "message": msg}), code

@app.route("/status")
def get_status():
    cam = camera.state if camera else {}
    sen = sensors.current if sensors else {}
    return jsonify({
        "focus_state": cam.get("focus_state", "absent"),
        "posture_state": cam.get("posture_state", "upright"),
        "focus_confidence": cam.get("focus_confidence", 0.0),
        "posture_confidence": cam.get("posture_confidence", 0.0),
        "sensors": sen, "camera_active": sen.get("camera_active", False),
        "session_active": session.is_active if session else False,
        "timestamp": datetime.now().isoformat(),
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
    body = request.get_json(silent=True) or {}
    active = body.get("active", not sensors.camera_active)
    sensors.set_camera_active(active)
    return ok({"camera_active": active})

@app.route("/")
def serve_dashboard():
    if DASHBOARD_DIR.exists():
        return send_from_directory(str(DASHBOARD_DIR), "index.html")
    return "<h2>SmartDesk OS API running</h2>", 200

def _discovery_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", DISCOVERY_PORT))
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            if data.decode().strip() == "SMARTDESK_DISCOVER":
                s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                s.connect(("8.8.8.8", 80))
                local_ip = s.getsockname()[0]
                s.close()
                sock.sendto(f"SMARTDESK_HERE:{local_ip}".encode(), addr)
        except Exception as e:
            print(f"[API] Discovery error: {e}")

def main():
    global sensors, session, camera, behavioral
    print("SmartDesk OS — MPU Stack Starting")
    sensors = SensorManager()
    session = SessionEngine()
    behavioral = BehavioralEngine()
    behavioral.start()
    camera = CameraInference(session, sensors)
    camera.start()
    threading.Thread(target=_discovery_listener, daemon=True).start()
    print(f"Dashboard: http://0.0.0.0:{API_PORT}")
    app.run(host="0.0.0.0", port=API_PORT, debug=False, use_reloader=False)

if __name__ == "__main__":
    main()
