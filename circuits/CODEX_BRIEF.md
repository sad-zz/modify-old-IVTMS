# Codex / Claude Continuation Brief

این فایل برای ادامهٔ توسعهٔ پروژه توسط agentهای کدنویس (Codex CLI، Claude Code، و
غیره) نوشته شده. نقطهٔ شروع: تجزیهٔ مدار قدیمی IVTMS به ۵ برد جدا.

## What's done
- ✅ تحلیل کامل مدار اصلی LOOP (از `main loop pcb3.SchDoc` PDF)
- ✅ ساختار پروژه برای ۵ برد جدا
- ✅ NETLIST.md به فرم جدول قابل خواندن (انسانی) برای هر برد — منبع حقیقت اتصالات
- ✅ BOM.csv برای هر برد
- ✅ KiCad 9 schematic skeleton (`.kicad_sch`) و project (`.kicad_pro`) برای هر برد
- ✅ تخصیص پین MCU برای ۳ variant
- ✅ Backplane bus pinout

## What's NOT done (intentional — leave for designer or next agent)
- 🟡 جانمایی فیزیکی PCB (`.kicad_pcb`) — فایل خالی است؛ از KiCad
  "Tools → Update PCB from Schematic" استفاده کنید
- 🟡 footprintهای دقیق (در فایل KiCad فقط `Footprint` خام تعیین شده، باید با
  KiCad library matcher تطبیق داده بشه)
- 🟡 SPICE simulation/verification
- 🟡 مهر و موم EMI/EMC، testpoint، fiducial

## Continuation prompts

اگر می‌خواید ادامه بدید، این promptها رو به Codex/Claude بدید:

### Prompt 1 — کامل‌سازی footprintها
```
For every component in circuits/*/schematic/*.kicad_sch, ensure the Footprint
property is set to a valid KiCad 9 library reference (e.g., "Resistor_SMD:R_0805_2012Metric").
Use SMD 0805 for passives by default, SOT-23 for small transistors, SOIC for
LM339, TSSOP-44 for dsPIC30F4011, LQFP-48 for STM32F103C8.
```

### Prompt 2 — تولید PCB از schematic
```
For each board in circuits/, run:
  kicad-cli pcb new --from-schematic NN-board/schematic/board.kicad_sch \
                    --out NN-board/pcb/board.kicad_pcb
Then add board outline (50x60mm), 4 mounting holes (M3) at corners with 3mm
inset, and place J_BUS connector at center-edge.
```

### Prompt 3 — وریفای الکتریکی
```
Run ERC on each schematic with:
  kicad-cli sch erc circuits/*/schematic/*.kicad_sch
Fix any unconnected nets, missing power flags, or bus conflicts. Ensure
PWR_FLAG symbols are placed on +12V_IN, +5V, +3V3, +4V0_RF, GND.
```

## Design constraints (don't violate)
1. مدار LOOP **عیناً** از PDF بازسازی شده — ولتاژ آستانه و مقاومت‌ها رو تغییر ندید
2. پین‌اوت backplane در `BACKPLANE.md` ثابته — اگر بردی پین می‌خواد، RESERVED (pin 16) رو استفاده کنید
3. هر برد باید مستقل کار کنه (مثلاً اگر MCU برداشته شد، PSU خراب نشه)
4. مدلهای 2N7000، 2SJ196، LM339، HCF4051، dsPIC30F4011 رو با معادل تجاری عوض **نکنید**
   مگر صریحاً درخواست بشه — این قطعات تأیید شدن و firmware موجود به اونها بسته‌ست

## Reference files
- `91-7.c` — firmware موجود برای dsPIC30F4011 (در ریشهٔ repo)
- `stm32-firmware/` — port STM32 (در ریشهٔ repo)
- `Variables.h`, `GPRS.h`, `MMC.h` — هدرهایی که pin assignment رو تعیین می‌کنن
- `main.SchDoc` — شماتیک Altium اصلی (مرجع تاریخی)
