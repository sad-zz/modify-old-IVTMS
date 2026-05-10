# PSU Board — 12V → 5V

برد تغذیهٔ مرکزی پروژه. ورودی 12V (از باتری/آداپتور) و خروجی پایدار 5V/2A.

## ساختار
```
+12V_IN ──┬── F1 (1A fuse) ──┬── D1 (SS24, reverse-protect) ──┬── L1 (FB) ──┬── +12V_BUS (به backplane)
          │                  │                                │             │
          C1 (470µF/25V)     │                                │             C2 (100µF/16V)
          │                                                                 │
                                                                            ├── U1 (LM2596S-5.0) ──┬── L2 (33µH) ──┬── +5V
                                                                                                  │               │
                                                                                                 D2 (SS34)        C3 (220µF/10V) + C4 (10µF cer.)
```

## انتخاب قطعه
- **U1: LM2596S-5.0** (TO-263) — رگولاتور switching ساده 3A. fixed 5V version.
- **L2: 33µH 3A** — سیم‌پیچ shielded SMD (SRR1280-330M یا معادل)
- **D2: SS34** — شاتکی 3A 40V freewheel
- **F1: 1A polyfuse** (Bourns MF-MSMF010)
- **D1: SS24** — حفاظت reverse polarity
- **TVS: SMAJ24CA** روی +12V_IN (peak 24V clamp)
- **LED1: green LED + R1 1K** نشانگر خاموش/روشن
- **LED2: red LED + R2 1K** برای STATUS از MCU (پین 15 backplane)

## TestPoint
- TP1: +12V_IN
- TP2: +5V
- TP3: GND

## ابعاد PCB پیشنهادی
40 × 60 mm، 2-layer، plane GND زیر inductor.

## نکات تست
1. ابتدا بدون load: ولتاژ +5V باید 4.95-5.05V باشه
2. با load 1A (۵Ω/5W): افت < 50mV
3. ripple خروجی < 80mV peak-to-peak

## تست under-current limit
LM2596 internal current limit ~3A. اگر مدار خروجی short بشه، U1 ترموتال shutdown
می‌کنه (≥125°C). برای محافظت بهتر، می‌تونید resettable polyfuse 2A در خط +5V
اضافه کنید (F2 — اختیاری).
