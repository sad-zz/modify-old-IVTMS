# MCU-C (STM32F103C8 + AIR780E + W25Q80) — Netlist

## Power tree
```
+12V_BUS (J_BUS.1,2) ──┬── U2 (MP1584, 3.8V) ── +V_BAT_RF ── به AIR780E Vbat (×4)
                       │       └─ C 470µF/16V + 100nF
                       └── R_DIV (R1+R2 1:11) ── ADC PA0 (VBAT sense)

+5V_BUS (J_BUS.5,6)  ──┬── U_LDO33 (AMS1117-3.3) ── +3V3
                       │     ├── STM32F103 VDD (×4) + VBAT
                       │     ├── W25Q80 Vcc
                       │     ├── microSD Vdd
                       │     └── AIR780E IO side
                       │
                       └── R_DIV (R3+R4 11:1) ── ADC PA1 (SOLAR sense, اختیاری)

USB Vbus ──D_USB (SS14)── parallel to +5V_BUS (با switch/diode-OR)
```

## STM32F103C8T6 پین‌ها (LQFP-48)

| Pin | Name      | Net (per firmware) |
|-----|-----------|---------------------|
| 1   | VBAT      | +3V3 (یا CR2032 + diode برای RTC backup) |
| 2   | PC13      | LED_HEARTBEAT (active-low LED + R 1K) |
| 3   | PC14      | RTC_OSC_IN  → Y2.1 |
| 4   | PC15      | RTC_OSC_OUT → Y2.2 |
| 5   | PD0/OSC_IN | HSE_IN  → Y1.1 |
| 6   | PD1/OSC_OUT| HSE_OUT → Y1.2 |
| 7   | NRST      | reset (10K pullup, 100nF to GND, button to GND) |
| 8   | VSSA      | GND |
| 9   | VDDA      | +3V3 (با FB + 100nF + 10nF) |
| 10  | PA0       | VBAT_SENSE |
| 11  | PA1       | SOLAR_SENSE |
| 12  | PA2       | LOOP_IN (TIM2_CH3) ← J_BUS.14 (CMP_OUT) |
| 13  | PA3       | spare |
| 14  | PA4       | FLASH_CS |
| 15  | PA5       | FLASH_SCK → W25Q80.6 |
| 16  | PA6       | FLASH_MISO ← W25Q80.2 |
| 17  | PA7       | FLASH_MOSI → W25Q80.5 |
| 18  | PB0       | MUX_A → J_BUS.8 |
| 19  | PB1       | MUX_B → J_BUS.9 |
| 20  | PB2/BOOT1 | FLASH_WP → W25Q80.3 (و BOOT1=0 برای flash) |
| 21  | PB10      | AIR780_RXD ← USART3_TX = AIR780E.RXD |
| 22  | PB11      | AIR780_TXD → USART3_RX از AIR780E.TXD |
| 23  | VSS_1     | GND |
| 24  | VDD_1     | +3V3 |
| 25  | PB12      | SD_CS |
| 26  | PB13      | SD_SCK |
| 27  | PB14      | SD_MISO |
| 28  | PB15      | SD_MOSI |
| 29  | PA8       | AIR_PWRKEY |
| 30  | PA9       | DEBUG_TX (USART1) |
| 31  | PA10      | DEBUG_RX (USART1) |
| 32  | PA11      | AIR_STATUS / USB_DM (mux) — اگر USB استفاده می‌شه، pin USB دارد priority |
| 33  | PA12      | USB_DP |
| 34  | PA13      | SWDIO |
| 35  | VSS_2     | GND |
| 36  | VDD_2     | +3V3 |
| 37  | PA14      | SWCLK |
| 38  | PA15      | spare |
| 39  | PB3       | FLASH_HOLD → W25Q80.7 |
| 40  | PB4       | MUX_C → J_BUS.10 (extension) |
| 41  | PB5       | LED_ONLOOP0 |
| 42  | PB6       | LED_ONLOOP1 |
| 43  | PB7       | LED_ONLOOP2 |
| 44  | BOOT0     | DFU select (jumper to GND or 3V3) |
| 45  | PB8       | LED_ONLOOP3 |
| 46  | PB9       | LED_CONN_STATE → J_BUS.15 (LED_STATUS) |
| 47  | VSS_3     | GND |
| 48  | VDD_3     | +3V3 |

