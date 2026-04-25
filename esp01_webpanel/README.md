# IVTMS ESP-01 WiFi Configuration Panel

An ESP-01 (ESP8266) module firmware that replaces the USB-Serial cable on the
IVTMS device's **P2** connector and provides a **web-based configuration panel**
over a local WiFi access-point.

No changes to the dsPIC firmware are required — the ESP-01 simply bridges the
existing UART1 command interface to a web browser.

---

## How it works

```
Operator's phone / PC
        │  WiFi (192.168.4.x)
        ▼
  ESP-01  ←── HTTP GET /cmd?c=0088 ──
        │       responds with plain-text
  UART  │  115200 baud, CR-terminated
        ▼
  dsPIC30F4013 (P2 / UART1)
```

The ESP-01 creates a WiFi access-point.  
The operator connects and opens `http://192.168.4.1` in any browser.  
Commands typed or selected in the web panel are forwarded over UART to the dsPIC
and the response is displayed in the browser.

---

## Hardware requirements

| Item | Notes |
|------|-------|
| ESP-01 (ESP8266) module | 1 MB flash version |
| 3.3 V power supply / regulator | **ESP-01 is 3.3 V only** – do not connect to 5 V |
| 4-pin header on P2 | Already present on the IVTMS board |

---

## Wiring (P2 connector)

The pin numbers refer to the P2 female connector viewed from the top of the PCB.
Confirm against the board schematic before connecting.

```
P2 pin  Signal         Connect to
──────────────────────────────────
  1     VCC 3.3 V   →  ESP-01 VCC (3.3 V) + EN
  2     GND          →  ESP-01 GND
  3     dsPIC TX     →  ESP-01 RX (GPIO3)
  4     dsPIC RX     →  ESP-01 TX (GPIO1)
```

**Additional pull-ups required for reliable boot:**

| ESP-01 pin | Connect to |
|-----------|-----------|
| GPIO0     | 3.3 V via 10 kΩ (HIGH = normal boot, not flash mode) |
| GPIO2     | 3.3 V via 10 kΩ (required for boot) |
| RST       | 3.3 V via 10 kΩ (or RC reset: 100 nF + 10 kΩ to GND) |

> **Note:** if the IVTMS board already provides a 3.3 V rail, use it.
> Ensure the regulator can supply at least 250 mA for the ESP-01 transmit peaks.

---

## Flashing the ESP-01

### One-time environment setup

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Open **File → Preferences** and add the ESP8266 board URL to *Additional boards manager URLs*:
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search **esp8266**, install the package.

### Build settings

| Setting | Value |
|---------|-------|
| Board | Generic ESP8266 Module |
| Flash Size | 1MB (FS: none) |
| Crystal Frequency | 26 MHz |
| CPU Frequency | 80 MHz |
| Upload Speed | 115200 |

### Enter flash mode

Temporarily wire **GPIO0 to GND** and **power cycle** the ESP-01 before uploading.
Remove the GPIO0-to-GND wire after the flash completes and power cycle again.

A USB-to-UART adapter (3.3 V logic) connected to the ESP-01 TX/RX is needed for
flashing if the IVTMS board is not in circuit.

---

## Using the web panel

1. Power on the IVTMS board.
2. On a phone or laptop, connect to WiFi network **IVTMS-Config** (password: `ivtms1234`).
3. Open **http://192.168.4.1** — or try **http://ivtms.local** on most OS.
4. The panel loads. Use the tabs to access all configuration sections.

> **Security note:** the WiFi AP is only active while the IVTMS board is powered.
> Change `AP_PASS` in the sketch to a strong password for field deployments.

---

## Tab reference

| Tab | Commands used |
|-----|--------------|
| **Status** | `0000` status · `0088` full settings · `0018` GSM signal · `0007` frequencies · `0046` vehicle counts |
| **Settings** | `0002` loops · `0003` H-device · `0004` loop distance · `0005` loop width · `0010` margins · `0011` HMM · `0021` power type |
| **Network** | `0032` APN · `0034` server IP/port · `0035` MQTT broker · `0036` MQTT enable |
| **Identity** | `0012` date/time · `0022` SMS number · `0023` location name |
| **Class/Speed** | `0040`–`0045` class limits · `0019` speed limits |
| **Advanced** | `0006` recalibrate · `0008` debug · `0013` reset · `0017` GSM restart · `0025` GSM debug |
| **Terminal** | Raw command input — any 4-digit code + parameters |

---

## Command format summary

| Code | Format | Description |
|------|--------|-------------|
| `0000` | — | Query status |
| `0002` | `0002XXXX` | Loop enable (X=0/1 per loop) |
| `0003` | `0003X` | H-device mode (0/1) |
| `0004` | `0004DDD` | Loop distance cm (3 digits) |
| `0005` | `0005WWW` | Loop width cm (3 digits) |
| `0006` | — | Recalibrate all loops |
| `0007` | — | Show calibration frequencies |
| `0008` | — | Toggle serial debug |
| `0010` | `0010TTBB` | Margins (top TT, bottom BB, 2 digits each) |
| `0011` | `0011HH` | HMM value (≥10, 2 digits) |
| `0012` | `0012YYMMDDHHmmSS` | Set date & time (2 digits each field) |
| `0013` | — | Reset MCU |
| `0017` | — | Restart GSM/SIM900 |
| `0018` | — | GSM signal quality |
| `0019` | `0019D1D1D1N1N1N1D2D2D2N2N2N2` | Speed limits (3 digits each: Day/Night × Lane 1/2) |
| `0021` | `0021X` | Power type (0=Solar, 1=Night, 2=Backup, 3=None) |
| `0022` | `002209XXXXXXXXX` | SMS number (11 digits) |
| `0023` | `0023<32 chars>` | Location name (pad to 32 chars with spaces) |
| `0025` | — | Toggle GSM debug |
| `0032` | `0032X` | APN (0=MTN IranCell, 1=MCI) |
| `0034` | `0034AAA.BBB.CCC.DDD,PPPPP` | Data server IP + port (octets zero-padded to 3 digits, port to 5 digits) |
| `0035` | `0035AAA.BBB.CCC.DDD,PPPPP` | MQTT broker IP + port |
| `0036` | `0036X` | MQTT publishing (0=off, 1=on) |
| `0040`–`0045` | `00XXNNNN` | Class X/A/B/C/D/E length limit (4 digits) |
| `0046` | — | Vehicle count totals |
| `0088` | — | Full settings dump |

All commands receive a response ending with `\r\nOK\r\n`.

---

## Customisation

Edit the constants at the top of `esp01_webpanel.ino`:

```cpp
static const char AP_SSID[] = "IVTMS-Config";   // WiFi network name
static const char AP_PASS[] = "ivtms1234";       // password (min 8 chars)
```

Set `AP_PASS` to `""` for an open (passwordless) access-point.
