# MCU-A (dsPIC30F4011 + SIM900) — Netlist

## Power tree
```
+12V_BUS (J_BUS.1,2) ──┬── U2 (MP1584, set 4.0V) ──┬── +4V0_RF (مودم Vbat)
                       │                            └── C_BULK 470µF/16V
                       └── (سایر مصرف ندارد روی این برد)

+5V_BUS (J_BUS.5,6)  ──┬── U1 (dsPIC30F4011) VDD
                       ├── U3 (DS1305) Vcc
                       ├── SD socket Vcc
                       └── سطح‌سازی‌ها (MAX_5V_HIGH side)

+3V3 (مشتق از +5V با LDO کوچک AMS1117-3.3 — اختیاری برای SD)
```

## Crystal
- Y1: 16 MHz / 22pF×2 → OSC1, OSC2 (پین 30, 31 PDIP-40 / 13, 14 TQFP-44)
- Y2: 32.768 kHz → DS1305 RTC

## SIM900 connections
| SIM900 pin | Net          | Notes |
|------------|--------------|-------|
| Vbat (×4)  | +4V0_RF      | bulk cap 470µF + 100nF نزدیک پین |
| GND (×6)   | GND          | تمام پین‌های GND باید وصل بشن |
| PWRKEY     | PWRKEY       | از dsPIC RB7، با pull-up 10K + pulse 1s |
| RXD        | از TX_LVL    | از سطح‌ساز (5V→2.8V) |
| TXD        | به RX_LVL    | به سطح‌ساز |
| STATUS     | MDM_STAT     | به dsPIC RB6 (با pull-up) |
| NETLIGHT   | NETLIGHT_LED | LED + R 1K |
| RI         | optional     | wakeup MCU |
| SIM_VDD/CLK/RST/IO/GND | SIM holder + ESD | TVS lines |
| MIC_P/N, SPK_P/N | اختیاری | اگر voice لازم نیست، باز |
| ANT        | به U.FL/SMA  | 50Ω track + matching |

## Level shifter (5V dsPIC → 2.8V SIM900)
استفاده از `SN74LVC1T45DBVR` (دو عدد، یکی برای TX، یکی برای RX) یا روش ساده‌تر
با Q (2N7000) + 2 مقاومت:

```
5V_TX ──(10K)── G(2N7000) ; D ──pull-up 10K── 2.8V ; S ── GND
out_low_side = D = SIM_RXD
```

## SD socket (SPI mode)
| SD pin | Net  |
|--------|------|
| 1 (CS) | MMC_CS (RF1) |
| 2 (DI) | SDO1 (MOSI)  |
| 3 (Vss)| GND  |
| 4 (Vdd)| +3V3 |
| 5 (Clk)| SCK1 |
| 6 (Vss)| GND  |
| 7 (DO) | SDI1 (MISO)  |
| 8 (RSV)| nc   |
| 9 (RSV)| nc   |
| level shift اگر +3V3 SD از +5V dsPIC: 4-wire (CS, MOSI, SCK, MISO) با LVC125 یا مقسم مقاومتی |

## DS1305 RTC
| DS1305 | Net     |
|--------|---------|
| Vcc    | +5V     |
| Vbat   | CR2032 + diode |
| GND    | GND     |
| X1, X2 | 32.768 kHz crystal + 6pF×2 |
| SCLK   | SCK1    |
| SDI    | SDO1    |
| SDO    | SDI1    |
| CE     | RTC_CS (RF0) |
| INT0/1 | nc یا به CN MCU |

## J_BUS (backplane) — pinout (همان BACKPLANE.md)
| Pin | Net           |
|-----|---------------|
| 1,2 | +12V_BUS      |
| 3,4 | GND           |
| 5,6 | +5V           |
| 7   | GND           |
| 8   | MUX_A (RC13)  |
| 9   | MUX_B (RC14)  |
| 10  | MUX_C (RC15)  |
| 11  | MUX_INH (RD0) |
| 12  | LOOP_IO_COM_U51 (AN0) |
| 13  | LOOP_IO_COM_U52 (AN1) |
| 14  | CMP_OUT (AN2) |
| 15  | LED_STATUS (RE4) |
| 16  | RESERVED      |

## ICSP header (J_ICSP, 5-pin)
| Pin | Net   |
|-----|-------|
| 1   | MCLR  |
| 2   | +5V   |
| 3   | GND   |
| 4   | PGD   |
| 5   | PGC   |

## Decoupling (اجباری)
- 100nF نزدیک هر VDD pin dsPIC (4 عدد VDD/AVDD)
- 100nF نزدیک Vbat پین SIM900 + 470µF tantal یا electrolytic
- 100nF نزدیک DS1305 و SD
- 10µF cer روی +5V_BUS ورودی + 10µF cer روی +4V0_RF
