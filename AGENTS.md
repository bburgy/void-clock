# Agent Guide — Void Clock

> **Before making changes**, check the latest PebbleOS changelog at https://ndocs.repebble.com/pebbleos-changelog. Firmware behavior evolves and may invalidate assumptions in this document.

---

## 1. Project Overview

**Void Clock** is a native C Pebble SDK 3 watchface targeting **Emery (Pebble Time 2 / PT2)** exclusively.

| Attribute | Value                                                           |
| --------- | --------------------------------------------------------------- |
| Language  | C (native, no JavaScript/Clay)                                  |
| Platform  | Emery only (`targetPlatforms: ["emery"]`)                       |
| Display   | Time, date, weekday, battery bar, Bluetooth, Quiet-Time, alarm indicators |
| Fonts     | LECO 60 (system) for time; Milford 30 (custom) for date/weekday |
| Version   | 1.0.5                                                           |

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

### ADR-4: Quiet Time polling = once per minute

**Context:** Pebble SDK v4.33 provides `quiet_time_is_active()` but no
subscription callback analogous to `connection_service` or `battery_state_service`.
There is no `quiet_time_service_subscribe()`.

**Decision:** Check `quiet_time_is_active()` inside `handle_minute()`, i.e. once
per minute.

**Rationale:**
- No OS event-driven alternative exists; polling is the only mechanism.
- The cost is a single boolean function call per minute — negligible compared
  to the time-formatting work already done in the same tick handler.
- This is consistent with ADR-2's trust-in-callbacks philosophy applied to a
  subsystem where the OS gives us no callback at all.
- Visual latency of ≤ 59 s is acceptable for a user-toggled quiet-mode
  indicator.

---

### ADR-5: Alarm polling = once per minute

**Context:** Pebble SDK v4.33 provides `alarm_service_peek_next()` on Emery,
but no subscription callback analogous to `connection_service` or
`battery_state_service`. There is no `alarm_service_subscribe()`.

**Decision:** Check `alarm_service_peek_next()` inside `status_update_icons()`,
i.e. once per minute (called from `handle_minute()`).

**Rationale:**
- No OS event-driven alternative exists; polling is the only mechanism.
- The cost is a single boolean function call per minute — negligible compared
  to the time-formatting work already done in the same tick handler.
- This is consistent with ADR-4's quiet-time polling pattern applied to a
  subsystem where the OS gives us no callback at all.
- Visual latency of ≤ 59 s is acceptable for an alarm-schedule indicator.

**Additional safeguard:** `app_focus_service_subscribe()` is used so that when
a user exits a system menu (where they may have changed alarms), the watchface
updates immediately on regaining focus without waiting for the next minute tick.

---

### ADR-6: Source code modularization

**Context:** `src/layers.c` grew to ~600 lines mixing text rendering, procedural
icon drawing, state machines, debounce timers, and OS callback handlers. Adding
a fourth subsystem (alarm) would push it past what a single file should carry.

**Decision:** Split into three modules:

| File | Responsibility |
|------|--------------|
| `src/datetime.c` | Time/date/weekday text layers, line separator, Milford 30 font |
| `src/icons.c` | Procedural icon drawing, layer creation, visibility toggling |
| `src/status.c` | State machines, debounce timers, polling (quiet time, alarm) |

**Rationale:**
- Each file has a single, well-defined responsibility.
- Adding a new icon or status no longer requires editing a monolithic file.
- `main.c` remains a thin shell — event routing only — following the
  Functional Core, Imperative Shell pattern.
- No build-system changes required; `wscript` already uses
  `ctx.path.ant_glob('src/**/*.c')`.
- Follows Pebble community conventions and SDK template guidance.

---

### ADR-7: Inline comment policy

**Context:** The codebase accumulated verbose inline comments (`// Drawing...`,
`// Done.`, `#ifdef PBL_DEBUG APP_LOG(...) #endif`) that explained what the
code did rather than why decisions were made.

