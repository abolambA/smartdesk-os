"""
behavioral_engine.py
─────────────────────
Analyzes historical session data from SQLite to generate behavioral insights:
  - Personal attention curve (typical focus duration before fatigue)
  - Peak productivity hours heatmap (24-hour)
  - Burnout risk score (requires 14+ days of data)
  - Phone pickup frequency trends
  - Weekly comparison metrics
  - Adaptive break timing

Runs as a background process, updating insights every 60 minutes.
"""

import threading
import time
import sqlite3
from datetime import datetime, timedelta
from pathlib import Path
from collections import defaultdict

DB_PATH = Path(__file__).parent / "smartdesk.db"

REFRESH_INTERVAL    = 3600   # seconds (1 hour)
MIN_DAYS_FOR_BURNOUT = 14    # minimum days of data needed
MIN_DAYS_FOR_BREAKS  = 7     # minimum days needed for adaptive breaks

# ── BehavioralEngine ──────────────────────────────────────────────────────────

class BehavioralEngine:
    def __init__(self):
        self._lock    = threading.Lock()
        self._running = False
        self._thread  = None
        self._conn    = sqlite3.connect(DB_PATH, check_same_thread=False)
        self._conn.row_factory = sqlite3.Row

        # Cached insights (updated hourly)
        self._insights = self._default_insights()

        # Run once immediately, then on a timer
        self._compute()

    def start(self):
        if self._running:
            return
        self._running = True
        self._thread = threading.Thread(target=self._refresh_loop, daemon=True)
        self._thread.start()
        print("[BEHAVIORAL] Engine started")

    def stop(self):
        self._running = False

    # ── Background refresh ────────────────────────────────────────────────────

    def _refresh_loop(self):
        while self._running:
            time.sleep(REFRESH_INTERVAL)
            self._compute()

    def _compute(self):
        try:
            sessions = self._load_sessions()
            if not sessions:
                return

            insights = {
                "burnout_risk_score":           self._burnout_risk(sessions),
                "adaptive_break_minutes":       self._adaptive_break(sessions),
                "weekly_scores":                self._weekly_scores(sessions),
                "peak_hours":                   self._peak_hours(sessions),
                "avg_focus_score":              self._avg_focus(sessions),
                "total_focused_hours_this_week":self._focused_hours_this_week(sessions),
                "phone_pickups_this_week":      self._phone_pickups_this_week(sessions),
                "burnout_trend":                self._burnout_trend(sessions),
                "computed_at":                  datetime.now().isoformat(),
            }
            with self._lock:
                self._insights = insights
            print(f"[BEHAVIORAL] Insights updated — burnout risk: {insights['burnout_risk_score']:.2f}")
        except Exception as e:
            print(f"[BEHAVIORAL] Compute error: {e}")

    # ── Data loading ──────────────────────────────────────────────────────────

    def _load_sessions(self) -> list:
        rows = self._conn.execute(
            "SELECT * FROM sessions WHERE end_time IS NOT NULL ORDER BY start_time ASC"
        ).fetchall()
        return [dict(r) for r in rows]

    def _sessions_last_n_days(self, sessions: list, days: int) -> list:
        cutoff = (datetime.now() - timedelta(days=days)).isoformat()
        return [s for s in sessions if s["start_time"] >= cutoff]

    # ── Burnout risk score (0.0 – 1.0) ───────────────────────────────────────

    def _burnout_risk(self, sessions: list) -> float:
        recent = self._sessions_last_n_days(sessions, MIN_DAYS_FOR_BURNOUT)
        if len(recent) < 3:
            return 0.0   # not enough data

        risk = 0.0
        signals = 0

        # Signal 1: declining focus score over last 5 days
        last5 = self._sessions_last_n_days(sessions, 5)
        if len(last5) >= 3:
            scores = [s["focus_score"] for s in last5 if s["focus_score"] is not None]
            if len(scores) >= 3:
                # Simple linear regression slope
                n = len(scores)
                slope = (n * sum(i * scores[i] for i in range(n))
                         - sum(range(n)) * sum(scores)) / \
                        (n * sum(i**2 for i in range(n)) - sum(range(n))**2 + 1e-9)
                if slope < -0.02:   # declining more than 2% per session
                    risk += 0.35
                signals += 1

        # Signal 2: worsening focus/distracted ratio week-over-week
        this_week = self._sessions_last_n_days(sessions, 7)
        last_week = [s for s in sessions if
                     (datetime.now() - timedelta(days=14)).isoformat() <= s["start_time"] <
                     (datetime.now() - timedelta(days=7)).isoformat()]
        if this_week and last_week:
            this_avg = sum(s["focus_score"] or 0 for s in this_week) / len(this_week)
            last_avg = sum(s["focus_score"] or 0 for s in last_week) / len(last_week)
            if this_avg < last_avg - 0.05:
                risk += 0.35
            signals += 1

        # Signal 3: increasing absence/distraction mid-session
        recent_distractions = [s["distraction_count"] or 0 for s in last5]
        if len(recent_distractions) >= 3:
            if recent_distractions[-1] > sum(recent_distractions[:-1]) / (len(recent_distractions) - 1) * 1.3:
                risk += 0.30
            signals += 1

        return round(min(risk, 1.0), 3)

    # ── Adaptive break timing (minutes) ──────────────────────────────────────

    def _adaptive_break(self, sessions: list) -> int:
        recent = self._sessions_last_n_days(sessions, MIN_DAYS_FOR_BREAKS)
        if len(recent) < 3:
            return 25  # default Pomodoro fallback

        # Find average time to first distraction event in each session
        first_distraction_times = []
        for s in recent:
            events = self._conn.execute(
                "SELECT timestamp FROM events WHERE session_id=? AND type='distraction' ORDER BY timestamp ASC LIMIT 1",
                (s["id"],)
            ).fetchone()
            if events and s["start_time"]:
                try:
                    start = datetime.fromisoformat(s["start_time"])
                    first_dist = datetime.fromisoformat(events["timestamp"])
                    minutes = (first_dist - start).total_seconds() / 60
                    if 5 < minutes < 120:  # sanity bounds
                        first_distraction_times.append(minutes)
                except Exception:
                    pass

        if not first_distraction_times:
            return 25

        avg_minutes = sum(first_distraction_times) / len(first_distraction_times)
        return max(15, min(90, round(avg_minutes)))

    # ── Weekly scores (last 7 days, one score per day) ───────────────────────

    def _weekly_scores(self, sessions: list) -> list:
        scores = []
        for days_ago in range(6, -1, -1):   # oldest first
            day_start = (datetime.now() - timedelta(days=days_ago)).replace(
                hour=0, minute=0, second=0, microsecond=0).isoformat()
            day_end   = (datetime.now() - timedelta(days=days_ago)).replace(
                hour=23, minute=59, second=59).isoformat()
            day_sessions = [s for s in sessions
                            if day_start <= s["start_time"] <= day_end]
            if day_sessions:
                avg = sum(s["focus_score"] or 0 for s in day_sessions) / len(day_sessions)
                scores.append(round(avg, 3))
            else:
                scores.append(0.0)
        return scores

    # ── Peak hours heatmap (24 values, index = hour of day) ──────────────────

    def _peak_hours(self, sessions: list) -> list:
        hour_scores  = defaultdict(list)
        for s in sessions:
            try:
                hour = datetime.fromisoformat(s["start_time"]).hour
                if s["focus_score"] is not None:
                    hour_scores[hour].append(s["focus_score"])
            except Exception:
                pass

        result = []
        for h in range(24):
            vals = hour_scores.get(h, [])
            result.append(round(sum(vals) / len(vals), 3) if vals else 0.0)
        return result

    # ── Aggregate helpers ─────────────────────────────────────────────────────

    def _avg_focus(self, sessions: list) -> float:
        recent = self._sessions_last_n_days(sessions, 30)
        if not recent:
            return 0.0
        vals = [s["focus_score"] for s in recent if s["focus_score"] is not None]
        return round(sum(vals) / len(vals), 3) if vals else 0.0

    def _focused_hours_this_week(self, sessions: list) -> int:
        recent = self._sessions_last_n_days(sessions, 7)
        total_seconds = sum(s["focused_seconds"] or 0 for s in recent)
        return round(total_seconds / 3600)

    def _phone_pickups_this_week(self, sessions: list) -> int:
        recent = self._sessions_last_n_days(sessions, 7)
        return sum(s["phone_pickup_count"] or 0 for s in recent)

    def _burnout_trend(self, sessions: list) -> str:
        recent = self._sessions_last_n_days(sessions, 14)
        if len(recent) < 4:
            return "stable"
        mid  = len(recent) // 2
        first_half_avg = sum(s["focus_score"] or 0 for s in recent[:mid]) / mid
        second_half_avg = sum(s["focus_score"] or 0 for s in recent[mid:]) / (len(recent) - mid)
        diff = second_half_avg - first_half_avg
        if diff > 0.05:
            return "improving"
        elif diff < -0.05:
            return "declining"
        return "stable"

    # ── Default insights (before enough data) ────────────────────────────────

    def _default_insights(self) -> dict:
        return {
            "burnout_risk_score":            0.0,
            "adaptive_break_minutes":        25,
            "weekly_scores":                 [0.0] * 7,
            "peak_hours":                    [0.0] * 24,
            "avg_focus_score":               0.0,
            "total_focused_hours_this_week": 0,
            "phone_pickups_this_week":       0,
            "burnout_trend":                 "stable",
            "computed_at":                   None,
        }

    # ── Public API ────────────────────────────────────────────────────────────

    @property
    def insights(self) -> dict:
        with self._lock:
            return dict(self._insights)

    def refresh_now(self):
        """Force an immediate recompute (called after session ends)."""
        threading.Thread(target=self._compute, daemon=True).start()
