"""
session_engine.py
─────────────────
Manages work session state and persists all events to a local SQLite database.
Tracks: session start/stop, focus scores, distraction events, posture nudges,
phone pickups, and absence periods.
"""

import sqlite3
import threading
from datetime import datetime
from pathlib import Path

DB_PATH = Path(__file__).parent / "smartdesk.db"

# ── Schema ────────────────────────────────────────────────────────────────────

SCHEMA = """
CREATE TABLE IF NOT EXISTS sessions (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    start_time          TEXT NOT NULL,
    end_time            TEXT,
    focus_score         REAL DEFAULT 0.0,
    focused_seconds     INTEGER DEFAULT 0,
    distracted_seconds  INTEGER DEFAULT 0,
    absent_seconds      INTEGER DEFAULT 0,
    distraction_count   INTEGER DEFAULT 0,
    phone_pickup_count  INTEGER DEFAULT 0,
    posture_nudge_count INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id  INTEGER NOT NULL,
    timestamp   TEXT NOT NULL,
    type        TEXT NOT NULL,
    value       TEXT,
    FOREIGN KEY (session_id) REFERENCES sessions(id)
);
"""

# ── SessionEngine ─────────────────────────────────────────────────────────────

class SessionEngine:
    def __init__(self):
        self._lock = threading.Lock()
        self._conn = sqlite3.connect(DB_PATH, check_same_thread=False)
        self._conn.row_factory = sqlite3.Row
        self._conn.executescript(SCHEMA)
        self._conn.commit()

        self._active_session_id = None
        self._session_start      = None
        self._focus_ticks        = 0   # 2-second ticks in focused state
        self._distracted_ticks   = 0
        self._absent_ticks       = 0
        self._distraction_count  = 0
        self._phone_pickup_count = 0
        self._posture_nudge_count = 0

        # hysteresis: track consecutive identical states
        self._last_focus_state  = "absent"
        self._consec_distracted = 0
        self._consec_absent     = 0

    # ── Session control ───────────────────────────────────────────────────────

    def start_session(self):
        with self._lock:
            if self._active_session_id:
                return self._active_session_id   # already running

            now = datetime.now().isoformat()
            cur = self._conn.execute(
                "INSERT INTO sessions (start_time) VALUES (?)", (now,))
            self._conn.commit()
            self._active_session_id  = cur.lastrowid
            self._session_start      = datetime.now()
            self._focus_ticks        = 0
            self._distracted_ticks   = 0
            self._absent_ticks       = 0
            self._distraction_count  = 0
            self._phone_pickup_count = 0
            self._posture_nudge_count = 0
            self._consec_distracted  = 0
            self._consec_absent      = 0
            print(f"[SESSION] Started session {self._active_session_id}")
            return self._active_session_id

    def stop_session(self):
        with self._lock:
            if not self._active_session_id:
                return None

            total = self._focus_ticks + self._distracted_ticks + self._absent_ticks
            focus_score = (self._focus_ticks / total) if total > 0 else 0.0

            now = datetime.now().isoformat()
            self._conn.execute("""
                UPDATE sessions SET
                    end_time            = ?,
                    focus_score         = ?,
                    focused_seconds     = ?,
                    distracted_seconds  = ?,
                    absent_seconds      = ?,
                    distraction_count   = ?,
                    phone_pickup_count  = ?,
                    posture_nudge_count = ?
                WHERE id = ?
            """, (
                now, focus_score,
                self._focus_ticks * 2,
                self._distracted_ticks * 2,
                self._absent_ticks * 2,
                self._distraction_count,
                self._phone_pickup_count,
                self._posture_nudge_count,
                self._active_session_id,
            ))
            self._conn.commit()
            sid = self._active_session_id
            self._active_session_id = None
            print(f"[SESSION] Ended session {sid} — focus score: {focus_score:.2f}")
            return sid

    # ── Called every 2 seconds by camera_inference ────────────────────────────

    def record_tick(self, focus_state: str, posture_state: str):
        """Record one 2-second inference tick."""
        if not self._active_session_id:
            return

        with self._lock:
            # Count ticks
            if focus_state == "focused":
                self._focus_ticks += 1
                self._consec_distracted = 0
                self._consec_absent     = 0
            elif focus_state == "distracted":
                self._distracted_ticks  += 1
                self._consec_distracted += 1
                self._consec_absent      = 0
            elif focus_state == "absent":
                self._absent_ticks      += 1
                self._consec_absent     += 1
                self._consec_distracted  = 0

            # Log distraction event after 3 consecutive distracted ticks (6 s)
            if self._consec_distracted == 3:
                self._distraction_count += 1
                self._log_event("distraction", focus_state)

            # Log absence event after 90 consecutive absent ticks (3 min)
            if self._consec_absent == 90:
                self._log_event("absence", "3min+")

            self._last_focus_state = focus_state

            # Persist running totals every 30 ticks (60 s)
            total = self._focus_ticks + self._distracted_ticks + self._absent_ticks
            if total % 30 == 0:
                focus_score = (self._focus_ticks / total) if total > 0 else 0.0
                self._conn.execute("""
                    UPDATE sessions SET
                        focus_score        = ?,
                        focused_seconds    = ?,
                        distracted_seconds = ?,
                        absent_seconds     = ?,
                        distraction_count  = ?,
                        phone_pickup_count = ?,
                        posture_nudge_count= ?
                    WHERE id = ?
                """, (
                    focus_score,
                    self._focus_ticks * 2,
                    self._distracted_ticks * 2,
                    self._absent_ticks * 2,
                    self._distraction_count,
                    self._phone_pickup_count,
                    self._posture_nudge_count,
                    self._active_session_id,
                ))
                self._conn.commit()

    def record_phone_pickup(self):
        with self._lock:
            if not self._active_session_id:
                return
            self._phone_pickup_count += 1
            self._log_event("phone_pickup", "detected")

    def record_posture_nudge(self, posture_state: str):
        with self._lock:
            if not self._active_session_id:
                return
            self._posture_nudge_count += 1
            self._log_event("posture_nudge", posture_state)

    def _log_event(self, event_type: str, value: str):
        """Internal — must be called while holding self._lock."""
        self._conn.execute(
            "INSERT INTO events (session_id, timestamp, type, value) VALUES (?,?,?,?)",
            (self._active_session_id, datetime.now().isoformat(), event_type, value),
        )
        self._conn.commit()

    # ── Read helpers for API ──────────────────────────────────────────────────

    def get_current_session(self) -> dict:
        if not self._active_session_id:
            return {}
        total = self._focus_ticks + self._distracted_ticks + self._absent_ticks
        focus_score = (self._focus_ticks / total) if total > 0 else 0.0
        duration = (datetime.now() - self._session_start).seconds if self._session_start else 0
        return {
            "id":                  self._active_session_id,
            "start_time":          self._session_start.isoformat() if self._session_start else None,
            "focus_score":         round(focus_score, 3),
            "focused_seconds":     self._focus_ticks * 2,
            "distracted_seconds":  self._distracted_ticks * 2,
            "absent_seconds":      self._absent_ticks * 2,
            "distraction_count":   self._distraction_count,
            "phone_pickup_count":  self._phone_pickup_count,
            "posture_nudge_count": self._posture_nudge_count,
            "duration_seconds":    duration,
        }

    def get_history(self, limit: int = 20) -> list:
        rows = self._conn.execute(
            "SELECT * FROM sessions WHERE end_time IS NOT NULL ORDER BY start_time DESC LIMIT ?",
            (limit,)
        ).fetchall()
        return [dict(r) for r in rows]

    def get_events(self, session_id: int) -> list:
        rows = self._conn.execute(
            "SELECT * FROM events WHERE session_id = ? ORDER BY timestamp ASC",
            (session_id,)
        ).fetchall()
        return [dict(r) for r in rows]

    @property
    def is_active(self) -> bool:
        return self._active_session_id is not None

    @property
    def active_session_id(self):
        return self._active_session_id
