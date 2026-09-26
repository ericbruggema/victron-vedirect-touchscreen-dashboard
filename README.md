# Victron VE.Direct touchscreen dashboard

A touchscreen dashboard for a Victron solar installation. It reads a **MPPT solar charger** and a **SmartShunt** directly over **VE.Direct** and shows solar power, battery state and consumption on a 2.8" colour touchscreen. It runs on an **ESP32-2432S028R** ("Cheap Yellow Display", CYD), a board that costs only a few euros.

- **Read-only.** It never writes to the MPPT or the SmartShunt.
- **No cloud, no WiFi, no clock module.** Everything runs locally on the board.
- **Touch to operate.** Swipe between pages and tap a card for details.
- **Demo mode.** Without any wiring, a built-in simulation shows the dashboard so you can try it first.

> Documentatie in het Nederlands: zie [INSTALLATIE.md](INSTALLATIE.md) (bouwen, bedrading, problemen oplossen).

## What you see

| Page | Content |
|---|---|
| **Dashboard** | Three cards: **Sun** (W, V/A, 24h peak), **Battery** (SoC %, V/A, time remaining), **Consumption** (W, A, 24h peak) |
| **Sun details** | Power, current, voltage, 24h yield, hourly PV graph |
| **Battery details** | SoC ring, voltage, current, remaining time, charge stage |
| **Energy** | Direct solar use, self-consumption, energy into/out of the battery, net balance |
| **Day & history** | Rolling 24h totals, min/max values, charge-stage split, 7-day yield graph |
| **Devices** | Model, firmware and settings read from the MPPT and SmartShunt |
| **System** | Uptime, free RAM, storage status, data source, firmware version |
| **Settings** | Brightness, screen-off after screensaver (yes/no), battery type, touch calibration, SD status |

Extras: energy-flow screensaver after 30 s without touch (shows PV, consumption and battery power), screen fully off after 5 minutes (optional), and an audible + on-screen alarm when the battery reaches a critical level.

"Day" statistics use a **rolling 24-hour window** since power-up, because the board has no real-time clock.

## Hardware

| Part | Notes |
|---|---|
| ESP32-2432S028R (CYD) | 2.8" 320x240 touchscreen, ESP32, backlight, speaker footprint, microSD slot |
| Victron MPPT | with a VE.Direct port (e.g. SmartSolar 100/30) |
| Victron SmartShunt | with a VE.Direct port |
| VE.Direct cables | 4-pin JST-PH 2.0: GND, TX, RX, +V |

> Tested with a board that has an **ST7789** display controller. Many guides assume ILI9341. See [Display driver](#display-driver) below.

## Wiring

VE.Direct is 3.3V serial. **Cross TX and RX**: the Victron **TX** goes to the ESP32 **RX**.

| Signal | ESP32 GPIO |
|---|---|
| MPPT TX  → ESP32 RX | **35** |
| SmartShunt TX → ESP32 RX | **22** |
| ESP32 TX → RX of both devices | **27** (a simple Y-split; only used to read device settings) |
| GND (all three) | **GND**, must be common |

Leave the **+V** pin of the VE.Direct connector unconnected. It is only meant for Victron accessories.

The boot screen shows these connections as a scrolling guide while it searches for data, so you can wire it up while it waits.

## Software setup

1. Install the **Arduino IDE** and the **ESP32 board package, core 3.x**.
2. In the Library Manager install **TFT_eSPI** (Bodmer) and **XPT2046_Touchscreen** (Paul Stoffregen).
3. Configure TFT_eSPI for this board (one time, see [Display driver](#display-driver)).
4. Open `VictronDashboardCYD.ino`, select the board (*ESP32 Dev Module* works), choose partition scheme **Huge APP** if the sketch is too big, and upload.
5. On first start, tap the two crosses to calibrate the touchscreen. This is stored and never asked again (you can redo it under Settings).

### Display driver

TFT_eSPI reads its pin and driver configuration from a file inside the library folder, not from the sketch. Copy `User_Setup_CYD.h` to `Documents\Arduino\libraries\TFT_eSPI\User_Setup.h` (keep the old file as a backup). Also make sure `User_Setup_Select.h` in that folder includes `User_Setup.h`.

Settings that matter on the tested board:

- `ST7789_DRIVER` (not ILI9341)
- `TFT_RGB_ORDER TFT_BGR` (red and blue were swapped with RGB)
- no colour inversion

If your screen stays white, shows the wrong colours or is mirrored, the driver or colour order is the first thing to check.

## Operating it

- **Swipe left/right** to go to the next/previous page.
- **Tap a card** on the dashboard for its details.
- **Tap the grid icon** (top right) to open Settings.
- **Tap the back arrow** (top left) to return to the dashboard.
- **Any touch** wakes the screen from the screensaver or from sleep.

## Configuration

Everything is in `config.h`: VE.Direct pins, battery type (LiFePO4 or lead-acid) and capacity, load colour thresholds, screensaver/screen-off timing, alarm sound, demo scenario and device labels. Comments in the file explain each option.

## Project layout

Single Arduino sketch; the `.ino` includes all headers.

| File | Purpose |
|---|---|
| `VictronDashboardCYD.ino` | Setup/loop, boot screen, screensaver, navigation, storage, alarm |
| `config.h` | All settings |
| `dashboard.h` | Shared types and declarations |
| `widgets.h` | Drawing widgets (cards, rows, gauges, graphs) with change-only redraw |
| `pages_*.h` | The pages |
| `touch.h` | XPT2046 touch, gestures, calibration |
| `ve_direct.h` | VE.Direct parser (text mode) and read-only HEX requests |
| `User_Setup_CYD.h` | TFT_eSPI configuration to install into the library |

Screen updates are **flicker-free**: every widget remembers what it drew and only repaints what actually changed.

## Status and limitations

- Developed and tested on one board; other CYD revisions may differ (display controller, touch pins).
- No WiFi, logging to the cloud or remote access, by design.
- Statistics are relative to power-up (rolling 24 h), not calendar days.
- The sketch could not be compiled in the environment where it was written; compile errors after a library update are most likely around `tft.drawArc()` or `tft.setViewport()`.

## Not affiliated with Victron

This is an independent hobby project. "Victron Energy", "VE.Direct", "MPPT" and "SmartShunt" are trademarks of their owners. Wire and use it at your own risk.

Made by Eric Bruggema. Free to use under the [MIT License](LICENSE).
