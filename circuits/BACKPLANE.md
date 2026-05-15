# Backplane / Inter-board connector

تمام بردها از طریق یک هدر استاندارد ۲×۸ (16 پین، 2.54mm) به هم وصل می‌شن. این هدر
"Backplane Bus" نام داره و روی هر برد به‌صورت `J_BUS` ارجاع می‌شه.

## پین‌اوت Backplane (J_BUS, 2x8 IDC)

| Pin | Net      | Direction (PSU view) | Notes |
|-----|----------|----------------------|-------|
| 1   | +12V_IN  | OUT                  | از PSU به سایر بردها (برای buckهای مودم) |
| 2   | +12V_IN  | OUT                  | (همان، برای جریان بالاتر) |
| 3   | GND      | —                    | |
| 4   | GND      | —                    | |
| 5   | +5V      | OUT                  | VCC اصلی (loop, dsPIC) |
| 6   | +5V      | OUT                  | |
| 7   | GND      | —                    | |
| 8   | MUX_A    | MCU→LOOP             | HCF4051 select line A |
| 9   | MUX_B    | MCU→LOOP             | HCF4051 select line B |
| 10  | MUX_C    | MCU→LOOP             | HCF4051 select line C |
| 11  | MUX_INH  | MCU→LOOP             | HCF4051 inhibit |
| 12  | LOOP_IO_COM_U51 | LOOP→MCU      | به ADC/COMP MCU |
| 13  | LOOP_IO_COM_U52 | LOOP→MCU      | به ADC/COMP MCU |
| 14  | CMP_OUT  | LOOP→MCU             | خروجی LM339 (open-collector) |
| 15  | LED_STATUS | MCU→PSU front      | LED روی برد PSU |
| 16  | RESERVED | —                    | برای آینده |

## نکات الکتریکی
- خط `+12V_IN` می‌تونه تا 2A بکشه (مودم AIR780E peak ≈1.6A)
- `+5V` فقط برای loop+dsPIC ➜ تا 500mA کافی است
- خط‌های `MUX_*` و `CMP_OUT` با pull-up 10K روی برد LOOP (روی برد MCU نباشه)
- جریان GND باید کم impedance باشه — هر دو پین GND به plane وصل بشن
