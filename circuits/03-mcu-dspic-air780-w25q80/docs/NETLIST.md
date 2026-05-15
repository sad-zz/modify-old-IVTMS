# MCU-B (dsPIC + AIR780E + W25Q80) — Netlist

## Power tree
```
+12V_BUS (J_BUS.1,2) ──┬── U2 (MP1584, set 3.8V) ── +V_BAT_RF (به AIR780E Vbat)
                       │       └─ C 470µF/16V + 100nF
                       └── (نه چیز دیگه)

+5V_BUS (J_BUS.5,6)  ──┬── U1 (dsPIC30F4011) VDD
                       ├── U3 (DS1305) Vcc
                       └── U_LDO33 (AMS1117-3.3) ── +3V3
                                                     ├── U4 (W25Q80) Vcc
                                                     ├── U5/U6 (level shifter VccB side)
                                                     └── pull-ups روی خط‌های 3.3V
```

## AIR780E module connections
| AIR780E pad | Net            | Notes                         |
|-------------|----------------|-------------------------------|
| Vbat        | +V_BAT_RF      | bulk 470µF + 100nF; 4 پاد در کنار |
| GND         | GND            | تمام پدها وصل                 |
| PWRKEY      | AIR_PWRKEY     | از RB7 dsPIC با pulse 2s فعال |
| RESET_N     | AIR_RESET      | از RD2 dsPIC، active low      |
| UART_TXD    | به RX_LVL_3V3 | به سطح‌ساز (1.8V LVL out side) |
| UART_RXD    | از TX_LVL_3V3 | از سطح‌ساز                    |
| STATUS      | MDM_STAT       | به RB6 dsPIC با pull-up       |
| NETLIGHT    | NETLIGHT_LED  | LED 4G + R 1K                 |
| USIM_VDD/CLK/DATA/RST | SIM holder + SP0503 ESD |              |
| ANT_MAIN    | به U.FL/SMA   | 50Ω matching                  |
| ANT_DIV     | اختیاری       | برای LTE diversity (Cat.1bis اختیاری) |
| ADC1, ADC2  | nc             |                               |

## W25Q80 (SOIC-8)
| Pin | Name | Net           |
|-----|------|---------------|
| 1   | /CS  | FLASH_CS (RD1 با level shifter یا direct اگر VIH dsPIC در 3.3V پذیرفته بشه) |
| 2   | DO   | به SDI1 dsPIC با level-shifter UP (3V3→5V) |
| 3   | /WP  | +3V3 (write protect غیرفعال) |
| 4   | GND  | GND            |
| 5   | DI   | از SDO1 dsPIC با level-shifter DOWN (5V→3V3) |
| 6   | CLK  | از SCK1 dsPIC با level-shifter DOWN |
| 7   | /HOLD| +3V3           |
| 8   | Vcc  | +3V3           |

> توصیه: استفاده از `74LVC125` با Vcc=3V3 برای 4 خط CS/MOSI/SCK (هر 4 unidirectional 5→3V3 OK چون LVC ورودی 5V tolerant)
> برای MISO (3V3→5V) از یک buffer دیگه با Vcc=5V (یا فقط BSS138 یا حتی direct وقتی 3.3V≥VIH dsPIC=2.0V معمولاً کار می‌کنه)

## Level shifter UART (dsPIC 5V ↔ AIR780E 1.8V)
استفاده از `TXS0108EPWR` (8-bit bidirectional auto-direction):
- VccA = 5V (سمت dsPIC)
- VccB = 1.8V (سمت AIR — اگر AIR780E خط 1.8V خروجی داره)
- یا ساده‌تر: 4× `SN74LVC1T45DBVR` با Vcca=5V, Vccb=1.8V

اگر AIR780E با IO 3.3V کار می‌کنه (بسیاری پروژه‌ها 3.3V):
- VccB = +3V3
- ساده‌تر و سازگارتر

## DS1305 RTC (همان MCU-A)
وصل با crystal 32.768kHz، CR2032 backup، CE = RTC_CS (RF0).

## Crystals
- Y1: 16 MHz dsPIC + 22pF×2
- Y2: 32.768 kHz DS1305 + 6pF×2

## ICSP header (J_ICSP, 5-pin)
| 1 | MCLR | | 2 | +5V | | 3 | GND | | 4 | PGD | | 5 | PGC |

## J_BUS (همان BACKPLANE.md)
بدون تغییر نسبت به MCU-A.

## ESD/protection
- TVS روی +12V_BUS ورودی (SMAJ24CA)
- ESD روی SIM lines (SP0503BAHTG)
- ESD روی USB (اختیاری اگر USB-CDC از طریق dsPIC یا AIR780E added)
