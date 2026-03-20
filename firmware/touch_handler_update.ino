// ── Touch button handler (in smartdesk_mcu.ino) ──────────────────────────────
// Replace the handleTouch() function with this version
// Single tap  → start/stop session
// Long press (2 seconds) → toggle camera on/off
// Double tap  → cycle display mode (alternative to encoder)

void handleTouch() {
  static unsigned long pressStart    = 0;
  static bool          pressing      = false;
  static unsigned long lastTap       = 0;
  static uint8_t       tapCount      = 0;
  static bool          longFired     = false;

  bool touched = digitalRead(PIN_TOUCH) == HIGH;
  unsigned long now = millis();

  // Debounce gap
  if (touched && !touchLast && (now - touchDebounce > 300)) {
    // Finger just touched down
    pressing     = true;
    pressStart   = now;
    longFired    = false;
    touchDebounce = now;
  }

  if (pressing && touched) {
    // Still holding — check for long press (2000ms)
    if (!longFired && (now - pressStart >= 2000)) {
      longFired = true;
      // LONG PRESS → toggle camera
      Bridge.put("touch_camera_toggle", "1");
      playSfx(SFX_CAMERA_ON);   // will play off or on depending on state
      // Visual feedback: flash ring purple
      setLedState("flash", "purple");
    }
  }

  if (!touched && touchLast) {
    // Finger just lifted
    if (pressing && !longFired) {
      // Was a short press — register as tap
      unsigned long pressDuration = now - pressStart;
      if (pressDuration < 2000) {
        // Check for double tap
        if (now - lastTap < 400 && tapCount == 1) {
          // DOUBLE TAP → cycle display mode
          tapCount = 0;
          displayMode = (displayMode + 1) % 4;
          tft.fillScreen(C_BG);
          drawModeHeader();
          playSfx(SFX_MODE_CHANGE);
        } else {
          // SINGLE TAP → session toggle
          tapCount  = 1;
          lastTap   = now;
          Bridge.put("touch_session_toggle", "1");
          playSfx(SFX_TOUCH);
        }
      }
    }
    pressing = false;
  }

  // Reset tap count after 400ms of no second tap
  if (tapCount == 1 && (now - lastTap > 400)) {
    tapCount = 0;
  }

  touchLast = touched;
}