> یادداشت: STM32F103C8 USB روی PA11/PA12. اگر USB می‌خوای، AIR_STATUS رو ببر روی PB12 یا PB14 (تداخل با SD نه!)؛ یا از pin PA11 برای STATUS با mux استفاده کن. در firmware موجود، PA11=AIR_STATUS بدون USB. اگر USB لازمه، PA11 و PA12 آزاد بشه و STATUS بره روی pin دیگه (پیشنهاد: PB4 → MUX_C رو روی PA15 ببر)

## AIR780E module connections
| AIR780E pad | Net          |
|-------------|--------------|
| Vbat (×4)   | +V_BAT_RF (470µF + 4×100nF) |
| GND         | GND          |
| PWRKEY      | AIR_PWRKEY (PA8) |
| RESET_N     | tied to RC pull-up + spare GPIO (PA15 یا nc) |
| UART_TXD    | PB11 (RX از منظر STM32) |
| UART_RXD    | PB10 (TX از منظر STM32) |
| STATUS      | AIR_STATUS (PA11) |
| NETLIGHT    | LED 4G + R 1K |
| USIM_*      | SIM holder + SP0503 |
| ANT_MAIN    | U.FL با 50Ω |

## W25Q80 (SPI1, +3V3)
بدون level shifter (همه 3V3).
| Pin | Net          |
|-----|--------------|
| 1 /CS  | PA4       |
| 2 DO   | PA6 (MISO)|
| 3 /WP  | PB2       |
| 4 GND  | GND       |
| 5 DI   | PA7 (MOSI)|
| 6 CLK  | PA5       |
| 7 /HOLD| PB3       |
| 8 Vcc  | +3V3 (100nF) |

## microSD socket (SPI2)
| SD pin | Net          |
|--------|--------------|
| 1 CS   | PB12         |
| 2 DI   | PB15 (MOSI)  |
| 3 Vss  | GND          |
| 4 Vdd  | +3V3 (10µF + 100nF) |
| 5 SCK  | PB13         |
| 6 Vss  | GND          |
| 7 DO   | PB14 (MISO)  |
| 8/9    | nc (RSV)     |

## SWD header (J_SWD, 4-pin)
| Pin | Net   |
|-----|-------|
| 1   | +3V3  |
| 2   | SWDIO (PA13) |
| 3   | SWCLK (PA14) |
| 4   | GND   |

## USB-C (اختیاری)
- USB-C 6-pin SMD (e.g., GCT USB4500)
- VBUS → SS14 → بسیار مفید: قابلیت تغذیه با USB
- D+/D- → PA12/PA11 (با مقاومت سری 22Ω، اختیاری)
- CC1/CC2 → 5.1K pull-down به GND (UFP طبق USB-C spec)

## Crystals
- Y1: 8 MHz HSE → PD0/PD1 (به جای OSC_IN/OUT)، 18pF×2
- Y2: 32.768 kHz LSE → PC14/PC15، 6pF×2

## RC reset
- C_RST 100nF + R_RST 10K pullup روی NRST + tactile switch به GND

## Decoupling
- 100nF نزدیک هر VDD (×4 + VDDA + VBAT)
- 4.7µF + 100nF روی +3V3 (LDO output)
- 10µF + 100nF نزدیک W25Q80 + SD
- 470µF + 4×100nF نزدیک AIR780E Vbat

## J_BUS pinout (همان BACKPLANE)
| Pin | Net          | STM32 pin |
|-----|--------------|-----------|
| 1,2 | +12V_BUS     | (به buck) |
| 3,4 | GND          | GND       |
| 5,6 | +5V          | (به LDO 3V3) |
| 7   | GND          | GND       |
| 8   | MUX_A        | PB0       |
| 9   | MUX_B        | PB1       |
| 10  | MUX_C        | PB4       |
| 11  | MUX_INH      | PA15 (یا spare) |
| 12  | LOOP_IO_COM_U51 | PA2 (TIM2_CH3) |
| 13  | LOOP_IO_COM_U52 | PA3       |
| 14  | CMP_OUT      | (موازی PA2) |
| 15  | LED_STATUS   | PB9       |
| 16  | RESERVED     | nc        |
