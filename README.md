# Void Clock - Minimalist High-Contrast Watchface

Born from the legacy of ClockLight (a decade in the making), Void Clock is
a high-contrast minimalist watchface designed for the focused. It strips
away the noise to display only what matters: the time, date, weekday,
battery level, and Bluetooth connection status.

Clean, essential, and relentlessly functional.

## Screenshots

**Normal State:**
![Normal](screenshot_normal.png)

**Bluetooth Disconnected:**
![No Bluetooth](screenshot_bt.png)

**Battery Empty:**
![Empty Battery](screenshot_battery.png)

## Changelog

### 1.0.2 - Redesigned Warning Icons

- **Changed** Bluetooth disconnected icon: bigger (24x32), bolder strokes,
  two-color design — black phone shape with red diagonal slash.
- **Changed** Empty battery icon: bigger (24x18), bolder strokes,
  two-color design — black battery outline with red X.
- **Changed** icon layer positions in `src/layers.c` to accommodate
  larger dimensions.

### 1.0.1 - Bluetooth Connection Stability Hotfix

- **Added** 15-second Bluetooth disconnect debounce to prevent spurious
  "no Bluetooth" icon flashing caused by PebbleOS standby mode
  power-management (especially on Pebble Time 2 / Emery).
- **Added** live re-check in the debounce timer — verifies the actual
  connection state before showing the icon.
- **Removed** per-minute Bluetooth resync from the time tick to respect
  PebbleOS v4.30.0 "fewer background wakeups" battery optimization.
- **Fixed** missing `connection_service_unsubscribe()` in window unload
  (resource leak).
- **Fixed** missing `bt_debounce_cancel()` teardown — pending timers are
  now cancelled on app exit.
- **Removed** unused `setToReady()` / `isInitialized` state (dead code
  cleanup).
- **Added** inline firmware timeline comments documenting the root cause
  across PebbleOS releases (core35 → v4.9.175 → v4.31.1).

### 1.0.0 - Initial Release

- Migrated from ClockLight project.
- Emery (Pebble Time 2) platform support only.
- Displays time, date, weekday, battery level, and Bluetooth connection
  status.
- High-contrast LECO 60 font for time; custom Milford font for
  date/weekday.
- Battery bar and empty-battery indicator.
- No-Bluetooth icon with emulator test scripts.
