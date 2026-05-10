# IVTMS — Modular Board Set

این پوشه تجزیهٔ مدار قدیمی IVTMS به ۵ برد جدا (modular) و سه variant میکروکنترلر+مودم رو
شامل می‌شه. هدف: تمیزسازی شماتیک قدیمی (Altium `main.SchDoc`) و آماده‌سازی برای
ساخت هر تکه به صورت مستقل با KiCad 9 و در صورت نیاز تولید PCB.

## نقشهٔ بردها

| Folder | Board | Function |
|---|---|---|
| `00-power-supply/` | PSU | ورودی 12V ➜ 5V (VCC) برای مدارهای آنالوگ و دیجیتال 5V |
| `01-loop-detector/` | LOOP | همان مدار `main loop pcb3.SchDoc` (PDF ضمیمه) — 4 کاناله |
| `02-mcu-dspic-sim900/` | MCU-A | dsPIC30F4011 + SIM900/800 (legacy 2G) |
| `03-mcu-dspic-air780-w25q80/` | MCU-B | dsPIC30F4011 + AIR780E (4G Cat.1bis) + W25Q80 SPI Flash |
| `04-mcu-stm32-air780-w25q80/` | MCU-C | STM32F103C8 + AIR780E + W25Q80 |

> یادداشت تغذیه: مدار LOOP و dsPIC با ۵V کار می‌کنن. STM32F103 و W25Q80 و AIR780E
> منطق ۳.۳V هستن، پس روی بردهای MCU-B/MCU-C یک LDO 3.3V محلی اضافه شده. SIM900
> و AIR780E هستهٔ رادیویی ۳.۸–۴.۲V دارن که از یک buck مجزا روی برد MCU تأمین می‌شه
> (تغذیهٔ مرکزی فقط 5V می‌ده).

## ارتباط بین بردها (Inter-board)

تمام بردها از طریق یک هدر استاندارد ۸-پین به هم وصل می‌شن (پین‌اوت در `BACKPLANE.md`):

```
PSU ──+12V/+5V/GND──┬─► LOOP
                    ├─► MCU-A | MCU-B | MCU-C
                    
LOOP ◄──MUX_SEL[A,B,C], CMP_OUT, INH──► MCU
```

## فایل‌های هر برد

```
NN-board/
  ├── schematic/board.kicad_sch  ← KiCad 9 schematic
  ├── schematic/board.kicad_pro  ← project
  ├── pcb/board.kicad_pcb        ← PCB (empty/placeholder, populate via "Update PCB from schematic")
  ├── docs/BOM.csv               ← لیست قطعات با مقدار/قاب/شماره
  ├── docs/NETLIST.md            ← جدول کامل اتصالات (human-readable)
  └── docs/README.md             ← شرح طبقات و طراحی برد
```

## تحلیل مدار اصلی LOOP (از PDF)

مدار `main loop pcb3.SchDoc` یک **interface ۴ کاناله برای حلقه‌های القایی تشخیص خودرو** هست
(inductive vehicle detection loops). برای هر یک از ۴ کانال (subscript 11,12,13,14):

1. **ورودی حلقه** از هدر `P1` (4 کانال × 2 = 8 پین)
2. **فیلتر ضد نویز RC**: 47nF + ۰Ω سری (`C##B`,`R##A`)
3. **شبکهٔ حفاظت ESD/spike**: سه ترانزیستور 2N7000 (`Q##A/B/C`) به صورت کلامپ
4. **دیود کلامپ شاتکی** 1N5819 (`D##A`)
5. **مقایسه‌گر LM339** (یک ربع از `U50`):
   - تقسیم‌کننده ولتاژ آستانه: 330K (pull-up) + 10K + 3.3K
   - مقاومت سری ورودی منفی 1K
6. **سوییچ بار P-Channel** 2SJ196/BSS92 (`Q##D`) با gate از خروجی LM339
7. **خروجی** پس از سوییچ به هدر `pl`

دو IC مالتی‌پلکسر **HCF4051BEY** (`U51`,`U52` — ۸-به-۱ آنالوگ) با خطوط آدرس A,B,C
و INH از طرف MCU جهت اسکن ترتیبی کانال‌ها استفاده می‌شن. خط `IO COM` به ورودی LM339
یا مستقیم به ADC/digital MCU می‌ره (بستگی به مد).

این تحلیل عیناً به برد `01-loop-detector` منتقل شده (بدون تغییر منطقی) — فقط
اتصال‌ها از طریق هدر backplane به جای hard-wire به MCU.

## variantها — تفاوت‌های کلیدی

### MCU-A: dsPIC30F4011 + SIM900/SIM800
- legacy 2G (در حال خاموش شدن در بسیاری کشورها) — برای retrofit موجود نگه‌داشته‌ شده
- UART2 ➜ SIM900 با level-shift (5V dsPIC ↔ 2.8V SIM)
- بدون SPI flash (داده محلی روی EEPROM داخلی dsPIC یا SD)
- تغذیهٔ SIM900 از buck محلی 12V➜4.0V (TPS5430 یا MP1584)

### MCU-B: dsPIC30F4011 + AIR780E + W25Q80
- 4G Cat.1bis (LTE) — جایگزین مدرن SIM900
- UART2 ➜ AIR780E با level-shift 5V↔1.8V/3.3V
- SPI1 ➜ W25Q80 (1MB flash) برای ذخیرهٔ لاگ تردد
- LDO 3.3V محلی (AMS1117) برای W25Q80 و سطح‌های منطقی AIR780E
- buck 12V➜4.0V برای رادیوی AIR780E

### MCU-C: STM32F103C8 + AIR780E + W25Q80
- ۳.۳V کاملاً native — بدون level-shift نیاز
- STM32 با ۲ SPI و ۳ UART — راحتی توسعه
- USB device (debug/firmware update)
- منبع بوت SWD + USB DFU

## نقشهٔ راه ساخت
1. اول `00-power-supply` رو بسازید/تست کنید (5V خروجی پایدار)
2. `01-loop-detector` رو با سیگنال تست + MCU روی breadboard وریفای کنید
3. یکی از ۳ variant MCU رو انتخاب و assemble کنید
4. firmware موجود (`91-7.c` برای dsPIC، `stm32-firmware/` برای STM32) با تغییرات pin-mapping استفاده می‌شه

## Codex/Claude continuation
این پروژه با هدف ادامهٔ توسعه توسط Codex/Claude نوشته شده. فایل‌های مرجع:
- `circuits/CODEX_BRIEF.md` — خلاصهٔ design intent برای هر LLM coding agent
- هر `NETLIST.md` — منبع حقیقت اتصالات؛ شماتیک‌های KiCad از این تولید/بازتولید می‌شن
