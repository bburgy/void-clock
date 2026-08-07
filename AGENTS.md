# Agent Guide — Void Clock

> **Before making changes**, check the latest PebbleOS changelog at https://ndocs.repebble.com/pebbleos-changelog. Firmware behavior evolves and may invalidate assumptions in this document.

---

## 1. Project Overview

**Void Clock** is a native C Pebble SDK 3 watchface targeting **Emery (Pebble Time 2 / PT2)** exclusively.

| Attribute | Value                                                           |
| --------- | --------------------------------------------------------------- |
| Language  | C (native, no JavaScript/Clay)                                  |
| Platform  | Emery only (`targetPlatforms: ["emery"]`)                       |
| Display   | Time, date, weekday, battery bar, Bluetooth connection icon     |
| Fonts     | LECO 60 (system) for time; Milford 30 (custom) for date/weekday |
| Version   | 1.0.1                                                           |

---

## 2. Architecture Decision Records (ADRs)

### ADR-1: Bluetooth debounce = 15 seconds

**Context:** The real bug is in the firmware, not this watchface.

- **core35** (late 2025): Standby mode enabled by default — the root cause of spurious BLE disconnections.
- **v4.9.175** (May 2026): "Speculative fix" for standby threshold causing spurious BT disconnects on PT2.
- **v4.31.2** (Jul 2026): Several power consumption bug fixes — BLE/standby further stabilized.
- **v4.33.0** (Aug 2026): Fixed fast-advertising stickiness after airplane mode.

The firmware itself debounces internally for ~25 s. A 15 s app-layer delay catches remaining edge-of-range twitches without hiding real disconnections from the user.

**Tunable values:**

| Value   | Use case                                                           |
| ------- | ------------------------------------------------------------------ |
| `0`     | No debounce; instant feedback (latest firmware only)               |
| `15000` | RECOMMENDED: catches brief twitches, fast real-disconnect feedback |
| `60000` | Maximum smoothing for very old firmware (overkill post-v4.31)      |

**Implementation:** `BLUETOOTH_DISCONNECT_DEBOUNCE_MS` in `src/layers.c`. The debounce callback (`bluetooth_debounce_callback()`) re-checks the **live** connection state (`connection_service_peek_pebble_app_connection()`) before showing the icon, so a stale callback value never causes a false alarm.

### ADR-2: NO per-minute Bluetooth polling

**Context:** PebbleOS v4.30.0 explicitly optimizes for "fewer background wakeups" to improve battery life.

`connection_service` callbacks are reliable; we trust them completely. Adding a `connection_service_peek_pebble_app_connection()` call inside `handle_minute()` would:

1. Fight against the firmware's own battery optimizations.
2. Add unnecessary CPU work 1,440 times/day (every minute tick).
3. Create a second code path mutating Bluetooth state, increasing complexity.

**The dead-end that was tried and removed:**

An earlier version of this branch had `bt_sync_status()` — a helper called from `handle_minute()` every 60 seconds to "prevent the icon from getting stuck." It was removed because:

- The scenario it protected against (missed connection callback) is theoretical — `connection_service` callbacks are delivered reliably by the OS.
- The debounce timer callback already re-checks live state before showing the icon — this is the correct place for a safety check (runs only on disconnect, not every minute).
- Battery efficiency concern raised during code review.

**Do not re-add per-minute polling.** If you believe a resync is necessary, use a conditional check (`bluetooth_icon_shown == true`) or a much longer AppTimer (e.g., 5 minutes), not the minute tick.

### ADR-3: No Hungarian notation

| ❌ Old                      | ✅ New                             |
| --------------------------- | ---------------------------------- |
| `ptr_time_layer`            | `time_layer`                       |
| `ptr_battery_layer`         | `battery_layer`                    |
| `s_bt_debounce_timer`       | `bluetooth_debounce_timer`         |
| `bt_debounce_cancel()`      | `bluetooth_debounce_cancel()`      |
| `BT_DISCONNECT_DEBOUNCE_MS` | `BLUETOOTH_DISCONNECT_DEBOUNCE_MS` |