**Decision:** Strip inline comments aggressively. Preserve only non-obvious
technical constraints (e.g., `// 2 px bleed margin for anti-aliasing`). Move
all architectural rationale into ADRs inside `AGENTS.md`.

**Rationale:**
- Well-named functions and variables should say *what* the code does.
- `AGENTS.md` is the canonical place for *why* a decision was made.
- Debug-logging blocks (`APP_LOG`) for mechanical operations (`// Drawing...`,
  `// Done.`) are prohibited. Selective high-signal logging for state
  transitions (BT connect/disconnect, debounce timer fire, status polling)
  may be preserved under `#ifdef PBL_DEBUG` for emulator debugging.
- Cleaner C source is easier to read and maintain.

## 3. PebbleOS Firmware Timeline (Relevant to This Watchface)

> **Note:** This timeline was last updated August 2026. Check the latest
> PebbleOS changelog at https://ndocs.repebble.com/pebbleos-changelog before
> making changes, as firmware behavior continues to evolve.

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
│   ├── main.c          # Thin shell: service subscriptions, lifecycle
│   ├── datetime.c      # Time, date, weekday text layers + line separator
│   ├── datetime.h
│   ├── icons.c         # Procedural icon drawing (all status icons)
│   ├── icons.h
│   ├── status.c        # State machines, debounce timers, quiet-time/alarm polling
│   └── status.h
├── resources/
│   ├── silentMode.svg  # SVG reference for the quiet mode icon
│   │                     (all status icons are drawn procedurally in src/icons.c)
│   └── MilfordCondensed-BG1w.ttf
├── screenshots/        # Store assets and README images
│   ├── emery_screenshot_normal.png
│   ├── emery_screenshot_bt.png
│   ├── emery_screenshot_battery.png
│   └── emery_screenshot_quiet.png
├── emu-*.sh            # Emulator helper scripts (see §5)
├── wscript             # Pebble SDK build rules
├── package.json        # App metadata (version, UUID, resources)
├── AGENTS.md           # This document: architecture guide and decisions
├── .gitignore          # Build artifacts to ignore
└── README.md           # User-facing documentation + changelog
```

### Key source files

**`src/main.c`**

- `window_load()`: Subscribes `tick_timer`, `battery_state`, `connection_service`, and `app_focus_service`. Peeks initial battery, BT, and quiet-time/alarm states.
- `window_unload()`: Unsubscribes all services + calls `status_deinit()`.
- Thin event-routing handlers delegate to `datetime.c` and `status.c`.

**`src/datetime.c`**

- `datetime_update()`: Formats time, date, weekday strings and updates text layers.
- `datetime_layers_create() / destroy()`: Manages time/date/weekday text layers, line separator, and Milford 30 font lifetime.

**`src/icons.c`**

- `draw_*_callback()`: Procedural drawing routines for all status icons (Bluetooth, empty battery, quiet mode, alarm).
- `icons_layers_create() / destroy()`: Manages icon layer creation and cleanup.
- `icons_set_*_shown()`: Visibility toggles called by `status.c`.

**`src/status.c`**

- `bluetooth_debounce_callback()`: Called after `BLUETOOTH_DISCONNECT_DEBOUNCE_MS`. Re-checks live BT state before showing icon.
- `status_handle_bluetooth()`: Event-driven. Hides icon immediately on connect; starts debounce timer on disconnect.
- `status_update_icons()`: Polls quiet-time and alarm state. Called once per minute from `handle_minute()` and on `app_focus_service` regain-focus events.

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
pebble emu-battery --emulator emery --percent 9   # triggers empty battery icon
```

### Pre-release checklist