Static variables do NOT get a `s_` prefix. Module functions use the full module name (`bluetooth_`, not `bt_`).

---

## 3. PebbleOS Firmware Timeline (Relevant to This Watchface)

| Version  | Date      | Relevant Change                                                   |
| -------- | --------- | ----------------------------------------------------------------- |
| core35   | late 2025 | Standby mode enabled by default — **ROOT CAUSE** of BT flapping   |
| v4.9.163 | Apr 2026  | Fixed infinite disconnect/connect loop on iOS                     |
| v4.9.175 | May 2026  | Standby threshold reduced; "speculative fix" for PT2 disconnects  |
| core31   | —         | Adjusted BLE advertising/connection parameters (Apple guidelines) |
| v4.12.0  | Jun 2026  | BLE-only advertising (BR/EDR not supported)                       |
| v4.30.0  | Jul 2026  | "Fewer background wakeups" battery optimization                   |
| v4.31.2  | Jul 2026  | Several power consumption bug fixes                               |
| v4.33.0  | Aug 2026  | Fixed fast-advertising stickiness after airplane mode             |

---

## 4. File Structure

```
void-clock/
├── src/
│   ├── main.c          # App entry point, service subscriptions, lifecycle
│   ├── layers.c        # All UI rendering and Bluetooth debounce logic
│   └── layers.h        # Shared declarations
├── resources/
│   ├── noBluetooth.pdc # BT disconnected icon (Pebble Draw Command)
│   ├── emptyBattery.pdc# Empty battery icon
│   └── MilfordCondensed-BG1w.ttf
├── emu-*.sh            # Emulator helper scripts (see §5)
├── wscript             # Pebble SDK build rules
├── package.json        # App metadata (version, UUID, resources)
└── README.md           # User-facing documentation + changelog
```

### Key source files

**`src/main.c`**

- `window_load()`: Subscribes `tick_timer`, `battery_state`, `connection_service`. Peeks initial BT state.
- `window_unload()`: Unsubscribes all services + calls `bluetooth_debounce_cancel()`.
- No `setToReady()` — it was dead code and was removed.

**`src/layers.c`**

- `bluetooth_debounce_callback()`: Called after `BLUETOOTH_DISCONNECT_DEBOUNCE_MS`. Re-checks live BT state before showing icon.
- `handle_app_connection_handler()`: Event-driven. Hides icon immediately on connect; starts debounce timer on disconnect (guarded against re-arming).
- `handle_minute()`: Updates time only. NO Bluetooth work.

---

## 5. Build & Test

### Build

```bash
pebble build
```

Produces `build/void-clock.pbw`.

### Emulator commands

```bash
# Start emulator (interactive QEMU window)
pebble emu-control --emulator emery

# Build + install
pebble build && pebble install --emulator emery

# Stream debug logs (run in separate terminal)
pebble logs --emulator emery

# Bluetooth state toggles
pebble emu-bt-connection --emulator emery --connected yes
pebble emu-bt-connection --emulator emery --connected no

# Battery test
pebble emu-battery --emulator emery --percent 5   # triggers empty battery icon
```

### Pre-release checklist

- [ ] `pebble build` succeeds with no errors
- [ ] `package.json` `"version"` bumped
- [ ] `README.md` changelog updated
- [ ] Emulator: disconnect → wait 15s → "no BT" icon appears
- [ ] Emulator: disconnect → reconnect within 15s → icon NEVER appears
- [ ] Emulator: rapid connect/disconnect flapping → no flickering
- [ ] Emulator: disconnect → wait 15s (icon shown) → reconnect → icon hides immediately

---

## 6. Changelog Conventions

- Use `## Changelog` section in `README.md`.
- Format: `### X.Y.Z - Description`.
- Prefix items with `Added`, `Fixed`, `Removed`, `Changed`.
- Link firmware context where relevant.

---

_This document should be updated whenever PebbleOS firmware changes affect Bluetooth behavior or when new architectural decisions are made._