- [ ] `pebble build` succeeds with no errors
- [ ] `package.json` `"version"` bumped
- [ ] `README.md` changelog updated
- [ ] Emulator: disconnect → wait 15s → "no BT" icon appears
- [ ] Emulator: disconnect → reconnect within 15s → icon NEVER appears
- [ ] Emulator: rapid connect/disconnect flapping → no flickering
- [ ] Emulator: disconnect → wait 15s (icon shown) → reconnect → icon hides immediately
- [ ] Emulator: set the battery level smaller than 10 percent -> "EMPTY_BATTERY" icon appears
- [ ] Emulator: set the battery level bigger or equals than 10 percent -> "EMPTY_BATTERY" icon should not appears
- [ ] Emulator: quiet time ON -> "Zzz" icon appears at top-left of status cluster without overlapping time/date/BT layers
- [ ] Emulator: quiet time OFF -> "Zzz" icon hides immediately
- [ ] Emulator: alarm ON -> alarm clock icon appears at top-left of status cluster (x=63) with red hands, without overlapping time/date/BT layers (verify with `screenshots/emery_screenshot_alarm.png`)
- [ ] Emulator: alarm OFF -> icon hides immediately (max 59 s)
- [ ] Emulator: long date strings (e.g., "September 28") do not overlap alarm icon
- [ ] Emulator: return from system menu -> quiet-time + alarm state refresh immediately

### Publish to Rebble App Store

#### 1. Prerequisites

- [ ] `pebble build` succeeds with no errors
- [ ] `package.json` `"version"` bumped to `<VERSION>`
- [ ] Changelog entry written in `README.md`

#### 2. Authentication (manual prerequisite)

Login must be done manually in an interactive terminal before the automated publish step:

```bash
pebble login --no-open-browser
```

If you are already logged in, the command exits immediately.  
If not, it displays a URL — open it in your browser to complete login.

> **Do not skip this.** The publish command below uses `--non-interactive` and will fail with an auth error if you are not already logged in.

#### 3. Publish Command

```bash
pebble publish \
  --non-interactive \
  --release-notes "<RELEASE_NOTES>" \
  --is-published \
  --screenshots <SCREENSHOT_FILES>
```

> **Tip**: Run `pebble publish --help` to see the full, up-to-date list of available flags and descriptions for your SDK version.

#### 4. Placeholders

| Placeholder          | What It Represents                                                                           |
| -------------------- | -------------------------------------------------------------------------------------------- |
| `<RELEASE_NOTES>`    | Release notes text shown in the app store listing                                            |
| `<SCREENSHOT_FILES>` | Screenshot paths. File names must start with the platform name, e.g., `emery_screenshot.png` |

#### 5. Full Example

```bash
pebble publish \
  --non-interactive \
  --release-notes "Added alarm clock indicator with red accent hands, refactored source into modular files." \
  --is-published \
  --screenshots screenshots/emery_screenshot_normal.png screenshots/emery_screenshot_bt.png screenshots/emery_screenshot_battery.png screenshots/emery_screenshot_quiet.png screenshots/emery_screenshot_alarm.png
```

#### 6. Notes

- **Draft vs. Published**: Omit `--is-published` to create a draft release you can review before making it public.
- **Non-interactive mode**: `--non-interactive` is required for `--screenshots` to be accepted without prompting. If you omit `--screenshots`, you can also omit `--non-interactive` to use the interactive screenshot source prompt.
- **Authentication**: `pebble publish` uses `--non-interactive`, so it assumes you are already logged in. If it fails with an auth error, return to step 2 and verify your login.
- **Flag Reference**: Use `pebble publish --help` for the latest available flags. The SDK may add or change options over time.

---

## 6. Changelog Conventions

- Use `## Changelog` section in `README.md`.
- Format: `### X.Y.Z - Description`.
- Prefix items with `Added`, `Fixed`, `Removed`, `Changed`.
- Link firmware context where relevant.

---

_This document should be updated whenever PebbleOS firmware changes affect Bluetooth or alarm behavior or when new architectural decisions are made._
